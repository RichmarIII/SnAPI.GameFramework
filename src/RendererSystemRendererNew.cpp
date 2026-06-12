#include "RendererSystem.h"

#if defined(SNAPI_GF_ENABLE_RENDERER_NEW)

#include "Profiling.h"
#include "Renderer.h"
#include "Platform/Linux/XlibWindow.h"

#include <UIContext.h>
#include <UIRenderPackets.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

namespace SnAPI::GameFramework
{
namespace
{
[[nodiscard]] static float ClampWindowExtent(const float Value)
{
    return std::max(1.0f, Value);
}

[[nodiscard]] static std::uint32_t ResolveExtent(const std::uint32_t Requested, const float OutputExtent)
{
    if (Requested > 0u)
    {
        return Requested;
    }

    return static_cast<std::uint32_t>(std::max(1.0f, std::round(OutputExtent)));
}

[[nodiscard]] static std::uint32_t ResolveWindowPixelExtent(const float Value)
{
    return static_cast<std::uint32_t>(std::max(1.0f, std::round(ClampWindowExtent(Value))));
}

[[nodiscard]] static SnAPI::Renderer::FrameTimingStamp MakeRendererTimingStamp(
    const std::chrono::steady_clock::time_point Time)
{
    return SnAPI::Renderer::FrameTimingStamp{
        .Valid = true,
        .MonotonicNanoseconds =
            std::chrono::duration_cast<std::chrono::nanoseconds>(Time.time_since_epoch()).count()};
}

[[nodiscard]] static SnAPI::Renderer::FramePipelineRuntimeSettings BuildRendererNewFramePipelineSettings(
    const SnAPI::Renderer::ResolvedRenderViewExtents& ViewExtents,
    const SnAPI::Renderer::SurfacePresentationProfile& PresentationProfile)
{
    SnAPI::Renderer::FramePipelineRuntimeSettings Settings{};
    Settings.ViewExtents = ViewExtents;
    Settings.PresentationProfile = PresentationProfile;
    Settings.EnableFsrUpscaling = false;
    Settings.EnableAtmosphereAerialPerspective = false;
    Settings.EnableAutoExposure = false;
    Settings.ProbeLightingEnabled = false;
    Settings.ForceSkyProbeRefresh = false;
    Settings.LocalProbeLightingEnabled = false;
    Settings.GlobalIllumination = SnAPI::Renderer::GlobalIlluminationRuntimeSettings{};
    return Settings;
}

[[nodiscard]] static bool RendererNewViewExtentsEqual(
    const SnAPI::Renderer::ResolvedRenderViewExtents& Left,
    const SnAPI::Renderer::ResolvedRenderViewExtents& Right)
{
    return Left.PlatformPresentationExtent.Width == Right.PlatformPresentationExtent.Width &&
           Left.PlatformPresentationExtent.Height == Right.PlatformPresentationExtent.Height &&
           Left.OutputExtent.Width == Right.OutputExtent.Width &&
           Left.OutputExtent.Height == Right.OutputExtent.Height &&
           Left.InternalRenderExtent.Width == Right.InternalRenderExtent.Width &&
           Left.InternalRenderExtent.Height == Right.InternalRenderExtent.Height;
}

[[nodiscard]] static bool ShouldLogRendererNewUiBridge() noexcept
{
    const char* value = std::getenv("SNAPI_GF_RENDERER_NEW_LOG_UI");
    return value != nullptr && value[0] != '\0' && value[0] != '0';
}

#if defined(SNAPI_GF_ENABLE_UI)
[[nodiscard]] static SnAPI::Renderer::UiColor ToRendererUiColor(const SnAPI::UI::Color& Color)
{
    constexpr float Inv255 = 1.0f / 255.0f;
    return SnAPI::Renderer::UiColor{
        static_cast<float>(Color.R) * Inv255,
        static_cast<float>(Color.G) * Inv255,
        static_cast<float>(Color.B) * Inv255,
        static_cast<float>(Color.A) * Inv255};
}

[[nodiscard]] static SnAPI::Renderer::EUiBlendMode ToRendererUiBlendMode(const SnAPI::UI::BlendMode Blend)
{
    switch (Blend)
    {
    case SnAPI::UI::BlendMode::Opaque:
        return SnAPI::Renderer::EUiBlendMode::Opaque;
    case SnAPI::UI::BlendMode::Additive:
        return SnAPI::Renderer::EUiBlendMode::Additive;
    case SnAPI::UI::BlendMode::Alpha:
    default:
        return SnAPI::Renderer::EUiBlendMode::Alpha;
    }
}

[[nodiscard]] static SnAPI::Renderer::UiRect ToRendererUiRect(
    const float X,
    const float Y,
    const float W,
    const float H,
    const float OffsetX,
    const float OffsetY)
{
    return SnAPI::Renderer::UiRect{X - OffsetX, Y - OffsetY, W, H};
}

[[nodiscard]] static bool ToRendererUiScissor(
    const SnAPI::UI::EScissorMode Mode,
    const SnAPI::UI::ScissorRect& Scissor,
    const float OffsetX,
    const float OffsetY,
    SnAPI::Renderer::UiScissorRect& OutScissor)
{
    OutScissor = {};
    switch (Mode)
    {
    case SnAPI::UI::EScissorMode::None:
        return true;
    case SnAPI::UI::EScissorMode::ClipAll:
        return false;
    case SnAPI::UI::EScissorMode::Rect:
        if (Scissor.W <= 0 || Scissor.H <= 0)
        {
            return false;
        }
        OutScissor.Enabled = true;
        OutScissor.Rect = SnAPI::Renderer::UiRect{
            static_cast<float>(Scissor.X) - OffsetX,
            static_cast<float>(Scissor.Y) - OffsetY,
            static_cast<float>(Scissor.W),
            static_cast<float>(Scissor.H)};
        return true;
    }

    return true;
}

[[nodiscard]] static SnAPI::Renderer::UiPacketKey MakeRendererUiPacketKey(
    const SnAPI::UI::RenderPacket& Packet,
    const SnAPI::Renderer::EUiPacketPipeline Pipeline,
    const SnAPI::Renderer::TextureHandle Texture,
    const SnAPI::Renderer::SamplerHandle Sampler,
    const SnAPI::Renderer::UiScissorRect& Scissor)
{
    return SnAPI::Renderer::UiPacketKey{
        .Pipeline = Pipeline,
        .Texture = Texture,
        .Sampler = Sampler,
        .BlendMode = ToRendererUiBlendMode(Packet.Key.Blend),
        .Scissor = Scissor};
}

[[nodiscard]] static bool SameRendererUiRect(
    const SnAPI::Renderer::UiRect& Left,
    const SnAPI::Renderer::UiRect& Right) noexcept
{
    return Left.X == Right.X &&
           Left.Y == Right.Y &&
           Left.Width == Right.Width &&
           Left.Height == Right.Height;
}

[[nodiscard]] static bool SameRendererUiScissor(
    const SnAPI::Renderer::UiScissorRect& Left,
    const SnAPI::Renderer::UiScissorRect& Right) noexcept
{
    return Left.Enabled == Right.Enabled &&
           (!Left.Enabled || SameRendererUiRect(Left.Rect, Right.Rect));
}

[[nodiscard]] static bool SameRendererUiPacketKey(
    const SnAPI::Renderer::UiPacketKey& Left,
    const SnAPI::Renderer::UiPacketKey& Right) noexcept
{
    return Left.Pipeline == Right.Pipeline &&
           Left.Texture == Right.Texture &&
           Left.Sampler == Right.Sampler &&
           Left.BlendMode == Right.BlendMode &&
           SameRendererUiScissor(Left.Scissor, Right.Scissor);
}

template <typename InstanceType>
static void AppendRendererUiDrawPacket(
    SnAPI::Renderer::UiFramePacket& FramePacket,
    SnAPI::Renderer::UiPacketKey Key,
    InstanceType Instance)
{
    if (!FramePacket.DrawPackets.empty())
    {
        auto& LastPacket = FramePacket.DrawPackets.back();
        if (SameRendererUiPacketKey(LastPacket.Key, Key))
        {
            if (auto* Instances = std::get_if<std::vector<InstanceType>>(&LastPacket.Instances))
            {
                Instances->push_back(std::move(Instance));
                return;
            }
        }
    }

    SnAPI::Renderer::UiDrawPacket DrawPacket{};
    DrawPacket.Key = std::move(Key);
    DrawPacket.Instances = std::vector<InstanceType>{std::move(Instance)};
    FramePacket.DrawPackets.emplace_back(std::move(DrawPacket));
}

[[nodiscard]] static SnAPI::Renderer::UiFramePacket BuildRendererUiFramePacket(
    SnAPI::UI::UIContext& Context,
    const SnAPI::UI::RenderPacketList& Packets,
    const std::function<SnAPI::Renderer::TextureHandle(const SnAPI::UI::UIContext&, std::uint32_t)>& ResolveTexture)
{
    SnAPI::Renderer::UiFramePacket FramePacket{};
    const auto PacketSpan = Packets.Packets();
    FramePacket.DrawPackets.reserve(PacketSpan.size());

    const auto ContextScreenRect = Context.GetScreenRect();
    const float OffsetX = ContextScreenRect.X;
    const float OffsetY = ContextScreenRect.Y;

    for (const auto& Packet : PacketSpan)
    {
        SnAPI::Renderer::UiScissorRect Scissor{};
        if (!ToRendererUiScissor(Packet.Key.ScissorMode, Packet.Key.Scissor, OffsetX, OffsetY, Scissor))
        {
            continue;
        }

        const auto PacketSampler = SnAPI::Renderer::SamplerHandle{static_cast<std::uint64_t>(Packet.Key.Sampler.Value)};

        if (const auto* Rects = std::get_if<SnAPI::UI::RectInstanceSpan>(&Packet.Instances))
        {
            for (const auto& Instance : *Rects)
            {
                AppendRendererUiDrawPacket(
                    FramePacket,
                    MakeRendererUiPacketKey(
                        Packet,
                        SnAPI::Renderer::EUiPacketPipeline::SolidColor,
                        SnAPI::Renderer::TextureHandle{},
                        PacketSampler,
                        Scissor),
                    SnAPI::Renderer::UiRectInstance{
                        .Rect = ToRendererUiRect(Instance.X, Instance.Y, Instance.W, Instance.H, OffsetX, OffsetY),
                        .Fill = ToRendererUiColor(Instance.Fill),
                        .CornerRadius = std::max(0.0f, Instance.CornerRadius),
                        .BorderWidth = std::max(0.0f, Instance.BorderThickness),
                        .BorderColor = ToRendererUiColor(Instance.Border)});
            }
            continue;
        }

        if (const auto* Images = std::get_if<SnAPI::UI::ImageInstanceSpan>(&Packet.Instances))
        {
            for (const auto& Instance : *Images)
            {
                const auto Texture = ResolveTexture(Context, Instance.Texture.Value);
                AppendRendererUiDrawPacket(
                    FramePacket,
                    MakeRendererUiPacketKey(
                        Packet,
                        SnAPI::Renderer::EUiPacketPipeline::Image,
                        Texture,
                        PacketSampler,
                        Scissor),
                    SnAPI::Renderer::UiImageInstance{
                        .Rect = ToRendererUiRect(Instance.X, Instance.Y, Instance.W, Instance.H, OffsetX, OffsetY),
                        .UvRect = SnAPI::Renderer::UiRect{Instance.U0, Instance.V0, Instance.U1 - Instance.U0, Instance.V1 - Instance.V0},
                        .Tint = ToRendererUiColor(Instance.Tint)});
            }
            continue;
        }

        if (const auto* Glyphs = std::get_if<SnAPI::UI::GlyphInstanceSpan>(&Packet.Instances))
        {
            for (const auto& Instance : *Glyphs)
            {
                AppendRendererUiDrawPacket(
                    FramePacket,
                    MakeRendererUiPacketKey(
                        Packet,
                        SnAPI::Renderer::EUiPacketPipeline::Glyph,
                        SnAPI::Renderer::TextureHandle{Instance.AtlasTextureHandle},
                        PacketSampler,
                        Scissor),
                    SnAPI::Renderer::UiGlyphInstance{
                        .Rect = ToRendererUiRect(Instance.X, Instance.Y, Instance.W, Instance.H, OffsetX, OffsetY),
                        .AtlasUvRect = SnAPI::Renderer::UiRect{Instance.U0, Instance.V0, Instance.U1 - Instance.U0, Instance.V1 - Instance.V0},
                        .Color = ToRendererUiColor(Instance.GlyphColor)});
            }
            continue;
        }

        if (const auto* Gradients = std::get_if<SnAPI::UI::GradientInstanceSpan>(&Packet.Instances))
        {
            for (const auto& Instance : *Gradients)
            {
                const std::size_t StopCount =
                    std::min<std::size_t>(SnAPI::UI::MaxGradientStops, static_cast<std::size_t>(Instance.Gradient.StopCount));
                const SnAPI::UI::Color StartColor =
                    StopCount > 0u ? Instance.Gradient.Stops[0].StopColor : SnAPI::UI::Color::Transparent();
                const SnAPI::UI::Color EndColor =
                    StopCount > 0u ? Instance.Gradient.Stops[StopCount - 1u].StopColor : SnAPI::UI::Color::Transparent();
                AppendRendererUiDrawPacket(
                    FramePacket,
                    MakeRendererUiPacketKey(
                        Packet,
                        SnAPI::Renderer::EUiPacketPipeline::Gradient,
                        SnAPI::Renderer::TextureHandle{},
                        PacketSampler,
                        Scissor),
                    SnAPI::Renderer::UiGradientInstance{
                        .Rect = ToRendererUiRect(Instance.X, Instance.Y, Instance.W, Instance.H, OffsetX, OffsetY),
                        .StartColor = ToRendererUiColor(StartColor),
                        .EndColor = ToRendererUiColor(EndColor),
                        .StartPoint = SnAPI::Renderer::Vec2{Instance.Gradient.StartX, Instance.Gradient.StartY},
                        .EndPoint = SnAPI::Renderer::Vec2{Instance.Gradient.EndX, Instance.Gradient.EndY},
                        .Kind = SnAPI::Renderer::EUiGradientKind::Linear});
            }
            continue;
        }

        if (const auto* Shadows = std::get_if<SnAPI::UI::ShadowInstanceSpan>(&Packet.Instances))
        {
            for (const auto& Instance : *Shadows)
            {
                AppendRendererUiDrawPacket(
                    FramePacket,
                    MakeRendererUiPacketKey(
                        Packet,
                        SnAPI::Renderer::EUiPacketPipeline::Shadow,
                        SnAPI::Renderer::TextureHandle{},
                        PacketSampler,
                        Scissor),
                    SnAPI::Renderer::UiShadowInstance{
                        .Rect = ToRendererUiRect(Instance.X, Instance.Y, Instance.W, Instance.H, OffsetX, OffsetY),
                        .Color = ToRendererUiColor(Instance.ShadowColor),
                        .Offset = SnAPI::Renderer::Vec2{0.0f, 0.0f},
                        .BlurRadius = std::max(0.0f, Instance.Blur),
                        .Spread = std::max(0.0f, Instance.Spread + Instance.Expansion),
                        .CornerRadius = std::max(0.0f, Instance.CornerRadius)});
            }
            continue;
        }

        if (const auto* Triangles = std::get_if<SnAPI::UI::TriangleInstanceSpan>(&Packet.Instances))
        {
            for (const auto& Instance : *Triangles)
            {
                AppendRendererUiDrawPacket(
                    FramePacket,
                    MakeRendererUiPacketKey(
                        Packet,
                        SnAPI::Renderer::EUiPacketPipeline::Triangle,
                        SnAPI::Renderer::TextureHandle{},
                        PacketSampler,
                        Scissor),
                    SnAPI::Renderer::UiTriangleInstance{
                        .A = SnAPI::Renderer::Vec2{Instance.X0 - OffsetX, Instance.Y0 - OffsetY},
                        .B = SnAPI::Renderer::Vec2{Instance.X1 - OffsetX, Instance.Y1 - OffsetY},
                        .C = SnAPI::Renderer::Vec2{Instance.X2 - OffsetX, Instance.Y2 - OffsetY},
                        .Color = ToRendererUiColor(Instance.Fill)});
            }
            continue;
        }

        if (const auto* Circles = std::get_if<SnAPI::UI::CircleInstanceSpan>(&Packet.Instances))
        {
            for (const auto& Instance : *Circles)
            {
                AppendRendererUiDrawPacket(
                    FramePacket,
                    MakeRendererUiPacketKey(
                        Packet,
                        SnAPI::Renderer::EUiPacketPipeline::Circle,
                        SnAPI::Renderer::TextureHandle{},
                        PacketSampler,
                        Scissor),
                    SnAPI::Renderer::UiCircleInstance{
                        .Center = SnAPI::Renderer::Vec2{Instance.CenterX - OffsetX, Instance.CenterY - OffsetY},
                        .Radius = std::max(0.0f, Instance.Radius),
                        .Fill = ToRendererUiColor(Instance.Fill),
                        .BorderWidth = std::max(0.0f, Instance.BorderThickness),
                        .BorderColor = ToRendererUiColor(Instance.Border)});
            }
        }
    }

    return FramePacket;
}
#endif
} // namespace

struct RendererSystem::RendererNewRuntimeState
{
#if defined(SNAPI_RENDERER_WITH_PLATFORM_XLIB)
    std::unique_ptr<SnAPI::Renderer::XlibWindow> XlibWindow{};
#endif
    std::unique_ptr<SnAPI::Renderer::RendererRuntime> Runtime{};
    SnAPI::Renderer::RenderScene* Scene = nullptr;
    SnAPI::Renderer::SurfaceHandle Surface{};
    SnAPI::Renderer::SurfacePresentationProfile PresentationProfile{};
    SnAPI::Renderer::ResolvedRenderViewExtents ViewExtents{};
    struct ViewportTargetState
    {
        SnAPI::Renderer::TextureHandle Texture{};
        SnAPI::Renderer::RenderTargetHandle Target{};
        std::uint32_t RenderWidth{1u};
        std::uint32_t RenderHeight{1u};
        bool Enabled{true};
    };
    std::unordered_map<std::uint64_t, ViewportTargetState> ViewportTargets{};
#if defined(SNAPI_GF_ENABLE_UI)
    SnAPI::Renderer::UiFramePacket PendingUiFramePacket{};
    bool UiPacketsQueuedThisFrame = false;
#endif
    std::chrono::steady_clock::time_point LastFrameStart = std::chrono::steady_clock::now();
    std::uint64_t FrameIndex = 0;
    bool SurfaceValid = false;

    [[nodiscard]] bool HasOpenWindow() const noexcept
    {
#if defined(SNAPI_RENDERER_WITH_PLATFORM_XLIB)
        return XlibWindow && !XlibWindow->ShouldClose();
#else
        return false;
#endif
    }

    [[nodiscard]] std::uint32_t Width() const noexcept
    {
#if defined(SNAPI_RENDERER_WITH_PLATFORM_XLIB)
        return XlibWindow ? XlibWindow->Width() : 1u;
#else
        return 1u;
#endif
    }

    [[nodiscard]] std::uint32_t Height() const noexcept
    {
#if defined(SNAPI_RENDERER_WITH_PLATFORM_XLIB)
        return XlibWindow ? XlibWindow->Height() : 1u;
#else
        return 1u;
#endif
    }

    void PumpWindowEvents()
    {
#if defined(SNAPI_RENDERER_WITH_PLATFORM_XLIB)
        if (XlibWindow)
        {
            XlibWindow->PumpEvents();
        }
#endif
    }
};

bool RendererSystem::EnsureRendererNewViewportTarget(
    const std::uint64_t ViewportID,
    const std::uint32_t RenderWidth,
    const std::uint32_t RenderHeight)
{
    if (!m_rendererNew || !m_rendererNew->Runtime || ViewportID == 0u)
    {
        return false;
    }

    auto& State = *m_rendererNew;
    const auto Width = std::max(1u, RenderWidth);
    const auto Height = std::max(1u, RenderHeight);
    auto& TargetState = State.ViewportTargets[ViewportID];
    if (TargetState.Texture.Valid() &&
        TargetState.Target.Valid() &&
        TargetState.RenderWidth == Width &&
        TargetState.RenderHeight == Height)
    {
        return true;
    }

    if (TargetState.Target.Valid())
    {
        (void)State.Runtime->DestroyRenderTarget(TargetState.Target);
        TargetState.Target = {};
    }
    if (TargetState.Texture.Valid())
    {
        (void)State.Runtime->DestroyTexture(TargetState.Texture);
        TargetState.Texture = {};
    }

    const auto ColorFormat = State.PresentationProfile.ColorFormat == SnAPI::Renderer::ETextureFormat::Unknown
        ? SnAPI::Renderer::ETextureFormat::RGBA16Float
        : State.PresentationProfile.ColorFormat;
    auto TextureResult = State.Runtime->CreateTexture(SnAPI::Renderer::TextureDesc{
        .Extent = SnAPI::Renderer::Extent2D{.Width = Width, .Height = Height},
        .Format = ColorFormat,
        .Usage = SnAPI::Renderer::ETextureUsage::RenderTarget,
        .DebugName = "RendererSystem.ViewportColor"});
    if (TextureResult.Failed())
    {
        std::cerr << "RendererSystem failed to create Renderer.New viewport texture: "
                  << TextureResult.Error().Message << '\n';
        return false;
    }

    auto TargetResult = State.Runtime->CreateRenderTarget(SnAPI::Renderer::RenderTargetDesc{
        .Extent = SnAPI::Renderer::Extent2D{.Width = Width, .Height = Height},
        .ColorFormat = ColorFormat,
        .ColorTextureView = SnAPI::Renderer::RenderTargetTextureViewDesc{
            .Texture = TextureResult.Value(),
            .MipLevel = 0u,
            .MipLevelCount = 1u,
            .ArrayLayer = 0u,
            .ArrayLayerCount = 1u},
        .CreateDepth = true,
        .DebugName = "RendererSystem.ViewportTarget"});
    if (TargetResult.Failed())
    {
        (void)State.Runtime->DestroyTexture(TextureResult.Value());
        std::cerr << "RendererSystem failed to create Renderer.New viewport target: "
                  << TargetResult.Error().Message << '\n';
        return false;
    }

    TargetState.Texture = TextureResult.Value();
    TargetState.Target = TargetResult.Value();
    TargetState.RenderWidth = Width;
    TargetState.RenderHeight = Height;
    return true;
}

void RendererSystem::DestroyRendererNewViewportTarget(const std::uint64_t ViewportID)
{
    if (!m_rendererNew)
    {
        return;
    }

    auto& State = *m_rendererNew;
    auto It = State.ViewportTargets.find(ViewportID);
    if (It == State.ViewportTargets.end() || !State.Runtime)
    {
        if (It != State.ViewportTargets.end())
        {
            State.ViewportTargets.erase(It);
        }
        return;
    }

    if (It->second.Target.Valid())
    {
        (void)State.Runtime->DestroyRenderTarget(It->second.Target);
    }
    if (It->second.Texture.Valid())
    {
        (void)State.Runtime->DestroyTexture(It->second.Texture);
    }
    State.ViewportTargets.erase(It);
}

[[nodiscard]] static bool ShouldRenderRendererNewViewportPreset(const ERenderViewportPassGraphPreset Preset) noexcept
{
    return Preset == ERenderViewportPassGraphPreset::DefaultWorld ||
           Preset == ERenderViewportPassGraphPreset::EditorWorld;
}

void RendererSystem::RendererNewRuntimeStateDeleter::operator()(RendererNewRuntimeState* State) const
{
    delete State;
}

void RendererSystem::WindowDeleter::operator()(SnAPI::Graphics::WindowBase* Window) const
{
    (void)Window;
}

void RendererSystem::LightManagerDeleter::operator()(SnAPI::Graphics::LightManager* Manager) const
{
    (void)Manager;
}

RendererSystem::~RendererSystem()
{
    Shutdown();
}

RendererSystem::RendererSystem(RendererSystem&& Other) noexcept
{
    GameLockGuard Lock(Other.m_mutex);
    m_settings = std::move(Other.m_settings);
    m_rendererNew = std::move(Other.m_rendererNew);
    m_activeCamera = std::move(Other.m_activeCamera);
    m_textQueue = std::move(Other.m_textQueue);
#if defined(SNAPI_GF_ENABLE_UI)
    m_uiPendingTextureUploads = std::move(Other.m_uiPendingTextureUploads);
    m_uiQueuedRects = std::move(Other.m_uiQueuedRects);
    m_uiPacketsQueuedThisFrame = Other.m_uiPacketsQueuedThisFrame;
#endif
    m_registeredRenderObjects = std::move(Other.m_registeredRenderObjects);
    m_registeredViewportPassGraphs = std::move(Other.m_registeredViewportPassGraphs);
    m_renderViewportPassGraphRevision = Other.m_renderViewportPassGraphRevision;
    m_defaultTaaJitterScale = Other.m_defaultTaaJitterScale;
    m_viewportTaaJitterScales = std::move(Other.m_viewportTaaJitterScales);
    m_taaFrameIndex = Other.m_taaFrameIndex;
    m_initialized = Other.m_initialized;

    Other.m_activeCamera.reset();
    Other.m_rendererNew.reset();
    Other.m_textQueue.clear();
#if defined(SNAPI_GF_ENABLE_UI)
    Other.m_uiPendingTextureUploads.clear();
    Other.m_uiQueuedRects.clear();
    Other.m_uiPacketsQueuedThisFrame = false;
#endif
    Other.m_registeredRenderObjects.clear();
    Other.m_registeredViewportPassGraphs.clear();
    Other.m_renderViewportPassGraphRevision = 1;
    Other.m_defaultTaaJitterScale = 1.0f;
    Other.m_viewportTaaJitterScales.clear();
    Other.m_taaFrameIndex = 0;
    Other.m_initialized = false;
}

RendererSystem& RendererSystem::operator=(RendererSystem&& Other) noexcept
{
    if (this == &Other)
    {
        return *this;
    }

    Shutdown();

    std::scoped_lock Lock(m_mutex, Other.m_mutex);
    m_settings = std::move(Other.m_settings);
    m_rendererNew = std::move(Other.m_rendererNew);
    m_activeCamera = std::move(Other.m_activeCamera);
    m_textQueue = std::move(Other.m_textQueue);
#if defined(SNAPI_GF_ENABLE_UI)
    m_uiPendingTextureUploads = std::move(Other.m_uiPendingTextureUploads);
    m_uiQueuedRects = std::move(Other.m_uiQueuedRects);
    m_uiPacketsQueuedThisFrame = Other.m_uiPacketsQueuedThisFrame;
#endif
    m_registeredRenderObjects = std::move(Other.m_registeredRenderObjects);
    m_registeredViewportPassGraphs = std::move(Other.m_registeredViewportPassGraphs);
    m_renderViewportPassGraphRevision = Other.m_renderViewportPassGraphRevision;
    m_defaultTaaJitterScale = Other.m_defaultTaaJitterScale;
    m_viewportTaaJitterScales = std::move(Other.m_viewportTaaJitterScales);
    m_taaFrameIndex = Other.m_taaFrameIndex;
    m_initialized = Other.m_initialized;

    Other.m_activeCamera.reset();
    Other.m_rendererNew.reset();
    Other.m_textQueue.clear();
#if defined(SNAPI_GF_ENABLE_UI)
    Other.m_uiPendingTextureUploads.clear();
    Other.m_uiQueuedRects.clear();
    Other.m_uiPacketsQueuedThisFrame = false;
#endif
    Other.m_registeredRenderObjects.clear();
    Other.m_registeredViewportPassGraphs.clear();
    Other.m_renderViewportPassGraphRevision = 1;
    Other.m_defaultTaaJitterScale = 1.0f;
    Other.m_viewportTaaJitterScales.clear();
    Other.m_taaFrameIndex = 0;
    Other.m_initialized = false;
    return *this;
}

TaskHandle RendererSystem::EnqueueTask(WorkTask InTask, CompletionTask OnComplete)
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    return m_taskQueue.EnqueueTask(std::move(InTask), std::move(OnComplete));
}

void RendererSystem::EnqueueThreadTask(std::function<void()> InTask)
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    m_taskQueue.EnqueueThreadTask(std::move(InTask));
}

void RendererSystem::ExecuteQueuedTasks()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    m_taskQueue.ExecuteQueuedTasks(*this, m_mutex);
}

bool RendererSystem::Initialize()
{
    return Initialize(RendererBootstrapSettings{});
}

bool RendererSystem::Initialize(const RendererBootstrapSettings& Settings)
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    GameLockGuard Lock(m_mutex);
    m_settings = Settings;
    return InitializeUnlocked();
}

bool RendererSystem::InitializeUnlocked()
{
    m_registeredViewportPassGraphs.clear();
    m_renderViewportPassGraphRevision = 1;
    m_taaFrameIndex = 0;
    m_rendererNew.reset();
    m_initialized = false;

    if (!m_settings.CreateGraphicsApi)
    {
        return true;
    }

    std::unique_ptr<RendererNewRuntimeState, RendererNewRuntimeStateDeleter> State{new RendererNewRuntimeState()};
    SnAPI::Renderer::RendererConfig Config{};
    Config.PreferredBackend = SnAPI::Renderer::EGraphicsBackend::Vulkan;
    Config.EnableXr = false;

    auto RuntimeResult = SnAPI::Renderer::RendererRuntime::Create(Config);
    if (RuntimeResult.Failed())
    {
        std::cerr << "RendererSystem failed to create renderer runtime: " << RuntimeResult.Error().Message << '\n';
        return false;
    }

    State->Runtime = std::move(RuntimeResult).Value();
    State->Scene = &State->Runtime->CreateRenderScene();
    State->LastFrameStart = std::chrono::steady_clock::now();
    m_rendererNew = std::move(State);

    if (m_settings.CreateWindow && !CreateWindowResources())
    {
        m_rendererNew.reset();
        return false;
    }

    m_initialized = true;
    if (m_initialized && m_settings.RegisterDefaultPassGraph)
    {
        (void)UseDefaultRenderViewport(true);
        (void)RegisterDefaultPassGraph();
    }
    return true;
}

void RendererSystem::ApplyOutOfMemoryFallbackSettings()
{
    m_settings.WindowWidth = std::min(m_settings.WindowWidth, ClampWindowExtent(m_settings.OutOfMemoryFallbackWindowWidth));
    m_settings.WindowHeight = std::min(m_settings.WindowHeight, ClampWindowExtent(m_settings.OutOfMemoryFallbackWindowHeight));
    if (m_settings.ForceWindowedOnOutOfMemory)
    {
        m_settings.FullScreen = false;
        m_settings.Borderless = false;
    }
    if (m_settings.DisableTransparencyOnOutOfMemory)
    {
        m_settings.AllowTransparency = false;
    }
}

void RendererSystem::Shutdown()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    GameLockGuard Lock(m_mutex);
    ShutdownUnlocked();
}

void RendererSystem::ShutdownUnlocked()
{
    m_initialized = false;
    m_rendererNew.reset();
    m_graphics = nullptr;
    m_window.reset();
    m_lightManager.reset();
    ResetPassPointers();
    m_activeCamera.reset();
    m_defaultGBufferMaterial.reset();
    m_defaultShadowMaterial.reset();
    m_defaultGBufferMaterialInstance.reset();
    m_defaultShadowMaterialInstance.reset();
    m_defaultFont = nullptr;
    m_defaultFontFallbacksConfigured = false;
    m_textQueue.clear();
#if defined(SNAPI_GF_ENABLE_UI)
    m_uiPendingTextureUploads.clear();
    m_uiQueuedRects.clear();
    m_uiPacketsQueuedThisFrame = false;
#endif
    m_registeredRenderObjects.clear();
    m_registeredViewportPassGraphs.clear();
    m_renderViewportPassGraphRevision = 1;
    m_viewportTaaJitterScales.clear();
    m_taaFrameIndex = 0;
}

bool RendererSystem::IsInitialized() const
{
    GameLockGuard Lock(m_mutex);
    return m_initialized;
}

SnAPI::Graphics::VulkanGraphicsAPI* RendererSystem::Graphics()
{
    return nullptr;
}

const SnAPI::Graphics::VulkanGraphicsAPI* RendererSystem::Graphics() const
{
    return nullptr;
}

SnAPI::Graphics::WindowBase* RendererSystem::Window()
{
    return nullptr;
}

const SnAPI::Graphics::WindowBase* RendererSystem::Window() const
{
    return nullptr;
}

bool RendererSystem::HasOpenWindow() const
{
    GameLockGuard Lock(m_mutex);
    return m_initialized && m_rendererNew && m_rendererNew->HasOpenWindow();
}

bool RendererSystem::SetActiveCamera(const std::shared_ptr<SnAPI::Graphics::ICamera>& Camera)
{
    GameLockGuard Lock(m_mutex);
    m_activeCamera = Camera;
    return m_initialized;
}

bool RendererSystem::SetActiveCamera(SnAPI::Graphics::ICamera* Camera)
{
    GameLockGuard Lock(m_mutex);
    if (!Camera)
    {
        m_activeCamera.reset();
    }
    return m_initialized;
}

SnAPI::Graphics::ICamera* RendererSystem::ActiveCamera() const
{
    GameLockGuard Lock(m_mutex);
    return m_activeCamera.get();
}

std::shared_ptr<SnAPI::Graphics::ICamera> RendererSystem::ActiveCameraShared() const
{
    GameLockGuard Lock(m_mutex);
    return m_activeCamera;
}

bool RendererSystem::SetProjectShaderSearchRoot(const std::filesystem::path& AssetRoot)
{
    (void)AssetRoot;
    GameLockGuard Lock(m_mutex);
    return m_initialized;
}

bool RendererSystem::SetViewPort(const SnAPI::Graphics::ViewportFit& ViewPort)
{
    (void)ViewPort;
    GameLockGuard Lock(m_mutex);
    return m_initialized;
}

bool RendererSystem::ClearViewPort()
{
    GameLockGuard Lock(m_mutex);
    return m_initialized;
}

bool RendererSystem::UseDefaultRenderViewport(const bool Enabled)
{
    GameLockGuard Lock(m_mutex);
    if (!m_initialized)
    {
        return false;
    }

    if (Enabled)
    {
        m_registeredViewportPassGraphs.try_emplace(1u, ERenderViewportPassGraphPreset::None);
    }
    else
    {
        DestroyRendererNewViewportTarget(1u);
        m_registeredViewportPassGraphs.erase(1u);
    }
    ++m_renderViewportPassGraphRevision;
    return true;
}

bool RendererSystem::IsUsingDefaultRenderViewport() const
{
    GameLockGuard Lock(m_mutex);
    return m_registeredViewportPassGraphs.contains(1u);
}

bool RendererSystem::SetPassViewPort(SnAPI::Graphics::ERenderPassType PassType, const SnAPI::Graphics::ViewportFit& ViewPort)
{
    (void)PassType;
    (void)ViewPort;
    GameLockGuard Lock(m_mutex);
    return m_initialized;
}

bool RendererSystem::ClearPassViewPort(SnAPI::Graphics::ERenderPassType PassType)
{
    (void)PassType;
    GameLockGuard Lock(m_mutex);
    return m_initialized;
}

bool RendererSystem::ClearPassViewPorts()
{
    GameLockGuard Lock(m_mutex);
    return m_initialized;
}

bool RendererSystem::CreateRenderViewport(std::string Name,
                                          const float X,
                                          const float Y,
                                          const float Width,
                                          const float Height,
                                          const std::uint32_t RenderWidth,
                                          const std::uint32_t RenderHeight,
                                          SnAPI::Graphics::ICamera* Camera,
                                          const bool Enabled,
                                          std::uint64_t& OutViewportID)
{
    (void)Name;
    (void)X;
    (void)Y;
    (void)Camera;
    (void)Enabled;
    GameLockGuard Lock(m_mutex);
    if (!m_initialized)
    {
        OutViewportID = 0;
        return false;
    }

    const auto ResolvedRenderWidth = ResolveExtent(RenderWidth, Width);
    const auto ResolvedRenderHeight = ResolveExtent(RenderHeight, Height);
    OutViewportID = 1u;
    while (m_registeredViewportPassGraphs.contains(OutViewportID))
    {
        ++OutViewportID;
    }
    if (!EnsureRendererNewViewportTarget(OutViewportID, ResolvedRenderWidth, ResolvedRenderHeight))
    {
        OutViewportID = 0u;
        return false;
    }
    if (m_rendererNew)
    {
        m_rendererNew->ViewportTargets[OutViewportID].Enabled = Enabled;
    }
    m_registeredViewportPassGraphs.emplace(OutViewportID, ERenderViewportPassGraphPreset::None);
    ++m_renderViewportPassGraphRevision;
    return true;
}

bool RendererSystem::UpdateRenderViewport(const std::uint64_t ViewportID,
                                          std::string Name,
                                          const float X,
                                          const float Y,
                                          const float Width,
                                          const float Height,
                                          const std::uint32_t RenderWidth,
                                          const std::uint32_t RenderHeight,
                                          SnAPI::Graphics::ICamera* Camera,
                                          const bool Enabled)
{
    (void)Name;
    (void)X;
    (void)Y;
    (void)Camera;
    GameLockGuard Lock(m_mutex);
    if (!m_initialized || !m_registeredViewportPassGraphs.contains(ViewportID))
    {
        return false;
    }

    const auto ResolvedRenderWidth = ResolveExtent(RenderWidth, Width);
    const auto ResolvedRenderHeight = ResolveExtent(RenderHeight, Height);
    if (!EnsureRendererNewViewportTarget(ViewportID, ResolvedRenderWidth, ResolvedRenderHeight))
    {
        return false;
    }
    if (m_rendererNew)
    {
        m_rendererNew->ViewportTargets[ViewportID].Enabled = Enabled;
    }
    return true;
}

bool RendererSystem::DestroyRenderViewport(const std::uint64_t ViewportID)
{
    GameLockGuard Lock(m_mutex);
    DestroyRendererNewViewportTarget(ViewportID);
    const bool Removed = m_registeredViewportPassGraphs.erase(ViewportID) > 0u;
    if (Removed)
    {
        ++m_renderViewportPassGraphRevision;
    }
    return Removed;
}

bool RendererSystem::HasRenderViewport(const std::uint64_t ViewportID) const
{
    GameLockGuard Lock(m_mutex);
    return m_registeredViewportPassGraphs.contains(ViewportID);
}

bool RendererSystem::SetRenderViewportIndex(const std::uint64_t ViewportID, const std::size_t Index)
{
    (void)Index;
    GameLockGuard Lock(m_mutex);
    return m_initialized && m_registeredViewportPassGraphs.contains(ViewportID);
}

std::optional<std::size_t> RendererSystem::RenderViewportIndex(const std::uint64_t ViewportID) const
{
    GameLockGuard Lock(m_mutex);
    if (!m_registeredViewportPassGraphs.contains(ViewportID))
    {
        return std::nullopt;
    }
    return static_cast<std::size_t>(ViewportID - 1u);
}

bool RendererSystem::CreateRenderTargetSwapChain(const std::uint32_t Width,
                                                 const std::uint32_t Height,
                                                 std::uint64_t& OutSwapChainID,
                                                 const std::uint32_t ImageCount)
{
    (void)Width;
    (void)Height;
    (void)ImageCount;
    GameLockGuard Lock(m_mutex);
    if (!m_initialized)
    {
        OutSwapChainID = 0;
        return false;
    }
    OutSwapChainID = 1u;
    return true;
}

bool RendererSystem::ResizeSwapChain(const std::uint64_t SwapChainID, const std::uint32_t Width, const std::uint32_t Height)
{
    (void)SwapChainID;
    (void)Width;
    (void)Height;
    GameLockGuard Lock(m_mutex);
    return m_initialized;
}

bool RendererSystem::DestroySwapChain(const std::uint64_t SwapChainID)
{
    (void)SwapChainID;
    GameLockGuard Lock(m_mutex);
    return m_initialized;
}

bool RendererSystem::AssignSwapChainToRenderViewport(const std::uint64_t ViewportID, const std::uint64_t SwapChainID)
{
    (void)SwapChainID;
    GameLockGuard Lock(m_mutex);
    return m_initialized && m_registeredViewportPassGraphs.contains(ViewportID);
}

std::optional<std::uint64_t> RendererSystem::RenderViewportSwapChain(const std::uint64_t ViewportID) const
{
    GameLockGuard Lock(m_mutex);
    if (!m_registeredViewportPassGraphs.contains(ViewportID))
    {
        return std::nullopt;
    }
    return 1u;
}

bool RendererSystem::RegisterRenderViewportPassGraph(const std::uint64_t ViewportID, const ERenderViewportPassGraphPreset Preset)
{
    GameLockGuard Lock(m_mutex);
    return RegisterRenderViewportPassGraphUnlocked(ViewportID, Preset, true);
}

bool RendererSystem::RegisterRenderViewportPassGraphUnlocked(const std::uint64_t ViewportID,
                                                             const ERenderViewportPassGraphPreset Preset,
                                                             const bool TrackDefaultPassPointers)
{
    (void)TrackDefaultPassPointers;
    if (!m_initialized || ViewportID == 0u)
    {
        return false;
    }

    const auto [It, Inserted] = m_registeredViewportPassGraphs.try_emplace(ViewportID, Preset);
    if (!Inserted && It->second != Preset && It->second != ERenderViewportPassGraphPreset::None)
    {
        return false;
    }
    It->second = Preset;
    ++m_renderViewportPassGraphRevision;
    return true;
}

bool RendererSystem::SetRenderViewportGlobalInputNameOverrides(std::uint64_t ViewportID, std::vector<std::pair<std::string, std::string>> Overrides)
{
    (void)Overrides;
    return HasRenderViewport(ViewportID);
}

bool RendererSystem::SetRenderViewportGlobalOutputNameOverrides(std::uint64_t ViewportID, std::vector<std::pair<std::string, std::string>> Overrides)
{
    (void)Overrides;
    return HasRenderViewport(ViewportID);
}

bool RendererSystem::SetRenderViewportPassInputNameOverrides(std::uint64_t ViewportID,
                                                             const SnAPI::Graphics::IHighLevelPass* Pass,
                                                             std::vector<std::pair<std::string, std::string>> Overrides)
{
    (void)Pass;
    (void)Overrides;
    return HasRenderViewport(ViewportID);
}

bool RendererSystem::SetRenderViewportPassOutputNameOverrides(std::uint64_t ViewportID,
                                                              const SnAPI::Graphics::IHighLevelPass* Pass,
                                                              std::vector<std::pair<std::string, std::string>> Overrides)
{
    (void)Pass;
    (void)Overrides;
    return HasRenderViewport(ViewportID);
}

bool RendererSystem::ClearRenderViewportPassNameOverrides(std::uint64_t ViewportID, const SnAPI::Graphics::IHighLevelPass* Pass)
{
    (void)Pass;
    return HasRenderViewport(ViewportID);
}

bool RendererSystem::ClearRenderViewportNameOverrides(std::uint64_t ViewportID)
{
    return HasRenderViewport(ViewportID);
}

bool RendererSystem::TrackRegisteredRenderObjectLocked(const std::shared_ptr<SnAPI::Graphics::IRenderObject>& RenderObject)
{
    if (!RenderObject)
    {
        return false;
    }
    m_registeredRenderObjects.emplace_back(RenderObject);
    return true;
}

bool RendererSystem::UntrackRegisteredRenderObjectLocked(const SnAPI::Graphics::IRenderObject* RenderObject)
{
    const auto OldSize = m_registeredRenderObjects.size();
    std::erase_if(m_registeredRenderObjects, [RenderObject](const auto& Entry) {
        const auto Shared = Entry.lock();
        return !Shared || Shared.get() == RenderObject;
    });
    return m_registeredRenderObjects.size() != OldSize;
}

void RendererSystem::PruneTrackedRenderObjectIfUnreferencedLocked(const SnAPI::Graphics::IRenderObject* RenderObject)
{
    (void)UntrackRegisteredRenderObjectLocked(RenderObject);
}

bool RendererSystem::ConfigureRenderObjectPassesLocked(const std::shared_ptr<SnAPI::Graphics::IRenderObject>& RenderObject,
                                                       bool Visible,
                                                       bool CastShadows)
{
    (void)Visible;
    (void)CastShadows;
    return TrackRegisteredRenderObjectLocked(RenderObject);
}

bool RendererSystem::AddRenderObject(const std::weak_ptr<SnAPI::Graphics::IRenderObject>& RenderObject,
                                     const std::uint64_t ViewportID,
                                     SnAPI::Graphics::ERenderPassType PassType)
{
    (void)ViewportID;
    (void)PassType;
    GameLockGuard Lock(m_mutex);
    return TrackRegisteredRenderObjectLocked(RenderObject.lock());
}

bool RendererSystem::AddRenderObject(const std::weak_ptr<SnAPI::Graphics::IRenderObject>& RenderObject, const Uuid& PassID)
{
    (void)PassID;
    GameLockGuard Lock(m_mutex);
    return TrackRegisteredRenderObjectLocked(RenderObject.lock());
}

bool RendererSystem::RemoveRenderObject(const std::weak_ptr<SnAPI::Graphics::IRenderObject>& RenderObject,
                                        std::uint64_t ViewportID,
                                        SnAPI::Graphics::ERenderPassType PassType)
{
    (void)ViewportID;
    (void)PassType;
    GameLockGuard Lock(m_mutex);
    const auto Shared = RenderObject.lock();
    return Shared ? UntrackRegisteredRenderObjectLocked(Shared.get()) : false;
}

bool RendererSystem::RemoveRenderObject(const std::weak_ptr<SnAPI::Graphics::IRenderObject>& RenderObject, const Uuid& PassID)
{
    (void)PassID;
    GameLockGuard Lock(m_mutex);
    const auto Shared = RenderObject.lock();
    return Shared ? UntrackRegisteredRenderObjectLocked(Shared.get()) : false;
}

bool RendererSystem::RemoveRenderObject(const std::weak_ptr<SnAPI::Graphics::IRenderObject>& RenderObject)
{
    GameLockGuard Lock(m_mutex);
    const auto Shared = RenderObject.lock();
    return Shared ? UntrackRegisteredRenderObjectLocked(Shared.get()) : false;
}

bool RendererSystem::ApplyDefaultMaterials(SnAPI::Graphics::IRenderObject& RenderObject)
{
    (void)RenderObject;
    return false;
}

std::shared_ptr<SnAPI::Graphics::Material> RendererSystem::DefaultGBufferMaterial()
{
    return {};
}

std::shared_ptr<SnAPI::Graphics::Material> RendererSystem::DefaultShadowMaterial()
{
    return {};
}

bool RendererSystem::ConfigureRenderObjectPasses(const std::shared_ptr<SnAPI::Graphics::IRenderObject>& RenderObject,
                                                 const bool Visible,
                                                 const bool CastShadows)
{
    GameLockGuard Lock(m_mutex);
    return ConfigureRenderObjectPassesLocked(RenderObject, Visible, CastShadows);
}

std::uint64_t RendererSystem::RenderViewportPassGraphRevision() const
{
    GameLockGuard Lock(m_mutex);
    return m_renderViewportPassGraphRevision;
}

void RendererSystem::SetDefaultTaaJitterScale(const float Value)
{
    GameLockGuard Lock(m_mutex);
    m_defaultTaaJitterScale = std::max(0.0f, Value);
}

void RendererSystem::SetViewportTaaJitterScale(const std::uint64_t ViewportID, const float Value)
{
    GameLockGuard Lock(m_mutex);
    m_viewportTaaJitterScales[ViewportID] = std::max(0.0f, Value);
}

bool RendererSystem::RecreateSwapChain()
{
    GameLockGuard Lock(m_mutex);
    return m_initialized;
}

bool RendererSystem::RecreateSwapChainForCurrentWindowUnlocked()
{
    return m_initialized;
}

bool RendererSystem::LoadDefaultFont(const std::string& FontPath, const std::uint32_t FontSize)
{
    (void)FontPath;
    (void)FontSize;
    GameLockGuard Lock(m_mutex);
    m_defaultFontFallbacksConfigured = m_initialized;
    return m_initialized;
}

bool RendererSystem::QueueText(std::string Text, const float X, const float Y)
{
    GameLockGuard Lock(m_mutex);
    if (!m_initialized || Text.empty())
    {
        return false;
    }
    m_textQueue.push_back(TextRequest{std::move(Text), X, Y});
    return true;
}

bool RendererSystem::HasDefaultFont() const
{
    GameLockGuard Lock(m_mutex);
    return m_defaultFontFallbacksConfigured;
}

SnAPI::Graphics::FontFace* RendererSystem::EnsureDefaultFontFace()
{
    return nullptr;
}

#if defined(SNAPI_GF_ENABLE_UI)
bool RendererSystem::QueueUiRenderPackets(const std::uint64_t ViewportID,
                                          SnAPI::UI::UIContext& Context,
                                          const SnAPI::UI::RenderPacketList& Packets)
{
    GameLockGuard Lock(m_mutex);
    if (!m_initialized || ViewportID == 0 || !m_rendererNew)
    {
        return false;
    }

    const auto ResolveTexture = [this](const SnAPI::UI::UIContext& PacketContext, const std::uint32_t TextureId)
        -> SnAPI::Renderer::TextureHandle
    {
        const auto BindingIt = m_uiExternalTextureBindings.find(UiTextureCacheKey{&PacketContext, TextureId});
        if (BindingIt == m_uiExternalTextureBindings.end() || !m_rendererNew)
        {
            return {};
        }

        const auto TargetIt = m_rendererNew->ViewportTargets.find(BindingIt->second.SourceViewportID);
        if (TargetIt == m_rendererNew->ViewportTargets.end())
        {
            return {};
        }

        return TargetIt->second.Texture;
    };

    auto FramePacket = BuildRendererUiFramePacket(Context, Packets, ResolveTexture);
    if (FramePacket.Empty())
    {
        if (ShouldLogRendererNewUiBridge())
        {
            std::cerr << "[GameFramework][Renderer.New][UI] queue viewport=" << ViewportID
                      << " sourcePackets=" << Packets.Packets().size()
                      << " translated empty\n";
        }
        return true;
    }

    if (!m_rendererNew->UiPacketsQueuedThisFrame)
    {
        m_rendererNew->PendingUiFramePacket = {};
        m_rendererNew->UiPacketsQueuedThisFrame = true;
    }

    auto& TargetPackets = m_rendererNew->PendingUiFramePacket.DrawPackets;
    TargetPackets.reserve(TargetPackets.size() + FramePacket.DrawPackets.size());
    for (auto& DrawPacket : FramePacket.DrawPackets)
    {
        TargetPackets.emplace_back(std::move(DrawPacket));
    }

    if (ShouldLogRendererNewUiBridge())
    {
        std::cerr << "[GameFramework][Renderer.New][UI] queue viewport=" << ViewportID
                  << " drawPackets=" << m_rendererNew->PendingUiFramePacket.DrawPacketCount()
                  << " instances=" << m_rendererNew->PendingUiFramePacket.InstanceCount()
                  << '\n';
    }

    m_uiPacketsQueuedThisFrame = true;
    return true;
}

bool RendererSystem::QueueUiRenderPackets(SnAPI::UI::UIContext& Context, const SnAPI::UI::RenderPacketList& Packets)
{
    return QueueUiRenderPackets(1u, Context, Packets);
}

bool RendererSystem::RegisterExternalViewportUiTexture(const SnAPI::UI::UIContext& Context,
                                                       const std::uint32_t TextureId,
                                                       const std::uint64_t SourceViewportID,
                                                       const bool HasTransparency)
{
    (void)HasTransparency;
    GameLockGuard Lock(m_mutex);
    if (!m_initialized)
    {
        return false;
    }
    m_uiExternalTextureBindings[UiTextureCacheKey{&Context, TextureId}] = UiExternalTextureBinding{SourceViewportID, HasTransparency};
    return true;
}

bool RendererSystem::RegisterExternalImageUiTexture(const SnAPI::UI::UIContext& Context,
                                                    const std::uint32_t TextureId,
                                                    SnAPI::Graphics::IGPUImage* Image,
                                                    const bool HasTransparency)
{
    GameLockGuard Lock(m_mutex);
    if (!m_initialized)
    {
        return false;
    }
    m_uiExternalImageBindings[UiTextureCacheKey{&Context, TextureId}] = UiExternalImageBinding{Image, HasTransparency};
    return true;
}

bool RendererSystem::UnregisterExternalViewportUiTexture(const SnAPI::UI::UIContext& Context, const std::uint32_t TextureId)
{
    GameLockGuard Lock(m_mutex);
    return m_uiExternalTextureBindings.erase(UiTextureCacheKey{&Context, TextureId}) > 0u;
}

bool RendererSystem::UnregisterExternalImageUiTexture(const SnAPI::UI::UIContext& Context, const std::uint32_t TextureId)
{
    GameLockGuard Lock(m_mutex);
    return m_uiExternalImageBindings.erase(UiTextureCacheKey{&Context, TextureId}) > 0u;
}

bool RendererSystem::EnsureUiMaterialResources()
{
    return m_initialized;
}

std::shared_ptr<SnAPI::Graphics::MaterialInstance> RendererSystem::ResolveUiMaterialForTexture(const SnAPI::UI::UIContext& Context,
                                                                                               const std::uint32_t TextureId)
{
    (void)Context;
    (void)TextureId;
    return {};
}

std::shared_ptr<SnAPI::Graphics::MaterialInstance> RendererSystem::ResolveUiMaterialForGradient(const QueuedUiRect& Entry)
{
    (void)Entry;
    return {};
}

std::shared_ptr<SnAPI::Graphics::MaterialInstance> RendererSystem::ResolveUiFontMaterialInstance(const std::uint64_t AtlasTextureHandle)
{
    (void)AtlasTextureHandle;
    return {};
}

void RendererSystem::FlushQueuedUiPackets()
{
    if (m_rendererNew && m_rendererNew->Scene && m_rendererNew->UiPacketsQueuedThisFrame)
    {
        if (ShouldLogRendererNewUiBridge())
        {
            std::cerr << "[GameFramework][Renderer.New][UI] submit drawPackets="
                      << m_rendererNew->PendingUiFramePacket.DrawPacketCount()
                      << " instances=" << m_rendererNew->PendingUiFramePacket.InstanceCount()
                      << '\n';
        }
        auto SubmitResult = m_rendererNew->Scene->SubmitUiFramePacket(std::move(m_rendererNew->PendingUiFramePacket));
        if (SubmitResult.Failed())
        {
            std::cerr << "RendererSystem failed to submit UI frame packet: "
                      << SubmitResult.Error().Message << '\n';
        }

        m_rendererNew->PendingUiFramePacket = {};
        m_rendererNew->UiPacketsQueuedThisFrame = false;
    }

    m_uiQueuedRects.clear();
    m_uiPacketsQueuedThisFrame = false;
}
#endif

void RendererSystem::EndFrame()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    ExecuteQueuedTasks();
    GameLockGuard Lock(m_mutex);
    if (!m_initialized || !m_rendererNew || !m_rendererNew->Runtime || !m_rendererNew->Scene)
    {
        return;
    }

    if (!m_rendererNew->SurfaceValid)
    {
#if defined(SNAPI_GF_ENABLE_UI)
        m_rendererNew->PendingUiFramePacket = {};
        m_rendererNew->UiPacketsQueuedThisFrame = false;
        m_uiQueuedRects.clear();
        m_uiPacketsQueuedThisFrame = false;
#endif
        ++m_taaFrameIndex;
        return;
    }

    m_rendererNew->PumpWindowEvents();
    if (!m_rendererNew->HasOpenWindow())
    {
        m_initialized = false;
        return;
    }

    const auto PlatformExtent =
        SnAPI::Renderer::Extent2D{.Width = m_rendererNew->Width(), .Height = m_rendererNew->Height()};
    const auto ViewExtents = SnAPI::Renderer::RenderResolutionSettings{}.Resolve(PlatformExtent);
    if (!RendererNewViewExtentsEqual(ViewExtents, m_rendererNew->ViewExtents))
    {
        auto ConfigureResult = m_rendererNew->Runtime->ConfigureFramePipeline(
            BuildRendererNewFramePipelineSettings(ViewExtents, m_rendererNew->PresentationProfile));
        if (ConfigureResult.Failed())
        {
            std::cerr << "RendererSystem failed to reconfigure frame pipeline: "
                      << ConfigureResult.Error().Message << '\n';
            m_initialized = false;
            return;
        }
        m_rendererNew->ViewExtents = ViewExtents;
    }

    const auto Now = std::chrono::steady_clock::now();
    const double DeltaSeconds =
        std::max(0.0, std::chrono::duration<double>(Now - m_rendererNew->LastFrameStart).count());
    m_rendererNew->LastFrameStart = Now;
    const auto TimingStamp = MakeRendererTimingStamp(Now);
    const auto FrameTiming = SnAPI::Renderer::FrameTimingInfo{
        .FrameStartTime = TimingStamp,
        .TargetFrameStartTime = TimingStamp,
        .PredictedPresentationTime = TimingStamp};
    const auto FrameIndex = m_rendererNew->FrameIndex + 1u;

    auto BeginFrameResult = m_rendererNew->Runtime->BeginFrame(SnAPI::Renderer::FrameBeginDesc{
        .FrameIndex = FrameIndex,
        .DeltaTimeSeconds = DeltaSeconds,
        .Timing = FrameTiming});
    if (BeginFrameResult.Failed())
    {
        std::cerr << "RendererSystem failed during BeginFrame: " << BeginFrameResult.Error().Message << '\n';
        m_initialized = false;
        return;
    }

    for (const auto& [ViewportID, Preset] : m_registeredViewportPassGraphs)
    {
        if (!ShouldRenderRendererNewViewportPreset(Preset))
        {
            continue;
        }

        const auto TargetIt = m_rendererNew->ViewportTargets.find(ViewportID);
        if (TargetIt == m_rendererNew->ViewportTargets.end() ||
            !TargetIt->second.Enabled ||
            !TargetIt->second.Target.Valid())
        {
            continue;
        }

        const auto TargetExtent = SnAPI::Renderer::Extent2D{
            .Width = std::max(1u, TargetIt->second.RenderWidth),
            .Height = std::max(1u, TargetIt->second.RenderHeight)};
        const auto View = SnAPI::Renderer::RenderView{
            .DebugName = "RendererSystem.Viewport." + std::to_string(ViewportID),
            .PlatformPresentationExtent = TargetExtent,
            .OutputExtent = TargetExtent,
            .InternalRenderExtent = TargetExtent,
            .Width = TargetExtent.Width,
            .Height = TargetExtent.Height,
            .CameraRelative = true,
            .ReverseZ = true,
            .NearPlane = 0.01,
            .Timing = FrameTiming,
            .TemporalViewKey = "Viewport." + std::to_string(ViewportID),
            .TemporalHistoryValid = m_rendererNew->FrameIndex > 0u,
            .TemporalSampleIndex = m_rendererNew->FrameIndex};
        auto ViewportRenderResult = m_rendererNew->Runtime->RenderSceneToTarget(SnAPI::Renderer::RenderSceneToTargetDesc{
            .Scene = m_rendererNew->Scene,
            .Target = TargetIt->second.Target,
            .View = View,
            .Profile = SnAPI::Renderer::ForwardProfile::Id,
            .FrameGraphOutputResourceName = "PresentTarget"});
        if (ViewportRenderResult.Failed())
        {
            std::cerr << "RendererSystem failed during RenderSceneToTarget for viewport " << ViewportID
                      << ": " << ViewportRenderResult.Error().Message << '\n';
        }
    }

    FlushQueuedText();
#if defined(SNAPI_GF_ENABLE_UI)
    FlushQueuedUiPackets();
#endif

    const auto View = SnAPI::Renderer::RenderView{
        .DebugName = "RendererSystem.MainView",
        .PlatformPresentationExtent = ViewExtents.PlatformPresentationExtent,
        .OutputExtent = ViewExtents.OutputExtent,
        .InternalRenderExtent = ViewExtents.InternalRenderExtent,
        .Width = ViewExtents.PlatformPresentationExtent.Width,
        .Height = ViewExtents.PlatformPresentationExtent.Height,
        .CameraRelative = true,
        .ReverseZ = true,
        .NearPlane = 0.01,
        .Timing = FrameTiming,
        .TemporalViewKey = "MainView",
        .TemporalHistoryValid = m_rendererNew->FrameIndex > 0u,
        .TemporalSampleIndex = m_rendererNew->FrameIndex};
    auto RenderResult = m_rendererNew->Runtime->RenderSceneToSurface(SnAPI::Renderer::RenderSceneToSurfaceDesc{
        .Scene = m_rendererNew->Scene,
        .Surface = m_rendererNew->Surface,
        .View = View,
        .Profile = SnAPI::Renderer::DeferredProfile::Id,
        .FrameGraphOutputResourceName = "PresentTarget"});
    if (RenderResult.Failed())
    {
        std::cerr << "RendererSystem failed during RenderSceneToSurface: " << RenderResult.Error().Message << '\n';
        (void)m_rendererNew->Runtime->EndFrame();
        m_initialized = false;
        return;
    }

    auto EndFrameResult = m_rendererNew->Runtime->EndFrame();
    if (EndFrameResult.Failed())
    {
        std::cerr << "RendererSystem failed during EndFrame: " << EndFrameResult.Error().Message << '\n';
        m_initialized = false;
        return;
    }

    m_rendererNew->FrameIndex = FrameIndex;
    ++m_taaFrameIndex;
}

bool RendererSystem::EnsureDefaultMaterials()
{
    return m_initialized;
}

bool RendererSystem::EnsureLightManagerInternal()
{
    return false;
}

bool RendererSystem::EnsureDefaultLighting()
{
    return false;
}

bool RendererSystem::EnsureDefaultEnvironmentProbe()
{
    return false;
}

bool RendererSystem::EnsureDefaultFont()
{
    return m_settings.PreloadDefaultFont ? LoadDefaultFont(m_settings.DefaultFontPath, m_settings.DefaultFontSize) : true;
}

bool RendererSystem::HandleWindowResizeIfNeeded()
{
    return false;
}

void RendererSystem::FlushQueuedText()
{
    m_textQueue.clear();
}

bool RendererSystem::CreateWindowResources()
{
    if (!m_rendererNew || !m_rendererNew->Runtime)
    {
        return false;
    }

    if (!m_settings.CreateWindow)
    {
        return true;
    }

#if defined(SNAPI_RENDERER_WITH_PLATFORM_XLIB)
    const auto Width = ResolveWindowPixelExtent(m_settings.WindowWidth);
    const auto Height = ResolveWindowPixelExtent(m_settings.WindowHeight);
    auto WindowResult = SnAPI::Renderer::XlibWindow::Create(SnAPI::Renderer::XlibWindowCreateInfo{
        .Width = Width,
        .Height = Height,
        .Title = m_settings.WindowTitle});
    if (WindowResult.Failed())
    {
        std::cerr << "RendererSystem failed to create Xlib window: " << WindowResult.Error().Message << '\n';
        return false;
    }

    auto Window = std::move(WindowResult).Value();
    auto SurfaceResult = m_rendererNew->Runtime->CreateSurface(SnAPI::Renderer::RenderSurfaceCreateInfo{
        .Window = Window->Window(),
        .NativeSurface = Window->NativeSurface(),
        .VSync = true,
        .HDR = false,
        .BufferCount = 3,
        .DebugName = "RendererSystem.Surface"});
    if (SurfaceResult.Failed())
    {
        std::cerr << "RendererSystem failed to create renderer surface: " << SurfaceResult.Error().Message << '\n';
        return false;
    }

    const auto Surface = SurfaceResult.Value();
    auto PresentationProfileResult = m_rendererNew->Runtime->FindSurfacePresentationProfile(Surface);
    if (PresentationProfileResult.Failed())
    {
        std::cerr << "RendererSystem failed to resolve surface presentation profile: "
                  << PresentationProfileResult.Error().Message << '\n';
        return false;
    }

    const auto ViewExtents = SnAPI::Renderer::RenderResolutionSettings{}.Resolve(
        SnAPI::Renderer::Extent2D{.Width = Window->Width(), .Height = Window->Height()});
    const auto PresentationProfile = PresentationProfileResult.Value();
    auto ConfigureResult = m_rendererNew->Runtime->ConfigureFramePipeline(
        BuildRendererNewFramePipelineSettings(ViewExtents, PresentationProfile));
    if (ConfigureResult.Failed())
    {
        std::cerr << "RendererSystem failed to configure frame pipeline: " << ConfigureResult.Error().Message << '\n';
        return false;
    }

    m_rendererNew->XlibWindow = std::move(Window);
    m_rendererNew->Surface = Surface;
    m_rendererNew->PresentationProfile = PresentationProfile;
    m_rendererNew->ViewExtents = ViewExtents;
    m_rendererNew->SurfaceValid = true;
    return true;
#else
    std::cerr << "RendererSystem cannot create a window because Xlib platform support is unavailable.\n";
    return false;
#endif
}

bool RendererSystem::RegisterDefaultPassGraph()
{
    return RegisterRenderViewportPassGraphUnlocked(1u, ERenderViewportPassGraphPreset::DefaultWorld, true);
}

void RendererSystem::ResetPassPointers()
{
    m_ssaoPass = nullptr;
    m_ssrPass = nullptr;
    m_bloomPass = nullptr;
    m_gbufferPass = nullptr;
    m_passGraphRegistered = false;
}

Graphics::LightManager* RendererSystem::LightManager()
{
    return nullptr;
}

const Graphics::LightManager* RendererSystem::LightManager() const
{
    return nullptr;
}

Graphics::LightManager* RendererSystem::EnsureLightManager()
{
    return nullptr;
}
} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER_NEW
