#include "Editor/EditorViewportBinding.h"

#include "GameRuntime.h"
#include "RendererSystem.h"
#include "UISystem.h"
#include "World.h"
#include "Rendering/GameRenderCamera.h"

#include <algorithm>
#include <cmath>


namespace SnAPI::GameFramework::Editor
{
namespace
{
constexpr float kMinExtent = 1.0f;
constexpr float kChangeEpsilon = 0.25f;
constexpr std::uint64_t kSurfaceViewportId = 1u;
} // namespace

Result EditorViewportBinding::Initialize(GameRuntime& Runtime, std::string ViewportName)
{
#if !defined(SNAPI_GF_ENABLE_RENDERER) || !defined(SNAPI_GF_ENABLE_UI)
    (void)Runtime;
    (void)ViewportName;
    return std::unexpected(MakeError(EErrorCode::NotSupported,
                                     "Editor viewport binding requires renderer and UI support"));
#else
    (void)ViewportName;
    Shutdown(nullptr);

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

    if (!Renderer.UseDefaultRenderViewport(true) || !Renderer.HasRenderViewport(kSurfaceViewportId))
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to enable renderer surface viewport"));
    }

    m_viewportId = kSurfaceViewportId;
    m_lastWidth = Width;
    m_lastHeight = Height;

    if (auto BindResult = Runtime.BindViewportWithUI(m_viewportId, m_rootContextId); !BindResult)
    {
        Shutdown(nullptr);
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
    (void)Runtime;
#endif

    m_viewportId = 0;
    m_rootContextId = 0;
    m_lastWidth = 0.0f;
    m_lastHeight = 0.0f;
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

    if (!Renderer.UseDefaultRenderViewport(true))
    {
        return false;
    }

    if (m_viewportId != kSurfaceViewportId || !Renderer.HasRenderViewport(kSurfaceViewportId))
    {
        const Result RecreateResult = Initialize(Runtime, {});
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

    const bool NeedsResize = std::abs(Width - m_lastWidth) > kChangeEpsilon || std::abs(Height - m_lastHeight) > kChangeEpsilon;
    if (NeedsResize)
    {
        m_lastWidth = Width;
        m_lastHeight = Height;
        (void)UI.SetViewportSize(Width, Height);
        (void)UI.SetContextScreenRect(m_rootContextId, 0.0f, 0.0f, Width, Height);
    }

    return true;
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
