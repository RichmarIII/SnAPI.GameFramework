#pragma once

#if defined(SNAPI_GF_ENABLE_UI)

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "Expected.h"
#include "GameThreading.h"
#include "TypeName.h"

#include <UIContext.h>
#include "ReflectionAnnotations.h"

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Bootstrap settings for world-owned SnAPI.UI integration.
 *
 * `UIBootstrapSettings` defines the initial logical root-viewport metrics used when
 * `UISystem` creates its root `UIContext`. Child contexts inherit their initial theme
 * and screen rectangle from their parent at creation time.
 *
 * Units:
 * - `ViewportWidth` and `ViewportHeight` are logical UI units, not physical framebuffer pixels
 * - `DpiScaleOverride` is a unitless multiplier applied to UI layout and rendering
 *
 * Validation:
 * - width and height must be finite and greater than zero
 * - `DpiScaleOverride`, when provided, must be finite and greater than zero
 *
 * @see UISystem
 */
SnType()
struct UIBootstrapSettings
{
    SnField(SnKey("ViewportWidth"))
    float ViewportWidth = 1600.0f; /**< @brief Initial logical viewport width in UI units; must be finite and > 0. */
    SnField(SnKey("ViewportHeight"))
    float ViewportHeight = 900.0f; /**< @brief Initial logical viewport height in UI units; must be finite and > 0. */
    std::optional<float> DpiScaleOverride{}; /**< @brief Optional explicit DPI scale override; when nullopt, UIContext defaults/environment values are preserved. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief World-owned UI system with parent/child `UIContext` graph support.
 *
 * `UISystem` owns the UI context tree for a `World`. It creates one root context at
 * initialization time and allows additional child contexts to be created explicitly for
 * things like embedded render viewports, modal shells, overlays, or editor panels that
 * need independent input routing and render-packet generation.
 *
 * Why this abstraction exists:
 * - to bind UI lifetime to the world instead of a process-global singleton
 * - to support multiple UI contexts while keeping one authoritative input-routing surface
 * - to provide explicit bindings between UI contexts and renderer viewports
 *
 * Core semantics:
 * - contexts are addressed by stable `ContextId` values for the lifetime of the context
 * - initialization always creates a root context; the root cannot be destroyed independently
 * - child contexts inherit theme and initial screen rect from their parent at creation time
 * - viewport/context bindings are one-to-one and replacing either side automatically unbinds prior mappings
 * - render packet generation operates on context snapshots known at call time
 *
 * Ownership and lifetime:
 * - Owned by `World`.
 * - Owns every `UIContext` it creates.
 * - Pointers returned by `Context(...)` are borrowed and invalidated when that context is destroyed or the system shuts down.
 *
 * Threading model:
 * - Main-thread only for normal usage.
 * - Cross-thread work should be marshaled via `EnqueueTask(...)`.
 *
 * Performance notes:
 * - `Tick(...)` walks contexts in parent-before-child order.
 * - Pointer and wheel input routing can touch multiple contexts to preserve capture and hover semantics.
 * - `BuildBoundViewportRenderPackets(...)` builds packets for every currently bound context and can allocate proportionally to binding count.
 *
 * @see World
 * @see UIBootstrapSettings
 */
SnType()
class UISystem final : public ITaskDispatcher
{
public:
    using WorkTask = std::function<void(UISystem&)>;
    using CompletionTask = std::function<void(const TaskHandle&)>;
    using ContextId = std::uint64_t;
    using ViewportId = std::uint64_t;

    /**
     * @brief Render-packet batch produced for one bound viewport/context pair.
     *
     * The packet list is owned by the batch value and is typically consumed immediately
     * by renderer integration code after `BuildBoundViewportRenderPackets(...)`.
     */
    struct ViewportPacketBatch
    {
        ViewportId Viewport = 0;
        ContextId Context = 0;
        SnAPI::UI::UIContext* ContextPtr = nullptr; /**< @brief Non-owning context pointer valid while the context remains alive. */
        SnAPI::UI::RenderPacketList Packets{};
    };

    /**
     * @brief Snapshot of a one-to-one viewport/context binding.
     */
    struct ViewportBinding
    {
        ViewportId Viewport = 0;
        ContextId Context = 0;
    };

    /** @brief Construct an uninitialized UI system. */
    UISystem() = default;
    /** @brief Destructor; shuts down and releases all owned contexts. */
    ~UISystem() override;

    UISystem(const UISystem&) = delete;
    UISystem& operator=(const UISystem&) = delete;

    UISystem(UISystem&& Other) noexcept;
    UISystem& operator=(UISystem&& Other) noexcept;

    /**
     * @brief Initialize with default bootstrap settings.
     * @return Success or error.
     * @post On success, a root context exists and `RootContextId()` is non-zero.
     */
    Result Initialize();
    /**
     * @brief Initialize with explicit bootstrap settings.
     * @param SettingsValue UI bootstrap settings copied into the subsystem.
     * @return Success or error.
     * @post On success, a new root context exists and prior context state is discarded.
     * @warning Reinitializes the system and destroys all previously owned contexts.
     */
    Result Initialize(const UIBootstrapSettings& SettingsValue);
    /**
     * @brief Shutdown and destroy all owned UI contexts.
     * @remarks Safe to call repeatedly.
     */
    void Shutdown();
    /**
     * @brief Check whether the UI system is initialized.
     * @return `true` when a live root context exists.
     */
    SnFunction(SnKey("IsInitialized"))
    bool IsInitialized() const;

    /**
     * @brief Tick all live contexts once in parent-before-child order.
     * @param DeltaSeconds Frame delta time in seconds.
     */
    void Tick(float DeltaSeconds);

    /**
     * @brief Build render packets for one UI context.
     * @param Context Target context id.
     * @param OutPackets Destination packet list written by the target context.
     * @return Success or error.
     * @pre The target context must exist.
     */
    Result BuildRenderPackets(ContextId Context, SnAPI::UI::RenderPacketList& OutPackets);
    /**
     * @brief Build render packets for every currently bound viewport/context pair.
     * @param OutBatches Destination vector replaced with one batch per live binding.
     * @return Success or error.
     * @remarks Bindings whose contexts no longer exist are skipped.
     */
    Result BuildBoundViewportRenderPackets(std::vector<ViewportPacketBatch>& OutBatches);

    /**
     * @brief Route a pointer event through the active context tree.
     * @param EventValue Pointer event in UI screen coordinates.
     * @remarks
     * The UI context owns input coordinate normalization before hit testing.
     * Pointer routing preserves capture semantics and may dispatch to multiple contexts
     * so hover/capture state can clear correctly when the pointer leaves a context.
     */
    void PushInput(const SnAPI::UI::PointerEvent& EventValue);
    /**
     * @brief Route a key event through all live contexts.
     * @param EventValue Key event to dispatch.
     */
    void PushInput(const SnAPI::UI::KeyEvent& EventValue);
    /**
     * @brief Route a text-input event through all live contexts.
     * @param EventValue Text input event to dispatch.
     */
    void PushInput(const SnAPI::UI::TextInputEvent& EventValue);
    /**
     * @brief Route a wheel event through the active context tree.
     * @param EventValue Wheel event in UI screen coordinates.
     * @remarks The root context is always included first so global overlays can respond.
     */
    void PushInput(const SnAPI::UI::WheelEvent& EventValue);

    /**
     * @brief Normalize a platform input point to root UI screen coordinates.
     * @param Point UI screen-space point, typically produced by the platform/input bridge.
     * @return Root-screen point, or the original point when the root context is unavailable.
     */
    [[nodiscard]] SnAPI::UI::UIPoint MapInputPointToScreenPoint(SnAPI::UI::UIPoint Point) const;

    /**
     * @brief Resize the logical root viewport.
     * @param Width Logical width in UI units. Must be finite and greater than zero.
     * @param Height Logical height in UI units. Must be finite and greater than zero.
     * @return Success or error.
     * @remarks Updates the root context viewport and its stored screen rect.
     */
    Result SetViewportSize(float Width, float Height);
    /**
     * @brief Override DPI scale for all live contexts.
     * @param Scale Unitless DPI multiplier. Must be finite and greater than zero.
     * @return Success or error.
     * @remarks Applies immediately to existing contexts and becomes part of the stored settings snapshot.
     */
    Result SetDpiScale(float Scale);

    /**
     * @brief Access the root context id.
     * @return Root context id, or `0` when uninitialized.
     */
    SnFunction(SnKey("RootContextId"))
    ContextId RootContextId() const;
    /**
     * @brief Create a child context under an existing parent context.
     * @param ParentContext Parent context id.
     * @param OutContextId Receives the created child context id on success; set to `0` on failure.
     * @return Success or error.
     * @post On success, the child context inherits the parent theme and initial screen rect.
     */
    Result CreateContext(ContextId ParentContext, ContextId& OutContextId);
    /**
     * @brief Destroy one non-root context and all of its descendants.
     * @param Context Context id to destroy.
     * @return Success or error.
     * @warning Destroying a context also removes any viewport binding involving that context.
     */
    Result DestroyContext(ContextId Context);

    /**
     * @brief Access a live context by id.
     * @param Context Context id to resolve.
     * @return Non-owning context pointer or `nullptr`.
     */
    SnAPI::UI::UIContext* Context(ContextId Context);
    /**
     * @brief Access a live context by id (const).
     * @param Context Context id to resolve.
     * @return Non-owning context pointer or `nullptr`.
     */
    const SnAPI::UI::UIContext* Context(ContextId Context) const;

    /**
     * @brief Reverse-map a context pointer to its stable id.
     * @param Context Borrowed context pointer.
     * @return Matching context id, or `0` when the pointer is null or not owned by this system.
     */
    ContextId ContextIdFor(const SnAPI::UI::UIContext* Context) const;
    /**
     * @brief Snapshot all live context ids in parent-before-child order.
     * @return Ordered vector of context ids.
     */
    std::vector<ContextId> ContextIds() const;

    /**
     * @brief Update the screen-space rectangle associated with one context.
     * @param Context Target context id.
     * @param X Screen-space left coordinate.
     * @param Y Screen-space top coordinate.
     * @param Width Screen-space width. Must be finite and non-negative.
     * @param Height Screen-space height. Must be finite and non-negative.
     * @return Success or error.
     */
    Result SetContextScreenRect(ContextId Context, float X, float Y, float Width, float Height);

    /**
     * @brief Bind one renderer viewport id to one UI context id.
     * @param Viewport Viewport id.
     * @param Context Context id.
     * @return Success or error.
     * @remarks Existing bindings involving either id are replaced so the mapping remains one-to-one.
     */
    Result BindViewportContext(ViewportId Viewport, ContextId Context);
    /**
     * @brief Remove the binding for one viewport id.
     * @param Viewport Viewport id.
     * @return Success or error.
     */
    Result UnbindViewportContext(ViewportId Viewport);
    /**
     * @brief Remove the binding for one context id.
     * @param Context Context id.
     * @return Success or error.
     */
    Result UnbindContext(ContextId Context);

    /**
     * @brief Resolve which context is bound to a viewport.
     * @param Viewport Viewport id.
     * @return Bound context id or `std::nullopt`.
     */
    std::optional<ContextId> BoundContextForViewport(ViewportId Viewport) const;
    /**
     * @brief Resolve which viewport is bound to a context.
     * @param Context Context id.
     * @return Bound viewport id or `std::nullopt`.
     */
    std::optional<ViewportId> BoundViewportForContext(ContextId Context) const;
    /**
     * @brief Snapshot all current viewport/context bindings.
     * @return Vector of bindings in unspecified map iteration order.
     */
    std::vector<ViewportBinding> ViewportBindings() const;

    TaskHandle EnqueueTask(WorkTask InTask, CompletionTask OnComplete = {});
    void EnqueueThreadTask(std::function<void()> InTask) override;
    void ExecuteQueuedTasks();

    /**
     * @brief Access the active bootstrap settings snapshot.
     * @return Borrowed reference to the subsystem-owned settings copy.
     */
    SnFunction(SnKey("Settings"))
    const UIBootstrapSettings& Settings() const;

    /**
     * @brief Register one UI element type across all live contexts.
     * @tparam TElement Element type to register.
     * @param ThemeTypeHash Theme-type hash used for styling. Defaults to the element type hash.
     * @return Success or error.
     * @remarks
     * Non-builtin registrations are remembered so contexts created later can inherit the registration.
     */
    template<typename TElement>
    Result RegisterElementType(uint32_t ThemeTypeHash = SnAPI::UI::TypeHash<TElement>())
    {
        GameLockGuard Lock(m_mutex);
        if (!m_initialized || m_rootContextId == 0)
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "UI system is not initialized"));
        }

        if constexpr (!SnAPI::UI::IsBuiltinElementTypeV<TElement>)
        {
            m_registeredExternalElementThemeHashes[SnAPI::UI::TypeHash<TElement>()] = ThemeTypeHash;
        }

        for (auto& [_, Node] : m_contextNodes)
        {
            if (Node.Context)
            {
                Node.Context->template RegisterElementType<TElement>(ThemeTypeHash);
            }
        }
        return Ok();
    }

    /**
     * @brief Unregister one UI element type across all live contexts.
     * @tparam TElement Element type to unregister.
     * @return Success or error.
     */
    template<typename TElement>
    Result UnregisterElementType()
    {
        GameLockGuard Lock(m_mutex);
        if (!m_initialized || m_rootContextId == 0)
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "UI system is not initialized"));
        }

        if constexpr (!SnAPI::UI::IsBuiltinElementTypeV<TElement>)
        {
            m_registeredExternalElementThemeHashes.erase(SnAPI::UI::TypeHash<TElement>());
        }

        for (auto& [_, Node] : m_contextNodes)
        {
            if (Node.Context)
            {
                Node.Context->template UnregisterElementType<TElement>();
            }
        }
        return Ok();
    }

    /**
     * @brief Query whether the root context currently knows an element type.
     * @tparam TElement Element type to query.
     * @return `true` when the root context reports the type as registered.
     */
    template<typename TElement>
    bool IsElementTypeRegistered() const
    {
        GameLockGuard Lock(m_mutex);
        if (!m_initialized || m_rootContextId == 0)
        {
            return false;
        }

        const auto RootIt = m_contextNodes.find(m_rootContextId);
        if (RootIt == m_contextNodes.end() || !RootIt->second.Context)
        {
            return false;
        }

        return RootIt->second.Context->template IsElementTypeRegistered<TElement>();
    }

private:
    struct ContextNode
    {
        ContextId Id = 0;
        ContextId Parent = 0;
        std::vector<ContextId> Children{};
        std::unique_ptr<SnAPI::UI::UIContext> Context{};
    };

    std::unique_ptr<SnAPI::UI::UIContext> CreateInitializedContext() const;
    ContextId CreateContextLocked(ContextId ParentContext, std::unique_ptr<SnAPI::UI::UIContext> Context);
    SnAPI::UI::UIContext* FindContextLocked(ContextId Context);
    const SnAPI::UI::UIContext* FindContextLocked(ContextId Context) const;
    bool IsContextPointEligibleLocked(ContextId Context, SnAPI::UI::UIPoint Position) const;
    bool IsContextKeyboardEligibleLocked(ContextId Context) const;
    void BuildContextOrderLocked(ContextId RootContext, std::vector<ContextId>& OutOrder) const;
    ContextId FindDeepestPointerTargetLocked(ContextId RootContext, SnAPI::UI::UIPoint Position) const;
    void DestroyContextRecursiveLocked(ContextId Context);
    void ShutdownUnlocked();

    mutable GameMutex m_mutex{};
    TSystemTaskQueue<UISystem> m_taskQueue{};
    UIBootstrapSettings m_settings{};

    std::unordered_map<ContextId, ContextNode> m_contextNodes{};
    std::unordered_map<const SnAPI::UI::UIContext*, ContextId> m_contextIdsByPointer{};

    std::unordered_map<ViewportId, ContextId> m_viewportToContext{};
    std::unordered_map<ContextId, ViewportId> m_contextToViewport{};

    std::unordered_map<uint32_t, uint32_t> m_registeredExternalElementThemeHashes{};

    ContextId m_rootContextId = 0;
    ContextId m_nextContextId = 0;

    ContextId m_activeInputContext = 0;
    ContextId m_pointerCaptureContext = 0;
    bool m_pointerLeftDown = false;
    bool m_pointerRightDown = false;
    bool m_pointerMiddleDown = false;
    SnAPI::UI::UIPoint m_lastPointerPosition{};
    bool m_hasLastPointerPosition = false;

    bool m_initialized = false;
};

SNAPI_DEFINE_TYPE_NAME(UISystem, "SnAPI::GameFramework::UISystem")
SNAPI_DEFINE_TYPE_NAME(UIBootstrapSettings, "SnAPI::GameFramework::UIBootstrapSettings")

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_UI
