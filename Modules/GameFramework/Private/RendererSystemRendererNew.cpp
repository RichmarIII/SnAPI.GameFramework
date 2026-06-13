#include "RendererSystem.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include "Profiling.h"
#include "Renderer.h"
#include "RenderAssets/MeshRuntimeAssets.h"
#include "Rendering/GameRenderMesh.h"
#include "Rendering/GameRenderObject.h"
#include "Rendering/GameRenderOutput.h"
#if defined(SNAPI_RENDERER_WITH_PLATFORM_WAYLAND)
#include "Platform/Linux/WaylandWindow.h"
#endif
#if defined(SNAPI_RENDERER_WITH_PLATFORM_XLIB)
#include "Platform/Linux/XlibWindow.h"
#endif

#include <UIContext.h>
#include <UIPacketWriter.h>
#include <UIRenderPackets.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

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

enum class ERendererNewHostWindowSystem : std::uint8_t
{
    Xlib = 0,
    Wayland
};

struct RendererNewHostWindow
{
    ERendererNewHostWindowSystem WindowSystem = ERendererNewHostWindowSystem::Xlib;
#if defined(SNAPI_RENDERER_WITH_PLATFORM_XLIB)
    std::unique_ptr<SnAPI::Renderer::XlibWindow> Xlib{};
#endif
#if defined(SNAPI_RENDERER_WITH_PLATFORM_WAYLAND)
    std::unique_ptr<SnAPI::Renderer::WaylandWindow> Wayland{};
#endif

    [[nodiscard]] bool Valid() const noexcept
    {
#if defined(SNAPI_RENDERER_WITH_PLATFORM_WAYLAND)
        if (WindowSystem == ERendererNewHostWindowSystem::Wayland)
        {
            return Wayland != nullptr;
        }
#endif
#if defined(SNAPI_RENDERER_WITH_PLATFORM_XLIB)
        return Xlib != nullptr;
#else
        return false;
#endif
    }

    [[nodiscard]] std::uint32_t Width() const noexcept
    {
#if defined(SNAPI_RENDERER_WITH_PLATFORM_WAYLAND)
        if (WindowSystem == ERendererNewHostWindowSystem::Wayland && Wayland)
        {
            return Wayland->Width();
        }
#endif
#if defined(SNAPI_RENDERER_WITH_PLATFORM_XLIB)
        return Xlib ? Xlib->Width() : 1u;
#else
        return 1u;
#endif
    }

    [[nodiscard]] std::uint32_t Height() const noexcept
    {
#if defined(SNAPI_RENDERER_WITH_PLATFORM_WAYLAND)
        if (WindowSystem == ERendererNewHostWindowSystem::Wayland && Wayland)
        {
            return Wayland->Height();
        }
#endif
#if defined(SNAPI_RENDERER_WITH_PLATFORM_XLIB)
        return Xlib ? Xlib->Height() : 1u;
#else
        return 1u;
#endif
    }

    [[nodiscard]] SnAPI::Renderer::PlatformWindowHandle Window() const noexcept
    {
#if defined(SNAPI_RENDERER_WITH_PLATFORM_WAYLAND)
        if (WindowSystem == ERendererNewHostWindowSystem::Wayland && Wayland)
        {
            return Wayland->Window();
        }
#endif
#if defined(SNAPI_RENDERER_WITH_PLATFORM_XLIB)
        return Xlib ? Xlib->Window() : 0u;
#else
        return 0u;
#endif
    }

    [[nodiscard]] SnAPI::Renderer::RenderNativeSurfaceInfo NativeSurface() const noexcept
    {
#if defined(SNAPI_RENDERER_WITH_PLATFORM_WAYLAND)
        if (WindowSystem == ERendererNewHostWindowSystem::Wayland && Wayland)
        {
            return Wayland->NativeSurface();
        }
#endif
#if defined(SNAPI_RENDERER_WITH_PLATFORM_XLIB)
        return Xlib ? Xlib->NativeSurface() : SnAPI::Renderer::RenderNativeSurfaceInfo{};
#else
        return {};
#endif
    }

    [[nodiscard]] bool ShouldClose() const noexcept
    {
#if defined(SNAPI_RENDERER_WITH_PLATFORM_WAYLAND)
        if (WindowSystem == ERendererNewHostWindowSystem::Wayland && Wayland)
        {
            return Wayland->ShouldClose();
        }
#endif
#if defined(SNAPI_RENDERER_WITH_PLATFORM_XLIB)
        return Xlib == nullptr || Xlib->ShouldClose();
#else
        return true;
#endif
    }

    void PumpEvents()
    {
#if defined(SNAPI_RENDERER_WITH_PLATFORM_WAYLAND)
        if (WindowSystem == ERendererNewHostWindowSystem::Wayland && Wayland)
        {
            Wayland->PumpEvents();
            return;
        }
#endif
#if defined(SNAPI_RENDERER_WITH_PLATFORM_XLIB)
        if (Xlib)
        {
            Xlib->PumpEvents();
        }
#endif
    }
};

[[nodiscard]] static const char* RendererNewHostWindowSystemName(const ERendererNewHostWindowSystem WindowSystem)
{
    switch (WindowSystem)
    {
    case ERendererNewHostWindowSystem::Wayland:
        return "Wayland";
    case ERendererNewHostWindowSystem::Xlib:
    default:
        return "Xlib";
    }
}

[[nodiscard]] static bool EnvironmentLooksLikeWaylandSession() noexcept
{
    const char* WaylandDisplay = std::getenv("WAYLAND_DISPLAY");
    if (WaylandDisplay != nullptr && WaylandDisplay[0] != '\0')
    {
        return true;
    }

    const char* SessionType = std::getenv("XDG_SESSION_TYPE");
    if (SessionType == nullptr)
    {
        return false;
    }

    std::string Normalized = SessionType;
    std::ranges::transform(
        Normalized,
        Normalized.begin(),
        [](const unsigned char Character) { return static_cast<char>(std::tolower(Character)); });
    return Normalized == "wayland";
}

[[nodiscard]] static std::string ReadRendererNewLinuxWindowSystemPreference()
{
    const char* ExplicitPreference = std::getenv("SNAPI_RENDERER_NEW_LINUX_WINDOW_SYSTEM");
    if (ExplicitPreference == nullptr || ExplicitPreference[0] == '\0')
    {
        return "auto";
    }

    std::string Normalized = ExplicitPreference;
    std::ranges::transform(
        Normalized,
        Normalized.begin(),
        [](const unsigned char Character) { return static_cast<char>(std::tolower(Character)); });
    return Normalized;
}

[[nodiscard]] static SnAPI::Renderer::TResult<RendererNewHostWindow> CreateRendererNewHostWindow(
    const std::uint32_t Width,
    const std::uint32_t Height,
    const std::string& Title)
{
    const auto CreateXlibWindow = [Width, Height, &Title]() -> SnAPI::Renderer::TResult<RendererNewHostWindow>
    {
#if defined(SNAPI_RENDERER_WITH_PLATFORM_XLIB)
        auto WindowResult = SnAPI::Renderer::XlibWindow::Create(SnAPI::Renderer::XlibWindowCreateInfo{
            .Width = Width,
            .Height = Height,
            .Title = Title});
        if (WindowResult.Failed())
        {
            return WindowResult.Error();
        }

        RendererNewHostWindow Window;
        Window.WindowSystem = ERendererNewHostWindowSystem::Xlib;
        Window.Xlib = std::move(WindowResult).Value();
        return SnAPI::Renderer::TResult<RendererNewHostWindow>{std::move(Window)};
#else
        return SnAPI::Renderer::RendererError{
            .Code = SnAPI::Renderer::ERendererErrorCode::UnsupportedOperation,
            .Message = "Xlib platform window support is not enabled in this build."};
#endif
    };

    const auto CreateWaylandWindow = [Width, Height, &Title]() -> SnAPI::Renderer::TResult<RendererNewHostWindow>
    {
#if defined(SNAPI_RENDERER_WITH_PLATFORM_WAYLAND)
        auto WindowResult = SnAPI::Renderer::WaylandWindow::Create(SnAPI::Renderer::WaylandWindowCreateInfo{
            .Width = Width,
            .Height = Height,
            .Title = Title});
        if (WindowResult.Failed())
        {
            return WindowResult.Error();
        }

        RendererNewHostWindow Window;
        Window.WindowSystem = ERendererNewHostWindowSystem::Wayland;
        Window.Wayland = std::move(WindowResult).Value();
        return SnAPI::Renderer::TResult<RendererNewHostWindow>{std::move(Window)};
#else
        return SnAPI::Renderer::RendererError{
            .Code = SnAPI::Renderer::ERendererErrorCode::UnsupportedOperation,
            .Message = "Wayland platform window support is not enabled in this build."};
#endif
    };

    const auto Preference = ReadRendererNewLinuxWindowSystemPreference();
    if (Preference == "wayland")
    {
        return CreateWaylandWindow();
    }

    if (Preference == "xlib" || Preference == "x11")
    {
        return CreateXlibWindow();
    }

    if (EnvironmentLooksLikeWaylandSession())
    {
        auto WaylandWindowResult = CreateWaylandWindow();
        if (WaylandWindowResult.Succeeded())
        {
            return WaylandWindowResult;
        }

        if (WaylandWindowResult.Error().Code != SnAPI::Renderer::ERendererErrorCode::UnsupportedOperation)
        {
            return WaylandWindowResult.Error();
        }
    }

    auto XlibWindowResult = CreateXlibWindow();
    if (XlibWindowResult.Succeeded())
    {
        return XlibWindowResult;
    }

    if (EnvironmentLooksLikeWaylandSession())
    {
        return XlibWindowResult.Error();
    }

    return CreateWaylandWindow();
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
    Settings.GlobalIllumination = {};
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

[[nodiscard]] static bool ShouldLogViewportOverlaySizes() noexcept
{
    const char* Value = std::getenv("SNAPI_GF_LOG_VIEWPORT_OVERLAY_SIZES");
    return Value != nullptr && Value[0] != '\0' && Value[0] != '0';
}

[[nodiscard]] static std::uint64_t CombineRendererNewSourceKey(
    const std::uint64_t Left,
    const std::uint64_t Right) noexcept
{
    return Left ^ (Right + 0x9e3779b97f4a7c15ull + (Left << 6u) + (Left >> 2u));
}

[[nodiscard]] static std::uint64_t MixRendererNewSourceKey(
    const std::uint64_t Seed,
    const std::uint64_t Value) noexcept
{
    return CombineRendererNewSourceKey(Seed, Value);
}

[[nodiscard]] static std::uint64_t ComputeRendererNewRuntimeMeshSignature(
    const RuntimeMeshData& MeshData) noexcept
{
    std::uint64_t Signature = CombineRendererNewSourceKey(MeshData.SourceId, MeshData.SourceRevision);
    Signature = MixRendererNewSourceKey(Signature, MeshData.VertexCount);
    Signature = MixRendererNewSourceKey(Signature, MeshData.Indices.size());
    Signature = MixRendererNewSourceKey(Signature, MeshData.Streams.size());
    for (const RuntimeMeshStream& Stream : MeshData.Streams)
    {
        Signature = MixRendererNewSourceKey(Signature, static_cast<std::uint64_t>(Stream.Semantic));
        Signature = MixRendererNewSourceKey(Signature, Stream.ElementCount);
        Signature = MixRendererNewSourceKey(Signature, Stream.StrideBytes);
        Signature = MixRendererNewSourceKey(Signature, Stream.Bytes.size());
    }
    return Signature == 0u ? 1u : Signature;
}

[[nodiscard]] static std::uint64_t ComputeRendererNewPrimitiveMeshSignature(
    const SnAPI::Renderer::PrimitiveMeshData& MeshData) noexcept
{
    std::uint64_t Signature = MeshData.GeometrySignature;
    Signature = MixRendererNewSourceKey(Signature, MeshData.Vertices.size());
    Signature = MixRendererNewSourceKey(Signature, MeshData.Indices.size());
    return Signature == 0u ? 1u : Signature;
}

[[nodiscard]] static const RuntimeMeshStream* FindRuntimeMeshStream(
    const RuntimeMeshData& MeshData,
    const EMeshStreamSemantic Semantic) noexcept
{
    const auto It = std::ranges::find_if(MeshData.Streams, [Semantic](const RuntimeMeshStream& Stream) {
        return Stream.Semantic == Semantic;
    });
    return It == MeshData.Streams.end() ? nullptr : &(*It);
}

template <std::size_t ComponentCount>
[[nodiscard]] static bool ReadRuntimeMeshFloatComponents(
    const RuntimeMeshStream& Stream,
    const std::uint32_t VertexIndex,
    std::array<float, ComponentCount>& OutValue) noexcept
{
    if (Stream.ElementCount <= VertexIndex)
    {
        return false;
    }

    const std::size_t RequiredBytes = sizeof(float) * ComponentCount;
    const std::size_t StrideBytes = Stream.StrideBytes == 0u ? RequiredBytes : Stream.StrideBytes;
    if (StrideBytes < RequiredBytes)
    {
        return false;
    }

    const std::size_t Offset = static_cast<std::size_t>(VertexIndex) * StrideBytes;
    if (Offset + RequiredBytes > Stream.Bytes.size())
    {
        return false;
    }

    std::memcpy(OutValue.data(), Stream.Bytes.data() + Offset, RequiredBytes);
    return true;
}

struct RendererNewMeshBuildResult
{
    SnAPI::Renderer::PrimitiveMeshData MeshData{};
    SnAPI::Renderer::StaticMeshSurfaceData SurfaceData{};
    SnAPI::Renderer::Point3 BoundsCenter{SnAPI::Renderer::Point3::Zero()};
    SnAPI::Renderer::Point3 BoundsMin{SnAPI::Renderer::Point3::Zero()};
    SnAPI::Renderer::Point3 BoundsMax{SnAPI::Renderer::Point3::Zero()};
    double BoundsRadius{0.0};
    bool HasBounds{false};
};

[[nodiscard]] static bool FinalizeRendererNewSurfaceMeshData(RendererNewMeshBuildResult& Result)
{
    if (Result.MeshData.Vertices.empty())
    {
        return false;
    }

    Result.SurfaceData.Vertices.clear();
    Result.SurfaceData.Vertices.reserve(Result.MeshData.Vertices.size());
    Result.SurfaceData.Indices = Result.MeshData.Indices;

    float MinX = std::numeric_limits<float>::max();
    float MinY = std::numeric_limits<float>::max();
    float MinZ = std::numeric_limits<float>::max();
    float MaxX = std::numeric_limits<float>::lowest();
    float MaxY = std::numeric_limits<float>::lowest();
    float MaxZ = std::numeric_limits<float>::lowest();

    for (const SnAPI::Renderer::PrimitiveMeshVertex& Vertex : Result.MeshData.Vertices)
    {
        const auto& Position = Vertex.PositionLocal;
        MinX = std::min(MinX, Position[0]);
        MinY = std::min(MinY, Position[1]);
        MinZ = std::min(MinZ, Position[2]);
        MaxX = std::max(MaxX, Position[0]);
        MaxY = std::max(MaxY, Position[1]);
        MaxZ = std::max(MaxZ, Position[2]);

        Result.SurfaceData.Vertices.push_back(SnAPI::Renderer::StaticMeshSurfaceVertex{
            .PositionLocal = Vertex.PositionLocal,
            .NormalLocal = Vertex.NormalLocal,
            .TangentLocalAndSign = Vertex.TangentLocalAndSign,
            .SurfaceBaseColorLinear = Vertex.SurfaceBaseColorLinear,
            .SurfaceUv = Vertex.SurfaceUv,
            .SurfaceMaterialParams = Vertex.SurfaceMaterialParams,
            .SurfaceEmissiveLinear = Vertex.SurfaceEmissiveLinear});
    }

    Result.BoundsMin << static_cast<double>(MinX), static_cast<double>(MinY), static_cast<double>(MinZ);
    Result.BoundsMax << static_cast<double>(MaxX), static_cast<double>(MaxY), static_cast<double>(MaxZ);
    Result.BoundsCenter << (static_cast<double>(MinX) + static_cast<double>(MaxX)) * 0.5,
        (static_cast<double>(MinY) + static_cast<double>(MaxY)) * 0.5,
        (static_cast<double>(MinZ) + static_cast<double>(MaxZ)) * 0.5;
    Result.BoundsRadius = 0.0;
    for (const auto& Vertex : Result.MeshData.Vertices)
    {
        const double X = static_cast<double>(Vertex.PositionLocal[0]) - Result.BoundsCenter[0];
        const double Y = static_cast<double>(Vertex.PositionLocal[1]) - Result.BoundsCenter[1];
        const double Z = static_cast<double>(Vertex.PositionLocal[2]) - Result.BoundsCenter[2];
        Result.BoundsRadius = std::max(Result.BoundsRadius, std::sqrt((X * X) + (Y * Y) + (Z * Z)));
    }
    Result.HasBounds = true;
    return true;
}

[[nodiscard]] static std::optional<RendererNewMeshBuildResult> BuildRendererNewSurfaceMeshData(
    const RuntimeMeshData& Source)
{
    const RuntimeMeshStream* PositionStream = FindRuntimeMeshStream(Source, EMeshStreamSemantic::Position);
    if (!PositionStream)
    {
        return std::nullopt;
    }

    const std::uint32_t VertexCount = Source.VertexCount != 0u ? Source.VertexCount : PositionStream->ElementCount;
    if (VertexCount == 0u)
    {
        return std::nullopt;
    }

    const RuntimeMeshStream* NormalStream = FindRuntimeMeshStream(Source, EMeshStreamSemantic::Normal);
    const RuntimeMeshStream* TangentStream = FindRuntimeMeshStream(Source, EMeshStreamSemantic::Tangent);
    const RuntimeMeshStream* UvStream = FindRuntimeMeshStream(Source, EMeshStreamSemantic::UV0);
    const RuntimeMeshStream* ColorStream = FindRuntimeMeshStream(Source, EMeshStreamSemantic::Color);

    RendererNewMeshBuildResult Result{};
    Result.MeshData.DebugName = Source.DebugName;
    Result.MeshData.GeometrySignature = ComputeRendererNewRuntimeMeshSignature(Source);
    Result.MeshData.Vertices.reserve(VertexCount);

    for (std::uint32_t VertexIndex = 0u; VertexIndex < VertexCount; ++VertexIndex)
    {
        std::array<float, 3> Position{};
        if (!ReadRuntimeMeshFloatComponents(*PositionStream, VertexIndex, Position))
        {
            return std::nullopt;
        }

        SnAPI::Renderer::PrimitiveMeshVertex Vertex{};
        Vertex.PositionLocal = {Position[0], Position[1], Position[2], 1.0f};

        if (NormalStream)
        {
            std::array<float, 3> Normal{};
            if (ReadRuntimeMeshFloatComponents(*NormalStream, VertexIndex, Normal))
            {
                Vertex.NormalLocal = {Normal[0], Normal[1], Normal[2], 0.0f};
            }
        }

        if (TangentStream)
        {
            std::array<float, 4> Tangent{};
            if (ReadRuntimeMeshFloatComponents(*TangentStream, VertexIndex, Tangent))
            {
                Vertex.TangentLocalAndSign = Tangent;
            }
        }

        if (UvStream)
        {
            std::array<float, 2> Uv{};
            if (ReadRuntimeMeshFloatComponents(*UvStream, VertexIndex, Uv))
            {
                Vertex.SurfaceUv = Uv;
            }
        }

        if (ColorStream)
        {
            std::array<float, 4> Color{};
            if (ReadRuntimeMeshFloatComponents(*ColorStream, VertexIndex, Color))
            {
                Vertex.SurfaceBaseColorLinear = Color;
            }
            else
            {
                std::array<float, 3> ColorRgb{};
                if (ReadRuntimeMeshFloatComponents(*ColorStream, VertexIndex, ColorRgb))
                {
                    Vertex.SurfaceBaseColorLinear = {ColorRgb[0], ColorRgb[1], ColorRgb[2], 1.0f};
                }
            }
        }

        Result.MeshData.Vertices.push_back(Vertex);
    }

    Result.MeshData.Indices = Source.Indices;
    if (!FinalizeRendererNewSurfaceMeshData(Result))
    {
        return std::nullopt;
    }
    return Result;
}

[[nodiscard]] static std::optional<RendererNewMeshBuildResult> BuildRendererNewSurfaceMeshData(
    SnAPI::Renderer::PrimitiveMeshData Source)
{
    if (Source.Vertices.empty())
    {
        return std::nullopt;
    }

    RendererNewMeshBuildResult Result{};
    Result.MeshData = std::move(Source);
    Result.MeshData.GeometrySignature = ComputeRendererNewPrimitiveMeshSignature(Result.MeshData);
    if (!FinalizeRendererNewSurfaceMeshData(Result))
    {
        return std::nullopt;
    }
    return Result;
}

struct RendererNewUploadedMeshResources
{
    SnAPI::Renderer::MeshHandle Mesh{};
    SnAPI::Renderer::BufferHandle VertexBuffer{};
    SnAPI::Renderer::BufferHandle IndexBuffer{};
    Vec3 LocalBoundsCenter{Vec3::Zero()};
    Vec3 LocalBoundsMin{Vec3::Zero()};
    Vec3 LocalBoundsMax{Vec3::Zero()};
    double LocalBoundsRadius{0.0};
    bool HasLocalBounds{false};
    std::string DebugName{};
};

[[nodiscard]] static Vec3 ToGameVec3(const SnAPI::Renderer::Point3& Value)
{
    return Vec3{
        static_cast<Vec3::Scalar>(Value[0]),
        static_cast<Vec3::Scalar>(Value[1]),
        static_cast<Vec3::Scalar>(Value[2])};
}

[[nodiscard]] static SnAPI::Renderer::Point3 ToRendererPoint3(const Vec3& Value)
{
    return SnAPI::Renderer::Point3{
        static_cast<double>(Value.x()),
        static_cast<double>(Value.y()),
        static_cast<double>(Value.z())};
}

[[nodiscard]] static std::optional<RendererNewUploadedMeshResources> UploadRendererNewStaticMesh(
    SnAPI::Renderer::RendererRuntime& Runtime,
    RendererNewMeshBuildResult MeshBuild,
    const std::string_view DebugName)
{
    auto& MeshData = MeshBuild.MeshData;
    if (!DebugName.empty())
    {
        MeshData.DebugName = std::string(DebugName);
    }
    if (MeshData.DebugName.empty())
    {
        MeshData.DebugName = "GameRenderMesh";
    }
    if (MeshData.GeometrySignature == 0u)
    {
        MeshData.GeometrySignature = ComputeRendererNewPrimitiveMeshSignature(MeshData);
    }

    auto VertexBufferResult = Runtime.CreateBuffer(SnAPI::Renderer::BufferDesc{
        .SizeBytes = MeshData.VertexBufferSizeBytes(),
        .Usage = SnAPI::Renderer::EBufferUsage::VertexBuffer,
        .MemoryClass = SnAPI::Renderer::EMemoryClass::Upload,
        .DebugName = MeshData.DebugName + ".VertexBuffer"});
    if (VertexBufferResult.Failed())
    {
        std::cerr << "RendererSystem failed to create Renderer.New mesh vertex buffer: "
                  << VertexBufferResult.Error().Message << '\n';
        return std::nullopt;
    }

    auto VertexUploadResult = Runtime.Device().WriteBuffer(
        VertexBufferResult.Value(),
        std::as_bytes(std::span<const SnAPI::Renderer::PrimitiveMeshVertex>(MeshData.Vertices)));
    if (VertexUploadResult.Failed())
    {
        std::cerr << "RendererSystem failed to upload Renderer.New mesh vertex data: "
                  << VertexUploadResult.Error().Message << '\n';
        (void)Runtime.DestroyBuffer(VertexBufferResult.Value());
        return std::nullopt;
    }

    SnAPI::Renderer::BufferHandle IndexBuffer{};
    if (!MeshData.Indices.empty())
    {
        auto IndexBufferResult = Runtime.CreateBuffer(SnAPI::Renderer::BufferDesc{
            .SizeBytes = MeshData.IndexBufferSizeBytes(),
            .Usage = SnAPI::Renderer::EBufferUsage::IndexBuffer,
            .MemoryClass = SnAPI::Renderer::EMemoryClass::Upload,
            .DebugName = MeshData.DebugName + ".IndexBuffer"});
        if (IndexBufferResult.Failed())
        {
            std::cerr << "RendererSystem failed to create Renderer.New mesh index buffer: "
                      << IndexBufferResult.Error().Message << '\n';
            (void)Runtime.DestroyBuffer(VertexBufferResult.Value());
            return std::nullopt;
        }

        auto IndexUploadResult = Runtime.Device().WriteBuffer(
            IndexBufferResult.Value(),
            std::as_bytes(std::span<const std::uint32_t>(MeshData.Indices)));
        if (IndexUploadResult.Failed())
        {
            std::cerr << "RendererSystem failed to upload Renderer.New mesh index data: "
                      << IndexUploadResult.Error().Message << '\n';
            (void)Runtime.DestroyBuffer(IndexBufferResult.Value());
            (void)Runtime.DestroyBuffer(VertexBufferResult.Value());
            return std::nullopt;
        }
        IndexBuffer = IndexBufferResult.Value();
    }

    auto MeshResult = Runtime.Meshes().CreateStaticMesh(SnAPI::Renderer::StaticMeshDesc{
        .DebugName = MeshData.DebugName,
        .VertexBuffer = VertexBufferResult.Value(),
        .VertexBuffers = {VertexBufferResult.Value()},
        .IndexBuffer = IndexBuffer,
        .GeometrySignature = MeshData.GeometrySignature,
        .VertexCount = static_cast<std::uint32_t>(MeshData.Vertices.size()),
        .PrimitiveTopology = SnAPI::Renderer::EPrimitiveTopology::TriangleList,
        .IndexFormat = SnAPI::Renderer::EIndexFormat::UInt32,
        .IndexCount = static_cast<std::uint32_t>(MeshData.Indices.size()),
        .VertexStrideBytes = static_cast<std::uint32_t>(sizeof(SnAPI::Renderer::PrimitiveMeshVertex)),
        .VertexLayout = SnAPI::Renderer::PrimitiveGeometry::SurfaceVertexLayout(),
        .LocalBoundsCenter = MeshBuild.BoundsCenter,
        .LocalBoundsRadius = MeshBuild.BoundsRadius,
        .LocalBoundsMin = MeshBuild.BoundsMin,
        .LocalBoundsMax = MeshBuild.BoundsMax,
        .HasLocalBoundsBox = MeshBuild.HasBounds,
        .ObjectPassTechniques = {},
        .SurfaceData = std::move(MeshBuild.SurfaceData)});
    if (MeshResult.Failed())
    {
        std::cerr << "RendererSystem failed to register Renderer.New static mesh: "
                  << MeshResult.Error().Message << '\n';
        if (IndexBuffer.Valid())
        {
            (void)Runtime.DestroyBuffer(IndexBuffer);
        }
        (void)Runtime.DestroyBuffer(VertexBufferResult.Value());
        return std::nullopt;
    }

    return RendererNewUploadedMeshResources{
        .Mesh = MeshResult.Value(),
        .VertexBuffer = VertexBufferResult.Value(),
        .IndexBuffer = IndexBuffer,
        .LocalBoundsCenter = ToGameVec3(MeshBuild.BoundsCenter),
        .LocalBoundsMin = ToGameVec3(MeshBuild.BoundsMin),
        .LocalBoundsMax = ToGameVec3(MeshBuild.BoundsMax),
        .LocalBoundsRadius = MeshBuild.BoundsRadius,
        .HasLocalBounds = MeshBuild.HasBounds,
        .DebugName = MeshData.DebugName};
}

[[nodiscard]] static SnAPI::Renderer::RenderFeatureChannelMask BuildGameObjectFeatureChannels(
    const bool CastShadows) noexcept
{
    auto Channels = SnAPI::Renderer::RenderFeatureChannelMask::From(SnAPI::Renderer::ERenderFeatureChannel::DynamicLighting)
        .With(SnAPI::Renderer::ERenderFeatureChannel::DepthPrepass)
        .With(SnAPI::Renderer::ERenderFeatureChannel::MotionVectors)
        .With(SnAPI::Renderer::ERenderFeatureChannel::GlobalIllumination);
    if (CastShadows)
    {
        Channels.Set(SnAPI::Renderer::ERenderFeatureChannel::StaticShadowCache);
    }
    return Channels;
}

[[nodiscard]] static bool DestroyRendererNewMeshResources(
    SnAPI::Renderer::RendererRuntime& Runtime,
    GameRenderMesh& Mesh)
{
    const bool HadResources = Mesh.Valid() || Mesh.VertexBuffer().Valid() || Mesh.IndexBuffer().Valid();
    if (Mesh.Mesh().Valid())
    {
        if (const auto Result = Runtime.Meshes().DestroyStaticMesh(Mesh.Mesh()); Result.Failed())
        {
            std::cerr << "RendererSystem failed to destroy Renderer.New mesh: " << Result.Error().Message << '\n';
        }
    }
    if (Mesh.VertexBuffer().Valid())
    {
        if (const auto Result = Runtime.DestroyBuffer(Mesh.VertexBuffer()); Result.Failed())
        {
            std::cerr << "RendererSystem failed to destroy Renderer.New mesh vertex buffer: "
                      << Result.Error().Message << '\n';
        }
    }
    if (Mesh.IndexBuffer().Valid())
    {
        if (const auto Result = Runtime.DestroyBuffer(Mesh.IndexBuffer()); Result.Failed())
        {
            std::cerr << "RendererSystem failed to destroy Renderer.New mesh index buffer: "
                      << Result.Error().Message << '\n';
        }
    }
    Mesh.Reset();
    return HadResources;
}

[[nodiscard]] static bool DestroyRendererNewObjectResource(
    SnAPI::Renderer::RenderScene& Scene,
    GameRenderObject& Object)
{
    const bool HadObject = Object.Valid();
    if (Object.Object().Valid())
    {
        if (const auto Result = Scene.DestroyObject(Object.Object()); Result.Failed())
        {
            std::cerr << "RendererSystem failed to destroy Renderer.New object: " << Result.Error().Message << '\n';
        }
    }
    Object.Reset();
    return HadObject;
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

[[nodiscard]] static std::array<float, 4> ToRendererTextColor(const SnAPI::UI::Color& Color)
{
    constexpr float Inv255 = 1.0f / 255.0f;
    return {
        static_cast<float>(Color.R) * Inv255,
        static_cast<float>(Color.G) * Inv255,
        static_cast<float>(Color.B) * Inv255,
        static_cast<float>(Color.A) * Inv255};
}

[[nodiscard]] static SnAPI::Renderer::UiColor ToRendererUiColor(const std::array<float, 4>& Color)
{
    return SnAPI::Renderer::UiColor{Color[0], Color[1], Color[2], Color[3]};
}

[[nodiscard]] static std::string EncodeUtf8Codepoint(std::uint32_t Codepoint)
{
    std::string Result;
    if (Codepoint <= 0x7Fu)
    {
        Result.push_back(static_cast<char>(Codepoint));
    }
    else if (Codepoint <= 0x7FFu)
    {
        Result.push_back(static_cast<char>(0xC0u | (Codepoint >> 6u)));
        Result.push_back(static_cast<char>(0x80u | (Codepoint & 0x3Fu)));
    }
    else if (Codepoint <= 0xFFFFu)
    {
        Result.push_back(static_cast<char>(0xE0u | (Codepoint >> 12u)));
        Result.push_back(static_cast<char>(0x80u | ((Codepoint >> 6u) & 0x3Fu)));
        Result.push_back(static_cast<char>(0x80u | (Codepoint & 0x3Fu)));
    }
    else if (Codepoint <= 0x10FFFFu)
    {
        Result.push_back(static_cast<char>(0xF0u | (Codepoint >> 18u)));
        Result.push_back(static_cast<char>(0x80u | ((Codepoint >> 12u) & 0x3Fu)));
        Result.push_back(static_cast<char>(0x80u | ((Codepoint >> 6u) & 0x3Fu)));
        Result.push_back(static_cast<char>(0x80u | (Codepoint & 0x3Fu)));
    }
    return Result;
}

class RendererNewUiFontMetricsAdapter final : public SnAPI::UI::IFontMetrics
{
public:
    void Bind(
        const SnAPI::Renderer::TextSystem* TextSystem,
        const SnAPI::Renderer::TextFontHandle Font,
        const float SizePixels)
    {
        const float ClampedSize = std::max(1.0f, SizePixels);
        if (m_TextSystem == TextSystem && m_Font == Font && m_SizePixels == ClampedSize)
        {
            return;
        }

        m_TextSystem = TextSystem;
        m_Font = Font;
        m_SizePixels = ClampedSize;
        m_CachedGlyphs.clear();
    }

    const SnAPI::UI::GlyphMetrics* GetGlyph(const std::uint32_t Codepoint) const override
    {
        if (!m_TextSystem || !m_Font.Valid() || Codepoint == 0u)
        {
            return nullptr;
        }

        if (const auto Found = m_CachedGlyphs.find(Codepoint); Found != m_CachedGlyphs.end())
        {
            return &Found->second;
        }

        const std::string Utf8 = EncodeUtf8Codepoint(Codepoint);
        if (Utf8.empty())
        {
            return nullptr;
        }

        const SnAPI::Renderer::TextStyleDesc Style{
            .Font = m_Font,
            .SizePixels = m_SizePixels,
            .RasterizationMode = SnAPI::Renderer::ETextGlyphRasterizationMode::Mtsdf};
        auto ShapedResult = m_TextSystem->ShapeText(SnAPI::Renderer::TextStringSubmissionDesc{
            .TextUtf8 = Utf8,
            .Style = Style});
        if (ShapedResult.Failed() || ShapedResult.Value().Glyphs.empty())
        {
            return nullptr;
        }

        const auto& ShapedGlyph = ShapedResult.Value().Glyphs.front();
        const float Advance = std::max(0.0f, static_cast<float>(ShapedGlyph.Advance.x()));
        SnAPI::UI::GlyphMetrics Metrics{};
        Metrics.Width = Advance;
        Metrics.Height = GetLineHeight();
        Metrics.BearingX = static_cast<float>(ShapedGlyph.Offset.x());
        Metrics.BearingY = -GetAscent();
        Metrics.Advance = Advance;

        auto [It, Inserted] = m_CachedGlyphs.emplace(Codepoint, Metrics);
        (void)Inserted;
        return &It->second;
    }

    float GetLineHeight() const override
    {
        return ResolveFontMetrics().LineHeightPixels;
    }

    float GetAscent() const override
    {
        return ResolveFontMetrics().AscentPixels;
    }

    float GetSizePixels() const override
    {
        return m_SizePixels;
    }

private:
    [[nodiscard]] SnAPI::Renderer::TextResolvedFontMetrics ResolveFontMetrics() const
    {
        if (!m_TextSystem || !m_Font.Valid())
        {
            return SnAPI::Renderer::TextResolvedFontMetrics{
                .AscentPixels = m_SizePixels * 0.75f,
                .DescentPixels = m_SizePixels * 0.25f,
                .LineHeightPixels = m_SizePixels};
        }

        auto MetricsResult = m_TextSystem->ResolveFontMetrics(SnAPI::Renderer::TextStyleDesc{
            .Font = m_Font,
            .SizePixels = m_SizePixels,
            .RasterizationMode = SnAPI::Renderer::ETextGlyphRasterizationMode::Mtsdf});
        if (MetricsResult.Failed())
        {
            return SnAPI::Renderer::TextResolvedFontMetrics{
                .AscentPixels = m_SizePixels * 0.75f,
                .DescentPixels = m_SizePixels * 0.25f,
                .LineHeightPixels = m_SizePixels};
        }
        return MetricsResult.Value();
    }

    const SnAPI::Renderer::TextSystem* m_TextSystem = nullptr;
    SnAPI::Renderer::TextFontHandle m_Font{};
    float m_SizePixels{16.0f};
    mutable std::unordered_map<std::uint32_t, SnAPI::UI::GlyphMetrics> m_CachedGlyphs{};
};

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
    const float OffsetY,
    const float ScaleX,
    const float ScaleY)
{
    return SnAPI::Renderer::UiRect{
        (X - OffsetX) * ScaleX,
        (Y - OffsetY) * ScaleY,
        W * ScaleX,
        H * ScaleY};
}

[[nodiscard]] static bool ToRendererUiScissor(
    const SnAPI::UI::EScissorMode Mode,
    const SnAPI::UI::ScissorRect& Scissor,
    const float OffsetX,
    const float OffsetY,
    const float ScaleX,
    const float ScaleY,
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
            (static_cast<float>(Scissor.X) - OffsetX) * ScaleX,
            (static_cast<float>(Scissor.Y) - OffsetY) * ScaleY,
            static_cast<float>(Scissor.W) * ScaleX,
            static_cast<float>(Scissor.H) * ScaleY};
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

static bool EnsureRendererTextAtlasTextures(
    SnAPI::Renderer::RendererRuntime& Runtime,
    const SnAPI::Renderer::TextFramePacket& FramePacket)
{
    auto BackendPacket = SnAPI::Renderer::TextSystem::BuildBackendPacket(FramePacket);
    BackendPacket.GlyphInstances.clear();
    for (auto& Command : BackendPacket.DrawCommands)
    {
        Command.FirstInstance = 0u;
        Command.InstanceCount = 0u;
    }

    for (const auto& Upload : BackendPacket.AtlasUploads)
    {
        const auto ExistingCommand = std::ranges::find_if(
            BackendPacket.DrawCommands,
            [&Upload](const SnAPI::Renderer::TextBackendDrawCommand& Command)
            {
                return Command.Key.Atlas == Upload.Atlas;
            });
        if (ExistingCommand != BackendPacket.DrawCommands.end())
        {
            continue;
        }

        BackendPacket.DrawCommands.push_back(SnAPI::Renderer::TextBackendDrawCommand{
            .Key = SnAPI::Renderer::TextBatchKey{
                .Atlas = Upload.Atlas,
                .MaterialInstance = SnAPI::Renderer::MaterialInstanceHandle{1u},
                .Domain = SnAPI::Renderer::ETextRenderDomain::Ui,
                .MaterialDomain = SnAPI::Renderer::EMaterialDomain::Ui,
                .RasterizationMode = Upload.Glyph.RasterizationMode},
            .FirstInstance = 0u,
            .InstanceCount = 0u});
    }

    auto UploadResult = Runtime.Text().UploadBackendPacket(Runtime.Device(), BackendPacket);
    if (UploadResult.Failed())
    {
        std::cerr << "RendererSystem failed to upload Renderer.New UI text atlas textures: "
                  << UploadResult.Error().Message << '\n';
        return false;
    }

    return true;
}

static bool AppendRendererUiTextGlyphs(
    SnAPI::Renderer::RendererRuntime& Runtime,
    SnAPI::Renderer::RenderOverlayPacket& OverlayPacket,
    const SnAPI::UI::RenderPacket& Packet,
    const SnAPI::UI::TextInstance& Instance,
    const SnAPI::Renderer::TextFontHandle TextFont,
    const float TextFontSizePixels,
    const SnAPI::Renderer::SamplerHandle PacketSampler,
    const SnAPI::Renderer::UiScissorRect& Scissor,
    const float OffsetX,
    const float OffsetY,
    const float ScaleX,
    const float ScaleY)
{
    if (!TextFont.Valid() || Instance.Text.empty())
    {
        return false;
    }

    SnAPI::Renderer::TextClipRect TextClipRect{};
    if (Scissor.Enabled)
    {
        TextClipRect = SnAPI::Renderer::TextClipRect{
            .MinX = Scissor.Rect.X,
            .MinY = Scissor.Rect.Y,
            .MaxX = Scissor.Rect.X + Scissor.Rect.Width,
            .MaxY = Scissor.Rect.Y + Scissor.Rect.Height};
    }

    const float FallbackSizePixels = TextFontSizePixels * std::max(Instance.Scale, 0.0001f);
    const float SizePixels = std::max(
        1.0f,
        Instance.SizePixels > 0.0f ? Instance.SizePixels : FallbackSizePixels);
    const float BaselineOffsetPixels = Instance.BaselineOffsetPixels > 0.0f
        ? Instance.BaselineOffsetPixels
        : SizePixels * 0.75f;

    const SnAPI::Renderer::TextStringSubmissionDesc Submission{
        .TextUtf8 = Instance.Text,
        .Style = SnAPI::Renderer::TextStyleDesc{
            .Font = TextFont,
            .SizePixels = SizePixels,
            .RasterizationMode = SnAPI::Renderer::ETextGlyphRasterizationMode::Mtsdf},
        .Layout = SnAPI::Renderer::TextLayoutDesc{
            .Origin = SnAPI::Renderer::Vec2{
                static_cast<double>(Instance.X - OffsetX),
                static_cast<double>(Instance.Y - OffsetY + BaselineOffsetPixels)},
            .Domain = SnAPI::Renderer::ETextRenderDomain::Ui,
            .ClipRect = TextClipRect},
        .MaterialInstance = SnAPI::Renderer::MaterialInstanceHandle{1u},
        .Color = ToRendererTextColor(Instance.TextColor)};

    auto FramePacketResult = Runtime.Text().PrepareFrame(SnAPI::Renderer::TextFrameSubmissionDesc{
        .TextSubmissions = std::span<const SnAPI::Renderer::TextStringSubmissionDesc>{&Submission, 1u}});
    if (FramePacketResult.Failed())
    {
        std::cerr << "RendererSystem failed to prepare Renderer.New UI text glyphs: "
                  << FramePacketResult.Error().Message << '\n';
        return false;
    }

    const SnAPI::Renderer::TextFramePacket& FramePacket = FramePacketResult.Value();
    if (!EnsureRendererTextAtlasTextures(Runtime, FramePacket))
    {
        return false;
    }

    for (const auto& Batch : FramePacket.DrawBatches)
    {
        const SnAPI::Renderer::TextureHandle AtlasTexture = Runtime.Text().AtlasTexture(Batch.Key.Atlas);
        if (!AtlasTexture.Valid())
        {
            continue;
        }

        for (const auto& Glyph : Batch.GlyphInstances)
        {
            const float X = static_cast<float>(Glyph.Position.x());
            const float Y = static_cast<float>(Glyph.Position.y());
            const float W = static_cast<float>(Glyph.Size.x());
            const float H = static_cast<float>(Glyph.Size.y());
            const float U0 = static_cast<float>(Glyph.AtlasUvMin.x());
            const float V0 = static_cast<float>(Glyph.AtlasUvMin.y());
            const float U1 = static_cast<float>(Glyph.AtlasUvMax.x());
            const float V1 = static_cast<float>(Glyph.AtlasUvMax.y());
            AppendRendererUiDrawPacket(
                OverlayPacket.Ui,
                MakeRendererUiPacketKey(
                    Packet,
                    SnAPI::Renderer::EUiPacketPipeline::Glyph,
                    AtlasTexture,
                    PacketSampler,
                    Scissor),
                SnAPI::Renderer::UiGlyphInstance{
                    .Rect = SnAPI::Renderer::UiRect{X * ScaleX, Y * ScaleY, W * ScaleX, H * ScaleY},
                    .AtlasUvRect = SnAPI::Renderer::UiRect{U0, V0, U1 - U0, V1 - V0},
                    .Color = ToRendererUiColor(Glyph.Color),
                    .GlyphId = Glyph.Glyph.GlyphId,
                    .Cluster = Glyph.Cluster});
        }
    }

    return true;
}

[[nodiscard]] static SnAPI::Renderer::RenderOverlayPacket BuildRendererUiOverlayPacket(
    SnAPI::Renderer::RendererRuntime& Runtime,
    SnAPI::UI::UIContext& Context,
    const SnAPI::UI::RenderPacketList& Packets,
    const std::function<SnAPI::Renderer::TextureHandle(const SnAPI::UI::UIContext&, std::uint32_t)>& ResolveTexture,
    const SnAPI::Renderer::TextFontHandle TextFont,
    const float TextFontSizePixels,
    const float RenderTargetWidth,
    const float RenderTargetHeight)
{
    SnAPI::Renderer::RenderOverlayPacket OverlayPacket{};
    const auto PacketSpan = Packets.Packets();
    OverlayPacket.Ui.DrawPackets.reserve(PacketSpan.size());

    const auto ContextScreenRect = Context.GetScreenRect();
    const auto ContextViewportSize = Context.GetViewportSize();
    const float OffsetX = ContextScreenRect.X;
    const float OffsetY = ContextScreenRect.Y;
    const float ScaleX = std::isfinite(RenderTargetWidth) && RenderTargetWidth > 0.0f && ContextViewportSize.W > 0.0f
        ? RenderTargetWidth / ContextViewportSize.W
        : 1.0f;
    const float ScaleY = std::isfinite(RenderTargetHeight) && RenderTargetHeight > 0.0f && ContextViewportSize.H > 0.0f
        ? RenderTargetHeight / ContextViewportSize.H
        : 1.0f;
    bool HasUiBounds = false;
    float MinX = std::numeric_limits<float>::max();
    float MinY = std::numeric_limits<float>::max();
    float MaxX = std::numeric_limits<float>::lowest();
    float MaxY = std::numeric_limits<float>::lowest();
    auto IncludeUiBounds = [&](const float X, const float Y, const float W, const float H)
    {
        if (!std::isfinite(X) || !std::isfinite(Y) || !std::isfinite(W) || !std::isfinite(H))
        {
            return;
        }

        HasUiBounds = true;
        MinX = std::min(MinX, X);
        MinY = std::min(MinY, Y);
        MaxX = std::max(MaxX, X + W);
        MaxY = std::max(MaxY, Y + H);
    };

    for (const auto& Packet : PacketSpan)
    {
        SnAPI::Renderer::UiScissorRect Scissor{};
        if (!ToRendererUiScissor(Packet.Key.ScissorMode, Packet.Key.Scissor, OffsetX, OffsetY, ScaleX, ScaleY, Scissor))
        {
            continue;
        }

        const auto PacketSampler = SnAPI::Renderer::SamplerHandle{static_cast<std::uint64_t>(Packet.Key.Sampler.Value)};

        if (const auto* Rects = std::get_if<SnAPI::UI::RectInstanceSpan>(&Packet.Instances))
        {
            for (const auto& Instance : *Rects)
            {
                IncludeUiBounds(Instance.X, Instance.Y, Instance.W, Instance.H);
                AppendRendererUiDrawPacket(
                    OverlayPacket.Ui,
                    MakeRendererUiPacketKey(
                        Packet,
                        SnAPI::Renderer::EUiPacketPipeline::SolidColor,
                        SnAPI::Renderer::TextureHandle{},
                        PacketSampler,
                        Scissor),
                    SnAPI::Renderer::UiRectInstance{
                        .Rect = ToRendererUiRect(Instance.X, Instance.Y, Instance.W, Instance.H, OffsetX, OffsetY, ScaleX, ScaleY),
                        .Fill = ToRendererUiColor(Instance.Fill),
                        .CornerRadius = std::max(0.0f, Instance.CornerRadius),
                        .BorderWidth = std::max(0.0f, Instance.BorderThickness * std::max(ScaleX, ScaleY)),
                        .BorderColor = ToRendererUiColor(Instance.Border)});
            }
            continue;
        }

        if (const auto* Images = std::get_if<SnAPI::UI::ImageInstanceSpan>(&Packet.Instances))
        {
            for (const auto& Instance : *Images)
            {
                IncludeUiBounds(Instance.X, Instance.Y, Instance.W, Instance.H);
                const auto Texture = ResolveTexture(Context, Instance.Texture.Value);
                AppendRendererUiDrawPacket(
                    OverlayPacket.Ui,
                    MakeRendererUiPacketKey(
                        Packet,
                        SnAPI::Renderer::EUiPacketPipeline::Image,
                        Texture,
                        PacketSampler,
                        Scissor),
                    SnAPI::Renderer::UiImageInstance{
                        .Rect = ToRendererUiRect(Instance.X, Instance.Y, Instance.W, Instance.H, OffsetX, OffsetY, ScaleX, ScaleY),
                        .UvRect = SnAPI::Renderer::UiRect{Instance.U0, Instance.V0, Instance.U1 - Instance.U0, Instance.V1 - Instance.V0},
                        .Tint = ToRendererUiColor(Instance.Tint)});
            }
            continue;
        }

        if (const auto* Glyphs = std::get_if<SnAPI::UI::GlyphInstanceSpan>(&Packet.Instances))
        {
            static_cast<void>(Glyphs);
            continue;
        }

        if (const auto* TextRuns = std::get_if<SnAPI::UI::TextInstanceSpan>(&Packet.Instances))
        {
            for (const auto& Instance : *TextRuns)
            {
                (void)AppendRendererUiTextGlyphs(
                    Runtime,
                    OverlayPacket,
                    Packet,
                    Instance,
                    TextFont,
                    TextFontSizePixels,
                    PacketSampler,
                    Scissor,
                    OffsetX,
                    OffsetY,
                    ScaleX,
                    ScaleY);
            }
            continue;
        }

        if (const auto* Gradients = std::get_if<SnAPI::UI::GradientInstanceSpan>(&Packet.Instances))
        {
            for (const auto& Instance : *Gradients)
            {
                IncludeUiBounds(Instance.X, Instance.Y, Instance.W, Instance.H);
                const std::size_t StopCount =
                    std::min<std::size_t>(SnAPI::UI::MaxGradientStops, static_cast<std::size_t>(Instance.Gradient.StopCount));
                const SnAPI::UI::Color StartColor =
                    StopCount > 0u ? Instance.Gradient.Stops[0].StopColor : SnAPI::UI::Color::Transparent();
                const SnAPI::UI::Color EndColor =
                    StopCount > 0u ? Instance.Gradient.Stops[StopCount - 1u].StopColor : SnAPI::UI::Color::Transparent();
                AppendRendererUiDrawPacket(
                    OverlayPacket.Ui,
                    MakeRendererUiPacketKey(
                        Packet,
                        SnAPI::Renderer::EUiPacketPipeline::Gradient,
                        SnAPI::Renderer::TextureHandle{},
                        PacketSampler,
                        Scissor),
                    SnAPI::Renderer::UiGradientInstance{
                        .Rect = ToRendererUiRect(Instance.X, Instance.Y, Instance.W, Instance.H, OffsetX, OffsetY, ScaleX, ScaleY),
                        .StartColor = ToRendererUiColor(StartColor),
                        .EndColor = ToRendererUiColor(EndColor),
                        .StartPoint = SnAPI::Renderer::Vec2{Instance.Gradient.StartX * ScaleX, Instance.Gradient.StartY * ScaleY},
                        .EndPoint = SnAPI::Renderer::Vec2{Instance.Gradient.EndX * ScaleX, Instance.Gradient.EndY * ScaleY},
                        .Kind = SnAPI::Renderer::EUiGradientKind::Linear});
            }
            continue;
        }

        if (const auto* Shadows = std::get_if<SnAPI::UI::ShadowInstanceSpan>(&Packet.Instances))
        {
            for (const auto& Instance : *Shadows)
            {
                IncludeUiBounds(Instance.X, Instance.Y, Instance.W, Instance.H);
                AppendRendererUiDrawPacket(
                    OverlayPacket.Ui,
                    MakeRendererUiPacketKey(
                        Packet,
                        SnAPI::Renderer::EUiPacketPipeline::Shadow,
                        SnAPI::Renderer::TextureHandle{},
                        PacketSampler,
                        Scissor),
                    SnAPI::Renderer::UiShadowInstance{
                        .Rect = ToRendererUiRect(Instance.X, Instance.Y, Instance.W, Instance.H, OffsetX, OffsetY, ScaleX, ScaleY),
                        .Color = ToRendererUiColor(Instance.ShadowColor),
                        .Offset = SnAPI::Renderer::Vec2{0.0f, 0.0f},
                        .BlurRadius = std::max(0.0f, Instance.Blur * std::max(ScaleX, ScaleY)),
                        .Spread = std::max(0.0f, (Instance.Spread + Instance.Expansion) * std::max(ScaleX, ScaleY)),
                        .CornerRadius = std::max(0.0f, Instance.CornerRadius * std::max(ScaleX, ScaleY))});
            }
            continue;
        }

        if (const auto* Triangles = std::get_if<SnAPI::UI::TriangleInstanceSpan>(&Packet.Instances))
        {
            for (const auto& Instance : *Triangles)
            {
                const float BoundsMinX = std::min({Instance.X0, Instance.X1, Instance.X2});
                const float BoundsMinY = std::min({Instance.Y0, Instance.Y1, Instance.Y2});
                IncludeUiBounds(
                    BoundsMinX,
                    BoundsMinY,
                    std::max({Instance.X0, Instance.X1, Instance.X2}) - BoundsMinX,
                    std::max({Instance.Y0, Instance.Y1, Instance.Y2}) - BoundsMinY);
                AppendRendererUiDrawPacket(
                    OverlayPacket.Ui,
                    MakeRendererUiPacketKey(
                        Packet,
                        SnAPI::Renderer::EUiPacketPipeline::Triangle,
                        SnAPI::Renderer::TextureHandle{},
                        PacketSampler,
                        Scissor),
                    SnAPI::Renderer::UiTriangleInstance{
                        .A = SnAPI::Renderer::Vec2{(Instance.X0 - OffsetX) * ScaleX, (Instance.Y0 - OffsetY) * ScaleY},
                        .B = SnAPI::Renderer::Vec2{(Instance.X1 - OffsetX) * ScaleX, (Instance.Y1 - OffsetY) * ScaleY},
                        .C = SnAPI::Renderer::Vec2{(Instance.X2 - OffsetX) * ScaleX, (Instance.Y2 - OffsetY) * ScaleY},
                        .Color = ToRendererUiColor(Instance.Fill)});
            }
            continue;
        }

        if (const auto* Circles = std::get_if<SnAPI::UI::CircleInstanceSpan>(&Packet.Instances))
        {
            for (const auto& Instance : *Circles)
            {
                IncludeUiBounds(
                    Instance.CenterX - Instance.Radius,
                    Instance.CenterY - Instance.Radius,
                    Instance.Radius * 2.0f,
                    Instance.Radius * 2.0f);
                AppendRendererUiDrawPacket(
                    OverlayPacket.Ui,
                    MakeRendererUiPacketKey(
                        Packet,
                        SnAPI::Renderer::EUiPacketPipeline::Circle,
                        SnAPI::Renderer::TextureHandle{},
                        PacketSampler,
                        Scissor),
                    SnAPI::Renderer::UiCircleInstance{
                        .Center = SnAPI::Renderer::Vec2{(Instance.CenterX - OffsetX) * ScaleX, (Instance.CenterY - OffsetY) * ScaleY},
                        .Radius = std::max(0.0f, Instance.Radius * std::max(ScaleX, ScaleY)),
                        .Fill = ToRendererUiColor(Instance.Fill),
                        .BorderWidth = std::max(0.0f, Instance.BorderThickness * std::max(ScaleX, ScaleY)),
                        .BorderColor = ToRendererUiColor(Instance.Border)});
            }
        }
    }

    if (ShouldLogViewportOverlaySizes())
    {
        std::cerr << "[GameFramework][Renderer.New][UIOverlayBuild]"
                  << " contextScreen=(" << ContextScreenRect.X << ',' << ContextScreenRect.Y << ' '
                  << ContextScreenRect.W << 'x' << ContextScreenRect.H << ')'
                  << " contextViewport=" << ContextViewportSize.W << 'x' << ContextViewportSize.H
                  << " dpi=" << Context.GetDpiScale()
                  << " overlayProjection=" << RenderTargetWidth << 'x' << RenderTargetHeight
                  << " scale=" << ScaleX << 'x' << ScaleY
                  << " sourcePackets=" << PacketSpan.size()
                  << " sourceBounds=";
        if (HasUiBounds)
        {
            std::cerr << '(' << MinX << ',' << MinY << ' ' << (MaxX - MinX) << 'x' << (MaxY - MinY) << ')';
        }
        else
        {
            std::cerr << "(empty)";
        }
        std::cerr << " outputDrawPackets=" << OverlayPacket.Ui.DrawPacketCount()
                  << " outputInstances=" << OverlayPacket.Ui.InstanceCount()
                  << " text=" << OverlayPacket.TextSubmissionCount()
                  << '\n';
    }

    return OverlayPacket;
}
#endif

template <typename SettingsType>
bool StoreRendererFeatureSettings(std::unordered_map<std::int64_t, SettingsType>& SettingsMap,
                                  const std::int64_t ViewportID,
                                  const SettingsType& Settings,
                                  std::uint64_t& Revision)
{
    SettingsMap[ViewportID] = Settings;
    ++Revision;
    return true;
}

template <typename SettingsType>
[[nodiscard]] const SettingsType* ResolveRendererFeatureSettings(
    const std::unordered_map<std::int64_t, SettingsType>& SettingsMap,
    const std::uint64_t ViewportID)
{
    if (ViewportID <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
    {
        const auto SpecificIt = SettingsMap.find(static_cast<std::int64_t>(ViewportID));
        if (SpecificIt != SettingsMap.end())
        {
            return &SpecificIt->second;
        }
    }

    const auto DefaultIt = SettingsMap.find(-1);
    return DefaultIt == SettingsMap.end() ? nullptr : &DefaultIt->second;
}

template <typename TPass>
[[nodiscard]] TPass* FindRendererNewPass(SnAPI::Renderer::RendererRuntime& Runtime, const std::string_view PassName)
{
    auto PassResult = Runtime.FrameGraph().FindPass<TPass>(PassName);
    return PassResult.Succeeded() ? PassResult.Value() : nullptr;
}

[[nodiscard]] bool SelectDeferredDebugView(const RendererDeferredShadingFeatureSettings& Settings,
                                           SnAPI::Renderer::EDeferredDebugView& OutView)
{
    if (Settings.DebugMotionVectors)
    {
        OutView = SnAPI::Renderer::EDeferredDebugView::GBufferMotionVectors;
    }
    else if (Settings.DebugNormals)
    {
        OutView = SnAPI::Renderer::EDeferredDebugView::GBufferNormal;
    }
    else if (Settings.DebugAlbedo)
    {
        OutView = SnAPI::Renderer::EDeferredDebugView::GBufferAlbedo;
    }
    else if (Settings.DebugAO)
    {
        OutView = SnAPI::Renderer::EDeferredDebugView::GtaoAmbientOcclusion;
    }
    else if (Settings.DebugRoughness || Settings.DebugMetallic)
    {
        OutView = SnAPI::Renderer::EDeferredDebugView::GBufferMaterial;
    }
    else if (Settings.DebugDepth)
    {
        OutView = SnAPI::Renderer::EDeferredDebugView::SceneDepth;
    }
    else if (Settings.DebugDirectLighting || Settings.DebugLighting)
    {
        OutView = SnAPI::Renderer::EDeferredDebugView::DirectLighting;
    }
    else if (Settings.DebugGI)
    {
        OutView = SnAPI::Renderer::EDeferredDebugView::SsgiIndirect;
    }
    else if (Settings.DebugSpecular)
    {
        OutView = SnAPI::Renderer::EDeferredDebugView::FinalColor;
    }
    else
    {
        return false;
    }

    return true;
}
} // namespace

struct RendererSystem::RendererNewRuntimeState
{
    RendererNewHostWindow Window{};
    std::unique_ptr<SnAPI::Renderer::RendererRuntime> Runtime{};
    SnAPI::Renderer::RenderScene* Scene = nullptr;
    SnAPI::Renderer::SurfaceHandle Surface{};
    SnAPI::Renderer::SurfacePresentationProfile PresentationProfile{};
    SnAPI::Renderer::ResolvedRenderViewExtents ViewExtents{};
    std::unordered_map<std::uint64_t, GameRenderOutput> ViewportOutputs{};
    SnAPI::Renderer::RenderOverlayPacket SurfaceOverlay{};
    std::unordered_map<std::int64_t, RendererDeferredShadingFeatureSettings> DeferredShadingFeatureSettings{};
    std::unordered_map<std::int64_t, RendererSsaoFeatureSettings> SsaoFeatureSettings{};
    std::unordered_map<std::int64_t, RendererSsgiFeatureSettings> SsgiFeatureSettings{};
    std::unordered_map<std::int64_t, RendererSsrFeatureSettings> SsrFeatureSettings{};
    std::unordered_map<std::int64_t, RendererTaaFeatureSettings> TaaFeatureSettings{};
    std::unordered_map<std::int64_t, RendererBloomFeatureSettings> BloomFeatureSettings{};
    std::unordered_map<std::int64_t, RendererAtmosphereFeatureSettings> AtmosphereFeatureSettings{};
    std::unordered_map<std::int64_t, RendererAtmosphereCompositeFeatureSettings> AtmosphereCompositeFeatureSettings{};
    std::unordered_map<std::int64_t, RendererHeightFogFeatureSettings> HeightFogFeatureSettings{};
    std::unordered_map<std::int64_t, RendererToneMapFeatureSettings> ToneMapFeatureSettings{};
    std::uint64_t FeatureSettingsRevision{1u};
    bool ScreenGlobalIlluminationEnabled{false};
    bool AtmosphereAerialPerspectiveEnabled{false};
    SnAPI::Renderer::TextFontHandle DefaultTextFont{};
    std::uint32_t DefaultTextFontSize{24u};
    std::string DefaultTextFontPath{};
#if defined(SNAPI_GF_ENABLE_UI)
    RendererNewUiFontMetricsAdapter UiFontMetrics{};
#endif
    std::chrono::steady_clock::time_point LastFrameStart = std::chrono::steady_clock::now();
    std::uint64_t FrameIndex = 0;
    bool SurfaceValid = false;

    [[nodiscard]] bool HasOpenWindow() const noexcept
    {
        return Window.Valid() && !Window.ShouldClose();
    }

    [[nodiscard]] std::uint32_t Width() const noexcept
    {
        return Window.Width();
    }

    [[nodiscard]] std::uint32_t Height() const noexcept
    {
        return Window.Height();
    }

    void PumpWindowEvents()
    {
        Window.PumpEvents();
    }
};

bool RendererSystem::EnsureRendererNewDefaultTextFontUnlocked()
{
    if (!m_rendererNew || !m_rendererNew->Runtime)
    {
        return false;
    }

    const auto requestedSize = std::max<std::uint32_t>(1u, m_settings.DefaultFontSize);
    if (m_rendererNew->DefaultTextFont.Valid() &&
        m_rendererNew->DefaultTextFontSize == requestedSize)
    {
        return true;
    }

    std::vector<std::string> candidates{};
    if (!m_settings.DefaultFontPath.empty())
    {
        candidates.push_back(m_settings.DefaultFontPath);
    }
    candidates.push_back("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    candidates.push_back("/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf");
    candidates.push_back("/usr/share/fonts/TTF/DejaVuSans.ttf");
    candidates.push_back("/usr/share/fonts/TTF/Arial.TTF");
    candidates.push_back("C:/Windows/Fonts/segoeui.ttf");
    candidates.push_back("C:/Windows/Fonts/arial.ttf");

    for (const auto& candidate : candidates)
    {
        if (candidate.empty())
        {
            continue;
        }

        std::error_code ec;
        if (!std::filesystem::exists(candidate, ec))
        {
            continue;
        }

        auto fontResult = m_rendererNew->Runtime->Text().RegisterFontFromPath(SnAPI::Renderer::TextFontPathRegistrationDesc{
            .FamilyName = "SnAPI UI",
            .StyleName = "Regular",
            .Path = candidate});
        if (fontResult.Failed())
        {
            continue;
        }

        m_rendererNew->DefaultTextFont = fontResult.Value();
        m_rendererNew->DefaultTextFontSize = requestedSize;
        m_rendererNew->DefaultTextFontPath = candidate;
        m_defaultFontFallbacksConfigured = true;
        return true;
    }

    m_rendererNew->DefaultTextFont = {};
    m_rendererNew->DefaultTextFontPath.clear();
    m_defaultFontFallbacksConfigured = false;
    return false;
}

void RendererSystem::WarmRendererNewQueuedOverlayTextUnlocked()
{
    if (!m_rendererNew || !m_rendererNew->Runtime)
    {
        return;
    }

    const auto WarmOverlay = [this](SnAPI::Renderer::RenderOverlayPacket& Overlay)
    {
        if (Overlay.TextSubmissionCount() == 0u)
        {
            return;
        }

        auto framePacketResult = m_rendererNew->Runtime->Text().PrepareFrame(SnAPI::Renderer::TextFrameSubmissionDesc{
            .TextSubmissions = Overlay.TextSubmissions,
            .PreShapedTextSubmissions = Overlay.PreShapedTextSubmissions});
        if (framePacketResult.Failed())
        {
            std::cerr << "RendererSystem failed to prepare Renderer.New overlay text: "
                      << framePacketResult.Error().Message << '\n';
            return;
        }

        auto backendPacket = SnAPI::Renderer::TextSystem::BuildBackendPacket(framePacketResult.Value());
        auto gpuPacketResult = m_rendererNew->Runtime->Text().UploadBackendPacket(
            m_rendererNew->Runtime->Device(),
            backendPacket);
        if (gpuPacketResult.Failed())
        {
            std::cerr << "RendererSystem failed to warm Renderer.New overlay text atlas: "
                      << gpuPacketResult.Error().Message << '\n';
            return;
        }

        if (gpuPacketResult.Value().InstanceBuffer.Valid())
        {
            auto destroyResult = m_rendererNew->Runtime->DestroyBuffer(gpuPacketResult.Value().InstanceBuffer);
            if (destroyResult.Failed())
            {
                std::cerr << "RendererSystem failed to release Renderer.New text warmup buffer: "
                          << destroyResult.Error().Message << '\n';
            }
        }
    };

    WarmOverlay(m_rendererNew->SurfaceOverlay);
    for (auto& [ViewportID, Output] : m_rendererNew->ViewportOutputs)
    {
        static_cast<void>(ViewportID);
        WarmOverlay(Output.Overlay());
    }
}

void RendererSystem::ClearRendererNewQueuedOverlaysUnlocked()
{
    if (m_rendererNew)
    {
        m_rendererNew->SurfaceOverlay = {};
        for (auto& [ViewportID, Output] : m_rendererNew->ViewportOutputs)
        {
            static_cast<void>(ViewportID);
            Output.ClearOverlay();
        }
    }
}

void RendererSystem::FlushRendererNewDebugLinesUnlocked()
{
    if (!m_rendererNew || !m_rendererNew->Scene)
    {
        m_rendererNewDebugLines.clear();
        return;
    }

    m_rendererNew->Scene->Debug.Clear();
    for (const GameRenderDebugLine& Line : m_rendererNewDebugLines)
    {
        auto Result = m_rendererNew->Scene->Debug.DrawLine(SnAPI::Renderer::DebugGeometryLine{
            .StartWorld = ToRendererPoint3(Line.StartWorld),
            .EndWorld = ToRendererPoint3(Line.EndWorld),
            .ThicknessPixels = Line.ThicknessPixels,
            .DepthMode = Line.DepthTest
                ? SnAPI::Renderer::EDebugGeometryDepthMode::Test
                : SnAPI::Renderer::EDebugGeometryDepthMode::Always,
            .Material = SnAPI::Renderer::DebugGeometryMaterialDesc{.TintLinear = Line.ColorLinear}});
        if (Result.Failed())
        {
            std::cerr << "RendererSystem failed to queue Renderer.New debug line: "
                      << Result.Error().Message << '\n';
        }
    }

    m_rendererNewDebugLines.clear();
}

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
    auto& Output = State.ViewportOutputs[ViewportID];
    if (Output.HasTarget() &&
        Output.Extent().Width == Width &&
        Output.Extent().Height == Height)
    {
        return true;
    }

    if (Output.Target().Valid())
    {
        (void)State.Runtime->DestroyRenderTarget(Output.Target());
    }
    if (Output.Texture().Valid())
    {
        (void)State.Runtime->DestroyTexture(Output.Texture());
    }
    const bool WasEnabled = Output.Id() == 0u || Output.Enabled();
    Output.ClearRenderTarget();

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

    Output.ConfigureTarget(
        ViewportID,
        "RendererSystem.Viewport." + std::to_string(ViewportID),
        TextureResult.Value(),
        TargetResult.Value(),
        SnAPI::Renderer::Extent2D{.Width = Width, .Height = Height},
        WasEnabled);
    return true;
}

void RendererSystem::DestroyRendererNewViewportTarget(const std::uint64_t ViewportID)
{
    if (!m_rendererNew)
    {
        return;
    }

    auto& State = *m_rendererNew;
    auto It = State.ViewportOutputs.find(ViewportID);
    if (It == State.ViewportOutputs.end() || !State.Runtime)
    {
        if (It != State.ViewportOutputs.end())
        {
            State.ViewportOutputs.erase(It);
        }
        return;
    }

    if (It->second.Target().Valid())
    {
        (void)State.Runtime->DestroyRenderTarget(It->second.Target());
    }
    if (It->second.Texture().Valid())
    {
        (void)State.Runtime->DestroyTexture(It->second.Texture());
    }
    State.ViewportOutputs.erase(It);
}

[[nodiscard]] static bool ShouldRenderRendererNewViewportPreset(const EGameRenderFeatureProfile Preset) noexcept
{
    return Preset == EGameRenderFeatureProfile::DefaultWorld ||
           Preset == EGameRenderFeatureProfile::EditorWorld;
}

void RendererSystem::RendererNewRuntimeStateDeleter::operator()(RendererNewRuntimeState* State) const
{
    delete State;
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
    m_mainWindow = Other.m_mainWindow;
    m_activeCamera = std::move(Other.m_activeCamera);
    m_textQueue = std::move(Other.m_textQueue);
#if defined(SNAPI_GF_ENABLE_UI)
    m_uiPendingTextureUploads = std::move(Other.m_uiPendingTextureUploads);
    m_uiQueuedRects = std::move(Other.m_uiQueuedRects);
    m_uiPacketsQueuedThisFrame = Other.m_uiPacketsQueuedThisFrame;
#endif
    m_renderViewportFeatureProfiles = std::move(Other.m_renderViewportFeatureProfiles);
    m_renderViewportFeatureRevision = Other.m_renderViewportFeatureRevision;
    m_defaultTaaJitterScale = Other.m_defaultTaaJitterScale;
    m_viewportTaaJitterScales = std::move(Other.m_viewportTaaJitterScales);
    m_taaFrameIndex = Other.m_taaFrameIndex;
    m_initialized = Other.m_initialized;

    Other.m_activeCamera.reset();
    Other.m_rendererNew.reset();
    Other.m_mainWindow.Reset();
    Other.m_textQueue.clear();
#if defined(SNAPI_GF_ENABLE_UI)
    Other.m_uiPendingTextureUploads.clear();
    Other.m_uiQueuedRects.clear();
    Other.m_uiPacketsQueuedThisFrame = false;
#endif
    Other.m_renderViewportFeatureProfiles.clear();
    Other.m_renderViewportFeatureRevision = 1;
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
    m_mainWindow = Other.m_mainWindow;
    m_activeCamera = std::move(Other.m_activeCamera);
    m_textQueue = std::move(Other.m_textQueue);
#if defined(SNAPI_GF_ENABLE_UI)
    m_uiPendingTextureUploads = std::move(Other.m_uiPendingTextureUploads);
    m_uiQueuedRects = std::move(Other.m_uiQueuedRects);
    m_uiPacketsQueuedThisFrame = Other.m_uiPacketsQueuedThisFrame;
#endif
    m_renderViewportFeatureProfiles = std::move(Other.m_renderViewportFeatureProfiles);
    m_renderViewportFeatureRevision = Other.m_renderViewportFeatureRevision;
    m_defaultTaaJitterScale = Other.m_defaultTaaJitterScale;
    m_viewportTaaJitterScales = std::move(Other.m_viewportTaaJitterScales);
    m_taaFrameIndex = Other.m_taaFrameIndex;
    m_initialized = Other.m_initialized;

    Other.m_activeCamera.reset();
    Other.m_rendererNew.reset();
    Other.m_mainWindow.Reset();
    Other.m_textQueue.clear();
#if defined(SNAPI_GF_ENABLE_UI)
    Other.m_uiPendingTextureUploads.clear();
    Other.m_uiQueuedRects.clear();
    Other.m_uiPacketsQueuedThisFrame = false;
#endif
    Other.m_renderViewportFeatureProfiles.clear();
    Other.m_renderViewportFeatureRevision = 1;
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
    m_renderViewportFeatureProfiles.clear();
    m_renderViewportFeatureRevision = 1;
    m_taaFrameIndex = 0;
    m_rendererNew.reset();
    m_initialized = false;

    if (!m_settings.CreateRendererRuntime)
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
    if (m_initialized && m_settings.ApplyDefaultFeatureProfile)
    {
        (void)UseDefaultRenderViewport(true);
        (void)ApplyDefaultFeatureProfile();
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
    m_mainWindow.Reset();
    ResetPassPointers();
    m_activeCamera.reset();
    m_defaultFontFallbacksConfigured = false;
    m_textQueue.clear();
#if defined(SNAPI_GF_ENABLE_UI)
    m_uiPendingTextureUploads.clear();
    m_uiQueuedRects.clear();
    m_uiPacketsQueuedThisFrame = false;
#endif
    m_renderViewportFeatureProfiles.clear();
    m_renderViewportFeatureRevision = 1;
    m_viewportTaaJitterScales.clear();
    m_taaFrameIndex = 0;
}

bool RendererSystem::IsInitialized() const
{
    GameLockGuard Lock(m_mutex);
    return m_initialized;
}

GameRenderWindow RendererSystem::MainWindow() const
{
    GameLockGuard Lock(m_mutex);
    return m_mainWindow;
}

bool RendererSystem::HasOpenWindow() const
{
    GameLockGuard Lock(m_mutex);
    return m_initialized && m_rendererNew && m_rendererNew->HasOpenWindow();
}

bool RendererSystem::SetActiveCamera(const std::shared_ptr<GameRenderCamera>& Camera)
{
    GameLockGuard Lock(m_mutex);
    m_activeCamera = Camera;
    return m_initialized;
}

bool RendererSystem::SetActiveCamera(GameRenderCamera* Camera)
{
    GameLockGuard Lock(m_mutex);
    if (!Camera)
    {
        m_activeCamera.reset();
        return m_initialized;
    }
    return m_initialized && m_activeCamera.get() == Camera;
}

GameRenderCamera* RendererSystem::ActiveCamera() const
{
    GameLockGuard Lock(m_mutex);
    return m_activeCamera.get();
}

std::shared_ptr<GameRenderCamera> RendererSystem::ActiveCameraShared() const
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

bool RendererSystem::UseDefaultRenderViewport(const bool Enabled)
{
    GameLockGuard Lock(m_mutex);
    if (!m_initialized)
    {
        return false;
    }

    if (Enabled)
    {
        m_renderViewportFeatureProfiles.try_emplace(1u, EGameRenderFeatureProfile::None);
    }
    else
    {
        DestroyRendererNewViewportTarget(1u);
        m_renderViewportFeatureProfiles.erase(1u);
    }
    ++m_renderViewportFeatureRevision;
    return true;
}

bool RendererSystem::IsUsingDefaultRenderViewport() const
{
    GameLockGuard Lock(m_mutex);
    return m_renderViewportFeatureProfiles.contains(1u);
}

bool RendererSystem::CreateRenderViewport(std::string Name,
                                          const float X,
                                          const float Y,
                                          const float Width,
                                          const float Height,
                                          const std::uint32_t RenderWidth,
                                          const std::uint32_t RenderHeight,
                                          const std::shared_ptr<GameRenderCamera>& Camera,
                                          const bool Enabled,
                                          std::uint64_t& OutViewportID)
{
    (void)Name;
    (void)X;
    (void)Y;
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
    while (m_renderViewportFeatureProfiles.contains(OutViewportID))
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
        m_rendererNew->ViewportOutputs[OutViewportID].Enabled(Enabled);
        m_rendererNew->ViewportOutputs[OutViewportID].Camera(Camera);
    }
    m_renderViewportFeatureProfiles.emplace(OutViewportID, EGameRenderFeatureProfile::None);
    ++m_renderViewportFeatureRevision;
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
                                          const std::shared_ptr<GameRenderCamera>& Camera,
                                          const bool Enabled)
{
    (void)Name;
    (void)X;
    (void)Y;
    GameLockGuard Lock(m_mutex);
    if (!m_initialized || !m_renderViewportFeatureProfiles.contains(ViewportID))
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
        m_rendererNew->ViewportOutputs[ViewportID].Enabled(Enabled);
        m_rendererNew->ViewportOutputs[ViewportID].Camera(Camera);
    }
    return true;
}

bool RendererSystem::DestroyRenderViewport(const std::uint64_t ViewportID)
{
    GameLockGuard Lock(m_mutex);
    if (ViewportID == 1u)
    {
        return false;
    }

    DestroyRendererNewViewportTarget(ViewportID);
    const bool Removed = m_renderViewportFeatureProfiles.erase(ViewportID) > 0u;
    if (Removed)
    {
        ++m_renderViewportFeatureRevision;
    }
    return Removed;
}

bool RendererSystem::HasRenderViewport(const std::uint64_t ViewportID) const
{
    GameLockGuard Lock(m_mutex);
    return m_renderViewportFeatureProfiles.contains(ViewportID);
}

bool RendererSystem::SetRenderViewportIndex(const std::uint64_t ViewportID, const std::size_t Index)
{
    (void)Index;
    GameLockGuard Lock(m_mutex);
    return m_initialized && m_renderViewportFeatureProfiles.contains(ViewportID);
}

std::optional<std::size_t> RendererSystem::RenderViewportIndex(const std::uint64_t ViewportID) const
{
    GameLockGuard Lock(m_mutex);
    if (!m_renderViewportFeatureProfiles.contains(ViewportID))
    {
        return std::nullopt;
    }
    return static_cast<std::size_t>(ViewportID - 1u);
}

bool RendererSystem::ApplyRenderViewportFeatureProfile(const std::uint64_t ViewportID, const EGameRenderFeatureProfile Preset)
{
    GameLockGuard Lock(m_mutex);
    return ApplyRenderViewportFeatureProfileUnlocked(ViewportID, Preset, true);
}

bool RendererSystem::ApplyRenderViewportFeatureProfileUnlocked(const std::uint64_t ViewportID,
                                                             const EGameRenderFeatureProfile Preset,
                                                             const bool TrackDefaultPassPointers)
{
    (void)TrackDefaultPassPointers;
    if (!m_initialized || ViewportID == 0u)
    {
        return false;
    }

    const auto [It, Inserted] = m_renderViewportFeatureProfiles.try_emplace(ViewportID, Preset);
    if (!Inserted && It->second != Preset && It->second != EGameRenderFeatureProfile::None)
    {
        return false;
    }
    It->second = Preset;
    ++m_renderViewportFeatureRevision;
    return true;
}

bool RendererSystem::CreateStaticRenderMesh(
    const RuntimeMeshData& MeshData,
    GameRenderMesh& OutMesh,
    const std::string_view DebugName)
{
    GameLockGuard Lock(m_mutex);
    if (!m_initialized || !m_rendererNew || !m_rendererNew->Runtime)
    {
        return false;
    }

    if (OutMesh.Valid())
    {
        (void)DestroyRendererNewMeshResources(*m_rendererNew->Runtime, OutMesh);
    }

    auto MeshDataResult = BuildRendererNewSurfaceMeshData(MeshData);
    if (!MeshDataResult.has_value())
    {
        std::cerr << "RendererSystem failed to translate compiled mesh data '"
                  << MeshData.DebugName << "' into Renderer.New surface mesh data.\n";
        return false;
    }

    auto Uploaded = UploadRendererNewStaticMesh(*m_rendererNew->Runtime, std::move(*MeshDataResult), DebugName);
    if (!Uploaded.has_value())
    {
        return false;
    }

    OutMesh.m_Mesh = Uploaded->Mesh;
    OutMesh.m_VertexBuffer = Uploaded->VertexBuffer;
    OutMesh.m_IndexBuffer = Uploaded->IndexBuffer;
    OutMesh.m_SourceId = MeshData.SourceId;
    OutMesh.m_SourceRevision = MeshData.SourceRevision;
    OutMesh.m_LocalBoundsCenter = Uploaded->LocalBoundsCenter;
    OutMesh.m_LocalBoundsMin = Uploaded->LocalBoundsMin;
    OutMesh.m_LocalBoundsMax = Uploaded->LocalBoundsMax;
    OutMesh.m_LocalBoundsRadius = Uploaded->LocalBoundsRadius;
    OutMesh.m_HasLocalBounds = Uploaded->HasLocalBounds;
    OutMesh.m_DebugName = std::move(Uploaded->DebugName);
    return true;
}

bool RendererSystem::CreateStaticRenderMesh(
    SnAPI::Renderer::PrimitiveMeshData MeshData,
    GameRenderMesh& OutMesh,
    const std::string_view DebugName)
{
    GameLockGuard Lock(m_mutex);
    if (!m_initialized || !m_rendererNew || !m_rendererNew->Runtime)
    {
        return false;
    }

    if (OutMesh.Valid())
    {
        (void)DestroyRendererNewMeshResources(*m_rendererNew->Runtime, OutMesh);
    }

    auto MeshDataResult = BuildRendererNewSurfaceMeshData(std::move(MeshData));
    if (!MeshDataResult.has_value())
    {
        std::cerr << "RendererSystem failed to translate primitive mesh data into Renderer.New surface mesh data.\n";
        return false;
    }

    const std::uint64_t SourceSignature = MeshDataResult->MeshData.GeometrySignature;
    auto Uploaded = UploadRendererNewStaticMesh(*m_rendererNew->Runtime, std::move(*MeshDataResult), DebugName);
    if (!Uploaded.has_value())
    {
        return false;
    }

    OutMesh.m_Mesh = Uploaded->Mesh;
    OutMesh.m_VertexBuffer = Uploaded->VertexBuffer;
    OutMesh.m_IndexBuffer = Uploaded->IndexBuffer;
    OutMesh.m_SourceId = SourceSignature;
    OutMesh.m_SourceRevision = 0u;
    OutMesh.m_LocalBoundsCenter = Uploaded->LocalBoundsCenter;
    OutMesh.m_LocalBoundsMin = Uploaded->LocalBoundsMin;
    OutMesh.m_LocalBoundsMax = Uploaded->LocalBoundsMax;
    OutMesh.m_LocalBoundsRadius = Uploaded->LocalBoundsRadius;
    OutMesh.m_HasLocalBounds = Uploaded->HasLocalBounds;
    OutMesh.m_DebugName = std::move(Uploaded->DebugName);
    return true;
}

bool RendererSystem::DestroyRenderMesh(GameRenderMesh& Mesh)
{
    GameLockGuard Lock(m_mutex);
    if (!m_rendererNew || !m_rendererNew->Runtime)
    {
        const bool HadMesh = Mesh.Valid();
        Mesh.Reset();
        return HadMesh;
    }

    return DestroyRendererNewMeshResources(*m_rendererNew->Runtime, Mesh);
}

bool RendererSystem::CreateStaticRenderObject(
    const GameRenderMesh& Mesh,
    GameRenderObject& OutObject,
    const SnAPI::Math::Matrix4& WorldFromLocal,
    const bool CastShadows,
    const std::string_view DebugName)
{
    return CreateStaticRenderObject(
        Mesh,
        OutObject,
        WorldFromLocal,
        BuildGameObjectFeatureChannels(CastShadows),
        CastShadows,
        DebugName);
}

bool RendererSystem::CreateStaticRenderObject(
    const GameRenderMesh& Mesh,
    GameRenderObject& OutObject,
    const SnAPI::Math::Matrix4& WorldFromLocal,
    SnAPI::Renderer::RenderFeatureChannelMask FeatureChannels,
    const bool CastShadows,
    const std::string_view DebugName)
{
    if (!Mesh.Valid())
    {
        return false;
    }

    GameLockGuard Lock(m_mutex);
    if (!m_initialized || !m_rendererNew || !m_rendererNew->Scene)
    {
        return false;
    }

    if (OutObject.Valid())
    {
        (void)DestroyRendererNewObjectResource(*m_rendererNew->Scene, OutObject);
    }

    std::string ObjectDebugName = DebugName.empty() ? Mesh.DebugName() : std::string(DebugName);
    if (ObjectDebugName.empty())
    {
        ObjectDebugName = "GameRenderObject";
    }

    const auto ObjectMobility = SnAPI::Renderer::ERenderMobility::Dynamic;

    auto ObjectResult = m_rendererNew->Scene->CreateStaticMeshObject(SnAPI::Renderer::RetainedStaticMeshObjectDesc{
        .DebugName = ObjectDebugName,
        .Mesh = Mesh.Mesh(),
        .Mobility = ObjectMobility,
        .FeatureChannels = FeatureChannels,
        .WorldFromLocal = WorldFromLocal,
        .PreviousWorldFromLocal = WorldFromLocal,
        .PreviousWorldFromLocalValid = true});
    if (ObjectResult.Failed())
    {
        std::cerr << "RendererSystem failed to create Renderer.New retained mesh object: "
                  << ObjectResult.Error().Message << '\n';
        return false;
    }

    OutObject.m_Object = ObjectResult.Value();
    OutObject.m_Mesh = Mesh.Mesh();
    OutObject.m_WorldFromLocal = WorldFromLocal;
    OutObject.m_PreviousWorldFromLocal = WorldFromLocal;
    OutObject.m_PreviousWorldFromLocalValid = true;
    OutObject.m_Visible = true;
    OutObject.m_CastShadows = CastShadows;
    OutObject.m_Mobility = ObjectMobility;
    OutObject.m_FeatureChannels = FeatureChannels;
    OutObject.m_DebugName = std::move(ObjectDebugName);
    return true;
}

bool RendererSystem::DestroyRenderObject(GameRenderObject& Object)
{
    GameLockGuard Lock(m_mutex);
    if (!m_rendererNew || !m_rendererNew->Scene)
    {
        const bool HadObject = Object.Valid();
        Object.Reset();
        return HadObject;
    }

    return DestroyRendererNewObjectResource(*m_rendererNew->Scene, Object);
}

bool RendererSystem::SetRenderObjectTransform(
    GameRenderObject& Object,
    const SnAPI::Math::Matrix4& WorldFromLocal)
{
    GameLockGuard Lock(m_mutex);
    if (!Object.Valid() || !m_initialized || !m_rendererNew || !m_rendererNew->Scene)
    {
        return false;
    }

    auto Result = m_rendererNew->Scene->SetObjectTransform(Object.Object(), WorldFromLocal);
    if (Result.Failed())
    {
        std::cerr << "RendererSystem failed to update Renderer.New object transform: "
                  << Result.Error().Message << '\n';
        return false;
    }

    Object.m_PreviousWorldFromLocal = Object.m_WorldFromLocal;
    Object.m_PreviousWorldFromLocalValid = true;
    Object.m_WorldFromLocal = WorldFromLocal;
    return true;
}

bool RendererSystem::QueueDebugLine(const GameRenderDebugLine& Line)
{
    if (!Line.StartWorld.allFinite() || !Line.EndWorld.allFinite() || Line.ThicknessPixels <= 0.0f)
    {
        return false;
    }

    GameLockGuard Lock(m_mutex);
    if (!m_initialized || !m_rendererNew || !m_rendererNew->Scene)
    {
        return false;
    }

    m_rendererNewDebugLines.push_back(Line);
    return true;
}

bool RendererSystem::CreateDirectionalRenderLight(
    const SnAPI::Renderer::DirectionalLightDesc& Desc,
    GameRenderLight& OutLight,
    const std::string_view DebugName)
{
    GameLockGuard Lock(m_mutex);
    if (!m_initialized || !m_rendererNew || !m_rendererNew->Scene)
    {
        return false;
    }

    if (OutLight.Valid())
    {
        if (const auto DestroyResult = m_rendererNew->Scene->DestroyLight(OutLight.Light()); DestroyResult.Failed())
        {
            std::cerr << "RendererSystem failed to replace Renderer.New light: "
                      << DestroyResult.Error().Message << '\n';
            return false;
        }
        OutLight.Reset();
    }

    auto LightDesc = Desc;
    if (!DebugName.empty())
    {
        LightDesc.DebugName = std::string(DebugName);
    }
    if (LightDesc.DebugName.empty())
    {
        LightDesc.DebugName = "GameRenderLight";
    }

    auto LightResult = m_rendererNew->Scene->CreateDirectionalLight(LightDesc);
    if (LightResult.Failed())
    {
        std::cerr << "RendererSystem failed to create Renderer.New directional light: "
                  << LightResult.Error().Message << '\n';
        return false;
    }

    OutLight.m_Light = LightResult.Value();
    OutLight.m_DirectionalDesc = std::make_shared<SnAPI::Renderer::DirectionalLightDesc>(LightDesc);
    OutLight.m_DebugName = LightDesc.DebugName;
    return true;
}

bool RendererSystem::SetDirectionalRenderLight(
    GameRenderLight& Light,
    const SnAPI::Renderer::DirectionalLightDesc& Desc)
{
    GameLockGuard Lock(m_mutex);
    if (!Light.Valid() || !m_initialized || !m_rendererNew || !m_rendererNew->Scene)
    {
        return false;
    }

    auto LightDesc = Desc;
    if (LightDesc.DebugName.empty())
    {
        LightDesc.DebugName = Light.DebugName();
    }

    auto Result = m_rendererNew->Scene->SetDirectionalLight(Light.Light(), LightDesc);
    if (Result.Failed())
    {
        std::cerr << "RendererSystem failed to update Renderer.New directional light: "
                  << Result.Error().Message << '\n';
        return false;
    }

    Light.m_DirectionalDesc = std::make_shared<SnAPI::Renderer::DirectionalLightDesc>(LightDesc);
    Light.m_DebugName = LightDesc.DebugName;
    return true;
}

bool RendererSystem::DestroyRenderLight(GameRenderLight& Light)
{
    GameLockGuard Lock(m_mutex);
    const bool HadLight = Light.Valid();
    if (!HadLight)
    {
        return false;
    }

    if (m_rendererNew && m_rendererNew->Scene)
    {
        if (const auto Result = m_rendererNew->Scene->DestroyLight(Light.Light()); Result.Failed())
        {
            std::cerr << "RendererSystem failed to destroy Renderer.New light: "
                      << Result.Error().Message << '\n';
            return false;
        }
    }

    Light.Reset();
    return true;
}

std::uint64_t RendererSystem::RenderViewportFeatureRevision() const
{
    GameLockGuard Lock(m_mutex);
    return m_renderViewportFeatureRevision;
}

std::uint64_t RendererSystem::RendererFeatureSettingsRevision() const
{
    GameLockGuard Lock(m_mutex);
    return m_rendererNew ? m_rendererNew->FeatureSettingsRevision : 0u;
}

bool RendererSystem::ApplyDeferredShadingFeatureSettings(
    const std::int64_t ViewportID,
    const RendererDeferredShadingFeatureSettings& Settings)
{
    GameLockGuard Lock(m_mutex);
    return m_initialized && m_rendererNew &&
           StoreRendererFeatureSettings(
               m_rendererNew->DeferredShadingFeatureSettings,
               ViewportID,
               Settings,
               m_rendererNew->FeatureSettingsRevision);
}

bool RendererSystem::ApplySsaoFeatureSettings(
    const std::int64_t ViewportID,
    const RendererSsaoFeatureSettings& Settings)
{
    GameLockGuard Lock(m_mutex);
    return m_initialized && m_rendererNew &&
           StoreRendererFeatureSettings(
               m_rendererNew->SsaoFeatureSettings,
               ViewportID,
               Settings,
               m_rendererNew->FeatureSettingsRevision);
}

bool RendererSystem::ApplySsgiFeatureSettings(
    const std::int64_t ViewportID,
    const RendererSsgiFeatureSettings& Settings)
{
    GameLockGuard Lock(m_mutex);
    return m_initialized && m_rendererNew &&
           StoreRendererFeatureSettings(
               m_rendererNew->SsgiFeatureSettings,
               ViewportID,
               Settings,
               m_rendererNew->FeatureSettingsRevision);
}

bool RendererSystem::ApplySsrFeatureSettings(
    const std::int64_t ViewportID,
    const RendererSsrFeatureSettings& Settings)
{
    GameLockGuard Lock(m_mutex);
    return m_initialized && m_rendererNew &&
           StoreRendererFeatureSettings(
               m_rendererNew->SsrFeatureSettings,
               ViewportID,
               Settings,
               m_rendererNew->FeatureSettingsRevision);
}

bool RendererSystem::ApplyTaaFeatureSettings(
    const std::int64_t ViewportID,
    const RendererTaaFeatureSettings& Settings)
{
    GameLockGuard Lock(m_mutex);
    if (!m_initialized || !m_rendererNew)
    {
        return false;
    }

    if (ViewportID >= 0)
    {
        m_viewportTaaJitterScales[static_cast<std::uint64_t>(ViewportID)] = std::max(0.0f, Settings.JitterScale);
    }
    else
    {
        m_defaultTaaJitterScale = std::max(0.0f, Settings.JitterScale);
    }
    return StoreRendererFeatureSettings(
        m_rendererNew->TaaFeatureSettings,
        ViewportID,
        Settings,
        m_rendererNew->FeatureSettingsRevision);
}

bool RendererSystem::ApplyBloomFeatureSettings(
    const std::int64_t ViewportID,
    const RendererBloomFeatureSettings& Settings)
{
    GameLockGuard Lock(m_mutex);
    return m_initialized && m_rendererNew &&
           StoreRendererFeatureSettings(
               m_rendererNew->BloomFeatureSettings,
               ViewportID,
               Settings,
               m_rendererNew->FeatureSettingsRevision);
}

bool RendererSystem::ApplyAtmosphereFeatureSettings(
    const std::int64_t ViewportID,
    const RendererAtmosphereFeatureSettings& Settings)
{
    GameLockGuard Lock(m_mutex);
    return m_initialized && m_rendererNew &&
           StoreRendererFeatureSettings(
               m_rendererNew->AtmosphereFeatureSettings,
               ViewportID,
               Settings,
               m_rendererNew->FeatureSettingsRevision);
}

bool RendererSystem::ApplyAtmosphereCompositeFeatureSettings(
    const std::int64_t ViewportID,
    const RendererAtmosphereCompositeFeatureSettings& Settings)
{
    GameLockGuard Lock(m_mutex);
    return m_initialized && m_rendererNew &&
           StoreRendererFeatureSettings(
               m_rendererNew->AtmosphereCompositeFeatureSettings,
               ViewportID,
               Settings,
               m_rendererNew->FeatureSettingsRevision);
}

bool RendererSystem::ApplyHeightFogFeatureSettings(
    const std::int64_t ViewportID,
    const RendererHeightFogFeatureSettings& Settings)
{
    GameLockGuard Lock(m_mutex);
    return m_initialized && m_rendererNew &&
           StoreRendererFeatureSettings(
               m_rendererNew->HeightFogFeatureSettings,
               ViewportID,
               Settings,
               m_rendererNew->FeatureSettingsRevision);
}

bool RendererSystem::ApplyToneMapFeatureSettings(
    const std::int64_t ViewportID,
    const RendererToneMapFeatureSettings& Settings)
{
    GameLockGuard Lock(m_mutex);
    return m_initialized && m_rendererNew &&
           StoreRendererFeatureSettings(
               m_rendererNew->ToneMapFeatureSettings,
               ViewportID,
               Settings,
               m_rendererNew->FeatureSettingsRevision);
}

void RendererSystem::ApplyRendererNewFeatureSettingsUnlocked(const std::uint64_t ViewportID)
{
    if (!m_rendererNew || !m_rendererNew->Runtime)
    {
        return;
    }

    auto& Runtime = *m_rendererNew->Runtime;

    SnAPI::Renderer::EDeferredDebugView DebugView = SnAPI::Renderer::EDeferredDebugView::FinalColor;
    const auto* DeferredSettings = ResolveRendererFeatureSettings(m_rendererNew->DeferredShadingFeatureSettings, ViewportID);
    const bool DebugViewEnabled = DeferredSettings != nullptr && SelectDeferredDebugView(*DeferredSettings, DebugView);
    Runtime.FrameGraph().PassEnabled(SnAPI::Renderer::DeferredProfile::Id, "DeferredDebugViewPass", DebugViewEnabled);
    Runtime.FrameGraph().PassEnabled(SnAPI::Renderer::DeferredProfile::Id, "DeferredTonemapExternalTargetPass", !DebugViewEnabled);
    Runtime.FrameGraph().PassEnabled(SnAPI::Renderer::DeferredProfile::Id, "DeferredTonemapDebugColorPass", DebugViewEnabled);
    if (auto* DebugPass = FindRendererNewPass<SnAPI::Renderer::DeferredDebugViewPass>(Runtime, "DeferredDebugViewPass"))
    {
        (void)DebugPass->Set<&SnAPI::Renderer::DeferredDebugViewPass::Settings::Enabled>(DebugViewEnabled);
        (void)DebugPass->Set<&SnAPI::Renderer::DeferredDebugViewPass::Settings::View>(DebugView);
    }

    if (const auto* SsaoSettings = ResolveRendererFeatureSettings(m_rendererNew->SsaoFeatureSettings, ViewportID))
    {
        if (auto* GtaoPass = FindRendererNewPass<SnAPI::Renderer::DeferredGtaoMainPass>(Runtime, "DeferredGtaoMainPass"))
        {
            (void)GtaoPass->Set<&SnAPI::Renderer::DeferredGtaoMainPass::Settings::EffectRadiusWorld>(SsaoSettings->Radius);
            (void)GtaoPass->Set<&SnAPI::Renderer::DeferredGtaoMainPass::Settings::EffectFalloffRange>(
                std::max(0.001f, SsaoSettings->FalloffEnd - SsaoSettings->FalloffStart));
            (void)GtaoPass->Set<&SnAPI::Renderer::DeferredGtaoMainPass::Settings::FinalValuePower>(
                std::max(0.001f, SsaoSettings->Intensity));
            (void)GtaoPass->Set<&SnAPI::Renderer::DeferredGtaoMainPass::Settings::ThinOccluderCompensation>(
                SsaoSettings->Thickness);
        }

        for (std::uint32_t Index = 1u; Index <= 5u; ++Index)
        {
            const std::string PassName = "DeferredGtaoDepthDownsamplePass" + std::to_string(Index);
            if (auto* DownsamplePass = FindRendererNewPass<SnAPI::Renderer::DeferredGtaoDepthDownsamplePass>(
                    Runtime,
                    PassName))
            {
                (void)DownsamplePass->Set<&SnAPI::Renderer::DeferredGtaoDepthDownsamplePass::Settings::EffectRadiusWorld>(
                    SsaoSettings->Radius);
                (void)DownsamplePass->Set<&SnAPI::Renderer::DeferredGtaoDepthDownsamplePass::Settings::EffectFalloffRange>(
                    std::max(0.001f, SsaoSettings->FalloffEnd - SsaoSettings->FalloffStart));
                (void)DownsamplePass->Set<&SnAPI::Renderer::DeferredGtaoDepthDownsamplePass::Settings::Epsilon>(
                    std::max(0.00001f, SsaoSettings->Bias));
            }
        }
    }

    if (const auto* SsgiSettings = ResolveRendererFeatureSettings(m_rendererNew->SsgiFeatureSettings, ViewportID))
    {
        if (auto* SsgiPass = FindRendererNewPass<SnAPI::Renderer::DeferredSsgiMainPass>(Runtime, "DeferredSsgiMainPass"))
        {
            (void)SsgiPass->Set<&SnAPI::Renderer::DeferredSsgiMainPass::Settings::EffectRadiusWorld>(
                SsgiSettings->MaxDistance);
            (void)SsgiPass->Set<&SnAPI::Renderer::DeferredSsgiMainPass::Settings::SampleThicknessWorld>(
                SsgiSettings->Thickness);
            (void)SsgiPass->Set<&SnAPI::Renderer::DeferredSsgiMainPass::Settings::SampleDistributionPower>(
                std::max(0.25f, SsgiSettings->StepExponent));
            (void)SsgiPass->Set<&SnAPI::Renderer::DeferredSsgiMainPass::Settings::IndirectIntensity>(
                SsgiSettings->Intensity);
            (void)SsgiPass->Set<&SnAPI::Renderer::DeferredSsgiMainPass::Settings::BounceSourceMaxLuminance>(
                std::max(0.0f, SsgiSettings->RadianceClamp));
            (void)SsgiPass->Set<&SnAPI::Renderer::DeferredSsgiMainPass::Settings::SliceCount>(
                static_cast<float>(std::max<std::uint32_t>(1u, SsgiSettings->RayCount)));
            (void)SsgiPass->Set<&SnAPI::Renderer::DeferredSsgiMainPass::Settings::SampleCount>(
                static_cast<float>(std::max<std::uint32_t>(1u, SsgiSettings->MaxSteps)));
            (void)SsgiPass->Set<&SnAPI::Renderer::DeferredSsgiMainPass::Settings::Epsilon>(
                std::max(0.00001f, SsgiSettings->SurfaceBias));
        }
    }

    if (const auto* TaaSettings = ResolveRendererFeatureSettings(m_rendererNew->TaaFeatureSettings, ViewportID))
    {
        if (auto* TemporalPass = FindRendererNewPass<SnAPI::Renderer::TemporalResolvePass>(Runtime, "TemporalResolvePass"))
        {
            const float FeedbackMin = std::clamp(1.0f - TaaSettings->BlendFactor, 0.0f, 0.999f);
            const float FeedbackMax = std::max(FeedbackMin, std::clamp(1.0f - TaaSettings->MotionBlendFactor, 0.0f, 0.999f));
            (void)TemporalPass->Set<&SnAPI::Renderer::TemporalResolvePass::Settings::FeedbackMin>(FeedbackMin);
            (void)TemporalPass->Set<&SnAPI::Renderer::TemporalResolvePass::Settings::FeedbackMax>(FeedbackMax);
            (void)TemporalPass->Set<&SnAPI::Renderer::TemporalResolvePass::Settings::MotionFeedbackMax>(FeedbackMax);
            (void)TemporalPass->Set<&SnAPI::Renderer::TemporalResolvePass::Settings::ColorClipThreshold>(
                std::max(0.0f, TaaSettings->ClampStrength));
        }
    }

    if (const auto* ToneMapSettings = ResolveRendererFeatureSettings(m_rendererNew->ToneMapFeatureSettings, ViewportID))
    {
        const auto ApplyToneMap = [ToneMapSettings](SnAPI::Renderer::TonemapPass* Pass)
        {
            if (!Pass)
            {
                return;
            }

            const float Operator = ToneMapSettings->EnableAgX ? 2.0f : (ToneMapSettings->EnableACES ? 1.0f : 0.0f);
            const float Saturation = ToneMapSettings->EnableAgX ? ToneMapSettings->AgXSaturation : ToneMapSettings->AcesSaturation;
            const float Contrast = ToneMapSettings->EnableAgX ? ToneMapSettings->AgXContrast : 1.0f;
            (void)Pass->Set<&SnAPI::Renderer::TonemapPass::Settings::Exposure>(ToneMapSettings->Exposure);
            (void)Pass->Set<&SnAPI::Renderer::TonemapPass::Settings::Contrast>(std::max(0.0f, Contrast));
            (void)Pass->Set<&SnAPI::Renderer::TonemapPass::Settings::Saturation>(std::max(0.0f, Saturation));
            (void)Pass->Set<&SnAPI::Renderer::TonemapPass::Settings::Operator>(Operator);
            (void)Pass->Set<&SnAPI::Renderer::TonemapPass::Settings::AgxLook>(ToneMapSettings->AgXPivot);
            (void)Pass->Set<&SnAPI::Renderer::TonemapPass::Settings::HdrShoulderStrength>(
                std::max(0.0f, ToneMapSettings->AcesWhitePoint));
        };
        ApplyToneMap(FindRendererNewPass<SnAPI::Renderer::TonemapPass>(Runtime, "DeferredTonemapExternalTargetPass"));
        ApplyToneMap(FindRendererNewPass<SnAPI::Renderer::TonemapPass>(Runtime, "DeferredTonemapDebugColorPass"));
    }
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

bool RendererSystem::LoadDefaultFont(const std::string& FontPath, const std::uint32_t FontSize)
{
    GameLockGuard Lock(m_mutex);
    if (!m_initialized || !m_rendererNew || !m_rendererNew->Runtime)
    {
        return false;
    }

    m_settings.DefaultFontPath = FontPath;
    m_settings.DefaultFontSize = std::max<std::uint32_t>(1u, FontSize);
    m_rendererNew->DefaultTextFont = {};
    m_rendererNew->DefaultTextFontPath.clear();
    return EnsureRendererNewDefaultTextFontUnlocked();
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
    return m_rendererNew && m_rendererNew->DefaultTextFont.Valid();
}

#if defined(SNAPI_GF_ENABLE_UI)
SnAPI::UI::IFontMetrics* RendererSystem::EnsureDefaultUiFontMetrics()
{
    GameLockGuard Lock(m_mutex);
    if (!m_initialized || !m_rendererNew || !m_rendererNew->Runtime)
    {
        return nullptr;
    }

    if (!EnsureRendererNewDefaultTextFontUnlocked())
    {
        return nullptr;
    }

    m_rendererNew->UiFontMetrics.Bind(
        &m_rendererNew->Runtime->Text(),
        m_rendererNew->DefaultTextFont,
        static_cast<float>(m_rendererNew->DefaultTextFontSize));
    return &m_rendererNew->UiFontMetrics;
}

bool RendererSystem::QueueUiRenderPackets(const std::uint64_t ViewportID,
                                          SnAPI::UI::UIContext& Context,
                                          const SnAPI::UI::RenderPacketList& Packets)
{
    GameLockGuard Lock(m_mutex);
    if (!m_initialized || ViewportID == 0 || !m_rendererNew || !m_rendererNew->Runtime)
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

        const auto TargetIt = m_rendererNew->ViewportOutputs.find(BindingIt->second.SourceViewportID);
        if (TargetIt == m_rendererNew->ViewportOutputs.end())
        {
            return {};
        }

        return TargetIt->second.Texture();
    };

    (void)EnsureRendererNewDefaultTextFontUnlocked();
    float RenderTargetWidth = Context.GetViewportSize().W;
    float RenderTargetHeight = Context.GetViewportSize().H;
    float OverlayProjectionWidth = RenderTargetWidth;
    float OverlayProjectionHeight = RenderTargetHeight;
    if (ViewportID != 1u)
    {
        const auto OutputIt = m_rendererNew->ViewportOutputs.find(ViewportID);
        if (OutputIt != m_rendererNew->ViewportOutputs.end())
        {
            const auto Extent = OutputIt->second.Extent();
            RenderTargetWidth = static_cast<float>(Extent.Width);
            RenderTargetHeight = static_cast<float>(Extent.Height);
        }

        // Renderer.New's default frame pipeline is configured once before the frame opens.
        // Embedded viewport targets are rendered inside that same frame, so the UI overlay
        // pass projects pixels using the configured pipeline output extent, not the offscreen
        // target extent. Scale into that projection space here; SnAPI.UI still owns DPI.
        OverlayProjectionWidth = static_cast<float>(std::max(1u, m_rendererNew->ViewExtents.OutputExtent.Width));
        OverlayProjectionHeight = static_cast<float>(std::max(1u, m_rendererNew->ViewExtents.OutputExtent.Height));
    }

    if (ShouldLogViewportOverlaySizes())
    {
        const auto ContextScreenRect = Context.GetScreenRect();
        const auto ContextViewportSize = Context.GetViewportSize();
        std::cerr << "[GameFramework][Renderer.New][UIQueue]"
                  << " viewport=" << ViewportID
                  << " contextScreen=(" << ContextScreenRect.X << ',' << ContextScreenRect.Y << ' '
                  << ContextScreenRect.W << 'x' << ContextScreenRect.H << ')'
                  << " contextViewport=" << ContextViewportSize.W << 'x' << ContextViewportSize.H
                  << " dpi=" << Context.GetDpiScale()
                  << " renderTarget=" << RenderTargetWidth << 'x' << RenderTargetHeight
                  << " overlayProjection=" << OverlayProjectionWidth << 'x' << OverlayProjectionHeight
                  << " packets=" << Packets.Packets().size()
                  << '\n';
    }

    auto OverlayPacket = BuildRendererUiOverlayPacket(
        *m_rendererNew->Runtime,
        Context,
        Packets,
        ResolveTexture,
        m_rendererNew->DefaultTextFont,
        static_cast<float>(std::max<std::uint32_t>(1u, m_rendererNew->DefaultTextFontSize)),
        OverlayProjectionWidth,
        OverlayProjectionHeight);
    if (OverlayPacket.Empty())
    {
        if (ShouldLogRendererNewUiBridge())
        {
            std::cerr << "[GameFramework][Renderer.New][UI] queue viewport=" << ViewportID
                      << " sourcePackets=" << Packets.Packets().size()
                      << " translated empty\n";
        }
        return true;
    }

    SnAPI::Renderer::RenderOverlayPacket* PendingOverlay = &m_rendererNew->SurfaceOverlay;
    if (ViewportID != 1u)
    {
        const auto OutputIt = m_rendererNew->ViewportOutputs.find(ViewportID);
        if (OutputIt == m_rendererNew->ViewportOutputs.end())
        {
            return false;
        }
        PendingOverlay = &OutputIt->second.Overlay();
    }

    auto& TargetPackets = PendingOverlay->Ui.DrawPackets;
    TargetPackets.reserve(TargetPackets.size() + OverlayPacket.Ui.DrawPackets.size());
    for (auto& DrawPacket : OverlayPacket.Ui.DrawPackets)
    {
        TargetPackets.emplace_back(std::move(DrawPacket));
    }
    PendingOverlay->TextSubmissions.reserve(PendingOverlay->TextSubmissions.size() + OverlayPacket.TextSubmissions.size());
    for (auto& TextSubmission : OverlayPacket.TextSubmissions)
    {
        PendingOverlay->TextSubmissions.emplace_back(std::move(TextSubmission));
    }
    PendingOverlay->PreShapedTextSubmissions.reserve(
        PendingOverlay->PreShapedTextSubmissions.size() + OverlayPacket.PreShapedTextSubmissions.size());
    for (auto& TextSubmission : OverlayPacket.PreShapedTextSubmissions)
    {
        PendingOverlay->PreShapedTextSubmissions.emplace_back(std::move(TextSubmission));
    }

    if (ShouldLogRendererNewUiBridge())
    {
        std::cerr << "[GameFramework][Renderer.New][UI] queue viewport=" << ViewportID
                  << " drawPackets=" << PendingOverlay->Ui.DrawPacketCount()
                  << " instances=" << PendingOverlay->Ui.InstanceCount()
                  << " text=" << PendingOverlay->TextSubmissionCount()
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

bool RendererSystem::UnregisterExternalViewportUiTexture(const SnAPI::UI::UIContext& Context, const std::uint32_t TextureId)
{
    GameLockGuard Lock(m_mutex);
    return m_uiExternalTextureBindings.erase(UiTextureCacheKey{&Context, TextureId}) > 0u;
}

void RendererSystem::FlushQueuedUiPackets()
{
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
        ClearRendererNewQueuedOverlaysUnlocked();
        m_rendererNewDebugLines.clear();
#if defined(SNAPI_GF_ENABLE_UI)
        m_uiQueuedRects.clear();
        m_uiPacketsQueuedThisFrame = false;
#endif
        ++m_taaFrameIndex;
        return;
    }

    m_rendererNew->PumpWindowEvents();
    if (m_rendererNew->Window.Valid())
    {
        m_mainWindow.Resize(m_rendererNew->Window.Width(), m_rendererNew->Window.Height());
        m_mainWindow.Open(!m_rendererNew->Window.ShouldClose());
    }
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

    FlushQueuedText();
#if defined(SNAPI_GF_ENABLE_UI)
    FlushQueuedUiPackets();
#endif
    WarmRendererNewQueuedOverlayTextUnlocked();

    auto BeginFrameResult = m_rendererNew->Runtime->BeginFrame(SnAPI::Renderer::FrameBeginDesc{
        .FrameIndex = FrameIndex,
        .DeltaTimeSeconds = DeltaSeconds,
        .Timing = FrameTiming});
    if (BeginFrameResult.Failed())
    {
        std::cerr << "RendererSystem failed during BeginFrame: " << BeginFrameResult.Error().Message << '\n';
        ClearRendererNewQueuedOverlaysUnlocked();
        m_rendererNewDebugLines.clear();
        m_initialized = false;
        return;
    }

    FlushRendererNewDebugLinesUnlocked();

    for (const auto& [ViewportID, Preset] : m_renderViewportFeatureProfiles)
    {
        if (!ShouldRenderRendererNewViewportPreset(Preset))
        {
            continue;
        }

        const auto TargetIt = m_rendererNew->ViewportOutputs.find(ViewportID);
        if (TargetIt == m_rendererNew->ViewportOutputs.end() ||
            !TargetIt->second.Enabled() ||
            !TargetIt->second.HasTarget())
        {
            continue;
        }

        const auto TargetExtent = TargetIt->second.Extent();
        auto View = SnAPI::Renderer::RenderView{
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
        if (auto Camera = TargetIt->second.Camera())
        {
            Camera->ApplyToView(View);
        }
        else if (m_activeCamera)
        {
            m_activeCamera->ApplyToView(View);
        }
        const auto* Overlay = TargetIt->second.HasOverlay() ? &TargetIt->second.Overlay() : nullptr;
        ApplyRendererNewFeatureSettingsUnlocked(ViewportID);
        auto ViewportRenderResult = m_rendererNew->Runtime->RenderSceneToTarget(SnAPI::Renderer::RenderSceneToTargetDesc{
            .Scene = m_rendererNew->Scene,
            .Target = TargetIt->second.Target(),
            .View = View,
            .Profile = SnAPI::Renderer::DeferredProfile::Id,
            .FrameGraphOutputResourceName = "PresentTarget",
            .Overlay = Overlay});
        if (ViewportRenderResult.Failed())
        {
            std::cerr << "RendererSystem failed during RenderSceneToTarget for viewport " << ViewportID
                      << ": " << ViewportRenderResult.Error().Message << '\n';
        }
    }

    auto View = SnAPI::Renderer::RenderView{
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
    if (m_activeCamera)
    {
        m_activeCamera->ApplyToView(View);
    }
    const auto* MainOverlay = !m_rendererNew->SurfaceOverlay.Empty() ? &m_rendererNew->SurfaceOverlay : nullptr;
    ApplyRendererNewFeatureSettingsUnlocked(1u);
    auto RenderResult = m_rendererNew->Runtime->RenderSceneToSurface(SnAPI::Renderer::RenderSceneToSurfaceDesc{
        .Scene = m_rendererNew->Scene,
        .Surface = m_rendererNew->Surface,
        .View = View,
        .Profile = SnAPI::Renderer::DeferredProfile::Id,
        .FrameGraphOutputResourceName = "PresentTarget",
        .Overlay = MainOverlay});
    if (RenderResult.Failed())
    {
        std::cerr << "RendererSystem failed during RenderSceneToSurface: " << RenderResult.Error().Message << '\n';
        (void)m_rendererNew->Runtime->EndFrame();
        ClearRendererNewQueuedOverlaysUnlocked();
        m_rendererNewDebugLines.clear();
        m_initialized = false;
        return;
    }

    auto EndFrameResult = m_rendererNew->Runtime->EndFrame();
    if (EndFrameResult.Failed())
    {
        std::cerr << "RendererSystem failed during EndFrame: " << EndFrameResult.Error().Message << '\n';
        ClearRendererNewQueuedOverlaysUnlocked();
        m_rendererNewDebugLines.clear();
        m_initialized = false;
        return;
    }

    ClearRendererNewQueuedOverlaysUnlocked();
    m_rendererNew->FrameIndex = FrameIndex;
    ++m_taaFrameIndex;
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
    if (m_rendererNew && !m_textQueue.empty() && EnsureRendererNewDefaultTextFontUnlocked())
    {
        auto& Overlay = m_rendererNew->SurfaceOverlay;
        Overlay.TextSubmissions.reserve(Overlay.TextSubmissions.size() + m_textQueue.size());
        const float SizePixels = static_cast<float>(std::max<std::uint32_t>(1u, m_rendererNew->DefaultTextFontSize));
        for (const auto& Request : m_textQueue)
        {
            if (Request.Text.empty())
            {
                continue;
            }

            Overlay.TextSubmissions.push_back(SnAPI::Renderer::TextStringSubmissionDesc{
                .TextUtf8 = Request.Text,
                .Style = SnAPI::Renderer::TextStyleDesc{
                    .Font = m_rendererNew->DefaultTextFont,
                    .SizePixels = SizePixels,
                    .RasterizationMode = SnAPI::Renderer::ETextGlyphRasterizationMode::Mtsdf},
                .Layout = SnAPI::Renderer::TextLayoutDesc{
                    .Origin = SnAPI::Renderer::Vec2{
                        static_cast<double>(Request.X),
                        static_cast<double>(Request.Y + SizePixels * 0.8f)},
                    .Domain = SnAPI::Renderer::ETextRenderDomain::Ui},
                .MaterialInstance = SnAPI::Renderer::MaterialInstanceHandle{1u},
                .Color = {1.0f, 1.0f, 1.0f, 1.0f}});
        }
    }

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
        m_mainWindow.Reset();
        return true;
    }

    const auto Width = ResolveWindowPixelExtent(m_settings.WindowWidth);
    const auto Height = ResolveWindowPixelExtent(m_settings.WindowHeight);
    auto WindowResult = CreateRendererNewHostWindow(Width, Height, m_settings.WindowTitle);
    if (WindowResult.Failed())
    {
        std::cerr << "RendererSystem failed to create renderer host window: " << WindowResult.Error().Message << '\n';
        return false;
    }

    auto Window = std::move(WindowResult).Value();
    auto SurfaceResult = m_rendererNew->Runtime->CreateSurface(SnAPI::Renderer::RenderSurfaceCreateInfo{
        .Window = Window.Window(),
        .NativeSurface = Window.NativeSurface(),
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
        SnAPI::Renderer::Extent2D{.Width = Window.Width(), .Height = Window.Height()});
    const auto PresentationProfile = PresentationProfileResult.Value();
    auto ConfigureResult = m_rendererNew->Runtime->ConfigureFramePipeline(
        BuildRendererNewFramePipelineSettings(ViewExtents, PresentationProfile));
    if (ConfigureResult.Failed())
    {
        std::cerr << "RendererSystem failed to configure frame pipeline: " << ConfigureResult.Error().Message << '\n';
        return false;
    }

    std::cerr << "RendererSystem renderer host window: "
              << RendererNewHostWindowSystemName(Window.WindowSystem) << '\n';
    m_rendererNew->Window = std::move(Window);
    m_mainWindow.Configure(
        m_rendererNew->Window.Width(),
        m_rendererNew->Window.Height(),
        m_rendererNew->Window.Window(),
        m_rendererNew->Window.NativeSurface(),
        !m_rendererNew->Window.ShouldClose());
    m_rendererNew->Surface = Surface;
    m_rendererNew->PresentationProfile = PresentationProfile;
    m_rendererNew->ViewExtents = ViewExtents;
    m_rendererNew->SurfaceValid = true;
    return true;
}

bool RendererSystem::ApplyDefaultFeatureProfile()
{
    return ApplyRenderViewportFeatureProfileUnlocked(1u, EGameRenderFeatureProfile::DefaultWorld, true);
}

void RendererSystem::ResetPassPointers()
{
}
} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
