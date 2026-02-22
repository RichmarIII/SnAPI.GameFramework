#include "Editor/EditorViewportBinding.h"

#include "GameRuntime.h"
#include "RendererSystem.h"
#include "UISystem.h"
#include "World.h"

#include <algorithm>
#include <cmath>

#include "WindowBase.hpp"

namespace SnAPI::GameFramework::Editor
{
namespace
{
constexpr float kMinExtent = 1.0f;
constexpr float kChangeEpsilon = 0.25f;
} // namespace

Result EditorViewportBinding::Initialize(GameRuntime& Runtime, std::string ViewportName)
{
#if !defined(SNAPI_GF_ENABLE_RENDERER) || !defined(SNAPI_GF_ENABLE_UI)
    (void)Runtime;
    (void)ViewportName;
    return std::unexpected(MakeError(EErrorCode::NotSupported,
                                     "Editor viewport binding requires renderer and UI support"));
#else
    Shutdown(&Runtime);

    auto* WorldPtr = Runtime.WorldPtr();
    if (!WorldPtr)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Game runtime is not initialized"));
    }

    auto& Renderer = WorldPtr->Renderer();
    auto& UI = WorldPtr->UI();
    if (!Renderer.IsInitialized() || !UI.IsInitialized())
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Renderer or UI system is not initialized"));
    }

    if (!ViewportName.empty())
    {
        m_viewportName = std::move(ViewportName);
    }

    m_rootContextId = UI.RootContextId();
    if (m_rootContextId == 0)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Root UI context is not available"));
    }

    float Width = 0.0f;
    float Height = 0.0f;
    if (!ResolveViewportSize(Runtime, Width, Height))
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Failed to resolve editor viewport size"));
    }

    (void)Renderer.UseDefaultRenderViewport(false);

    std::uint64_t ViewportId = 0;
    if (!Renderer.CreateRenderViewport(m_viewportName,
                                       0.0f,
                                       0.0f,
                                       Width,
                                       Height,
                                       static_cast<std::uint32_t>(std::round(Width)),
                                       static_cast<std::uint32_t>(std::round(Height)),
                                       nullptr,
                                       true,
                                       ViewportId))
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to create root editor render viewport"));
    }

    m_viewportId = ViewportId;
    m_lastWidth = Width;
    m_lastHeight = Height;
    m_appliedRenderWidth = std::max<std::uint32_t>(1u, static_cast<std::uint32_t>(std::round(Width)));
    m_appliedRenderHeight = std::max<std::uint32_t>(1u, static_cast<std::uint32_t>(std::round(Height)));
    m_pendingRenderWidth = m_appliedRenderWidth;
    m_pendingRenderHeight = m_appliedRenderHeight;
    m_hasPendingRenderExtentResize = false;

    if (!Renderer.RegisterRenderViewportPassGraph(m_viewportId, ERenderViewportPassGraphPreset::UiPresentOnly))
    {
        Shutdown(&Runtime);
        return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to register UiPresentOnly root pass graph"));
    }

    if (auto BindResult = Runtime.BindViewportWithUI(m_viewportId, m_rootContextId); !BindResult)
    {
        Shutdown(&Runtime);
        return std::unexpected(BindResult.error());
    }

    (void)UI.SetViewportSize(Width, Height);
    (void)UI.SetContextScreenRect(m_rootContextId, 0.0f, 0.0f, Width, Height);
    return Ok();
#endif
}

void EditorViewportBinding::Shutdown(GameRuntime* Runtime)
{
#if !defined(SNAPI_GF_ENABLE_RENDERER) || !defined(SNAPI_GF_ENABLE_UI)
    (void)Runtime;
#else
    if (Runtime && Runtime->WorldPtr())
    {
        auto& Renderer = Runtime->World().Renderer();
        if (m_viewportId != 0)
        {
            (void)Runtime->UnbindViewportFromUI(m_viewportId);
            if (Renderer.IsInitialized())
            {
                (void)Renderer.DestroyRenderViewport(m_viewportId);
            }
        }
    }
#endif

    m_viewportId = 0;
    m_rootContextId = 0;
    m_lastWidth = 0.0f;
    m_lastHeight = 0.0f;
    m_appliedRenderWidth = 0;
    m_appliedRenderHeight = 0;
    m_pendingRenderWidth = 0;
    m_pendingRenderHeight = 0;
    m_hasPendingRenderExtentResize = false;
}

bool EditorViewportBinding::SyncToWindow(GameRuntime& Runtime)
{
#if !defined(SNAPI_GF_ENABLE_RENDERER) || !defined(SNAPI_GF_ENABLE_UI)
    (void)Runtime;
    return false;
#else
    if (!IsInitialized())
    {
        return false;
    }

    auto* WorldPtr = Runtime.WorldPtr();
    if (!WorldPtr)
    {
        return false;
    }

    auto& Renderer = WorldPtr->Renderer();
    auto& UI = WorldPtr->UI();
    if (!Renderer.IsInitialized() || !UI.IsInitialized())
    {
        return false;
    }

    // Editor flow is fully explicit-viewport based. Keep the renderer default viewport
    // disabled so no implicit fullscreen viewport can participate in rendering.
    (void)Renderer.UseDefaultRenderViewport(false);

    if (!Renderer.HasRenderViewport(m_viewportId))
    {
        const std::string SavedName = m_viewportName;
        const Result RecreateResult = Initialize(Runtime, SavedName);
        return static_cast<bool>(RecreateResult);
    }

    if (!EnsureUiBinding(Runtime))
    {
        return false;
    }

    float Width = 0.0f;
    float Height = 0.0f;
    if (!ResolveViewportSize(Runtime, Width, Height))
    {
        return false;
    }

    const auto ToRenderExtent = [](const float Value) {
        return std::max<std::uint32_t>(1u, static_cast<std::uint32_t>(std::round(Value)));
    };

    const bool NeedsResize = std::abs(Width - m_lastWidth) > kChangeEpsilon || std::abs(Height - m_lastHeight) > kChangeEpsilon;
    if (NeedsResize)
    {
        m_lastWidth = Width;
        m_lastHeight = Height;
        (void)UI.SetViewportSize(Width, Height);
        (void)UI.SetContextScreenRect(m_rootContextId, 0.0f, 0.0f, Width, Height);
    }

    const std::uint32_t DesiredRenderWidth = ToRenderExtent(Width);
    const std::uint32_t DesiredRenderHeight = ToRenderExtent(Height);
    std::uint32_t RenderWidth = m_appliedRenderWidth > 0 ? m_appliedRenderWidth : DesiredRenderWidth;
    std::uint32_t RenderHeight = m_appliedRenderHeight > 0 ? m_appliedRenderHeight : DesiredRenderHeight;
    bool ApplyRenderExtent = false;
    bool IsPointerPressed = false;
#if defined(SNAPI_GF_ENABLE_INPUT)
    if (const auto* Snapshot = WorldPtr->Input().Snapshot())
    {
        IsPointerPressed = Snapshot->MouseButtonDown(SnAPI::Input::EMouseButton::Left);
    }
#endif

    if (DesiredRenderWidth != m_appliedRenderWidth || DesiredRenderHeight != m_appliedRenderHeight)
    {
        const bool PendingChanged = !m_hasPendingRenderExtentResize
                                    || m_pendingRenderWidth != DesiredRenderWidth
                                    || m_pendingRenderHeight != DesiredRenderHeight;
        if (PendingChanged)
        {
            m_pendingRenderWidth = DesiredRenderWidth;
            m_pendingRenderHeight = DesiredRenderHeight;
            m_hasPendingRenderExtentResize = true;
        }

        if (!IsPointerPressed)
        {
            RenderWidth = m_pendingRenderWidth;
            RenderHeight = m_pendingRenderHeight;
            ApplyRenderExtent = true;
        }
    }
    else
    {
        m_pendingRenderWidth = DesiredRenderWidth;
        m_pendingRenderHeight = DesiredRenderHeight;
        m_hasPendingRenderExtentResize = false;
        RenderWidth = DesiredRenderWidth;
        RenderHeight = DesiredRenderHeight;
    }

    if (!NeedsResize && !ApplyRenderExtent)
    {
        return true;
    }

    const bool Updated = Renderer.UpdateRenderViewport(m_viewportId,
                                                       m_viewportName,
                                                       0.0f,
                                                       0.0f,
                                                       Width,
                                                       Height,
                                                       RenderWidth,
                                                       RenderHeight,
                                                       nullptr,
                                                       true);
    if (Updated && (ApplyRenderExtent || m_appliedRenderWidth == 0 || m_appliedRenderHeight == 0))
    {
        m_appliedRenderWidth = RenderWidth;
        m_appliedRenderHeight = RenderHeight;
        m_pendingRenderWidth = RenderWidth;
        m_pendingRenderHeight = RenderHeight;
        m_hasPendingRenderExtentResize = false;
    }
    return Updated;
#endif
}

bool EditorViewportBinding::ResolveViewportSize(GameRuntime& Runtime, float& OutWidth, float& OutHeight) const
{
    OutWidth = 0.0f;
    OutHeight = 0.0f;

#if !defined(SNAPI_GF_ENABLE_RENDERER) || !defined(SNAPI_GF_ENABLE_UI)
    (void)Runtime;
    return false;
#else
    auto* WorldPtr = Runtime.WorldPtr();
    if (!WorldPtr)
    {
        return false;
    }

    const auto* Window = WorldPtr->Renderer().Window();
    if (Window)
    {
        const auto WindowSize = Window->Size();
        if (std::isfinite(WindowSize.x()) && std::isfinite(WindowSize.y()) && WindowSize.x() > kMinExtent &&
            WindowSize.y() > kMinExtent)
        {
            OutWidth = WindowSize.x();
            OutHeight = WindowSize.y();
            return true;
        }
    }

    const auto& UiSettings = WorldPtr->UI().Settings();
    if (!std::isfinite(UiSettings.ViewportWidth) || !std::isfinite(UiSettings.ViewportHeight))
    {
        return false;
    }

    OutWidth = std::max(kMinExtent, UiSettings.ViewportWidth);
    OutHeight = std::max(kMinExtent, UiSettings.ViewportHeight);
    return true;
#endif
}

bool EditorViewportBinding::EnsureUiBinding(GameRuntime& Runtime) const
{
#if !defined(SNAPI_GF_ENABLE_RENDERER) || !defined(SNAPI_GF_ENABLE_UI)
    (void)Runtime;
    return false;
#else
    if (!IsInitialized())
    {
        return false;
    }

    const auto BoundContext = Runtime.BoundUIContext(m_viewportId);
    if (BoundContext && *BoundContext == m_rootContextId)
    {
        return true;
    }

    const auto RebindResult = Runtime.BindViewportWithUI(m_viewportId, m_rootContextId);
    return static_cast<bool>(RebindResult);
#endif
}

} // namespace SnAPI::GameFramework::Editor
