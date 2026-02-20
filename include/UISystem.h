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

#include <UIContext.h>

namespace SnAPI::GameFramework
{

/**
 * @brief Bootstrap settings for world-owned SnAPI.UI integration.
 */
struct UIBootstrapSettings
{
    float ViewportWidth = 1600.0f; /**< @brief Initial logical viewport width in UI units; must be finite and > 0. */
    float ViewportHeight = 900.0f; /**< @brief Initial logical viewport height in UI units; must be finite and > 0. */
    std::optional<float> DpiScaleOverride{}; /**< @brief Optional explicit DPI scale override; when nullopt, UIContext defaults/environment values are preserved. */
};

/**
 * @brief World-owned UI system with parent/child `UIContext` graph support.
 *
 * @remarks
 * - Contexts are addressed by stable `ContextId` values.
 * - Context lifecycle is explicit (`CreateContext`, `DestroyContext`).
 * - Viewport bindings are explicit and one-to-one (`ViewportId <-> ContextId`).
 */
class UISystem final : public ITaskDispatcher
{
public:
    using WorkTask = std::function<void(UISystem&)>;
    using CompletionTask = std::function<void(const TaskHandle&)>;
    using ContextId = std::uint64_t;
    using ViewportId = std::uint64_t;

    struct ViewportPacketBatch
    {
        ViewportId Viewport = 0;
        ContextId Context = 0;
        SnAPI::UI::UIContext* ContextPtr = nullptr;
        SnAPI::UI::RenderPacketList Packets{};
    };

    struct ViewportBinding
    {
        ViewportId Viewport = 0;
        ContextId Context = 0;
    };

    UISystem() = default;
    ~UISystem() override;

    UISystem(const UISystem&) = delete;
    UISystem& operator=(const UISystem&) = delete;

    UISystem(UISystem&& Other) noexcept;
    UISystem& operator=(UISystem&& Other) noexcept;

    Result Initialize();
    Result Initialize(const UIBootstrapSettings& SettingsValue);
    void Shutdown();
    bool IsInitialized() const;

    void Tick(float DeltaSeconds);

    Result BuildRenderPackets(ContextId Context, SnAPI::UI::RenderPacketList& OutPackets);
    Result BuildBoundViewportRenderPackets(std::vector<ViewportPacketBatch>& OutBatches);

    void PushInput(const SnAPI::UI::PointerEvent& EventValue);
    void PushInput(const SnAPI::UI::KeyEvent& EventValue);
    void PushInput(const SnAPI::UI::TextInputEvent& EventValue);
    void PushInput(const SnAPI::UI::WheelEvent& EventValue);

    Result SetViewportSize(float Width, float Height);
    Result SetDpiScale(float Scale);

    ContextId RootContextId() const;
    Result CreateContext(ContextId ParentContext, ContextId& OutContextId);
    Result DestroyContext(ContextId Context);

    SnAPI::UI::UIContext* Context(ContextId Context);
    const SnAPI::UI::UIContext* Context(ContextId Context) const;

    ContextId ContextIdFor(const SnAPI::UI::UIContext* Context) const;
    std::vector<ContextId> ContextIds() const;

    Result SetContextScreenRect(ContextId Context, float X, float Y, float Width, float Height);

    Result BindViewportContext(ViewportId Viewport, ContextId Context);
    Result UnbindViewportContext(ViewportId Viewport);
    Result UnbindContext(ContextId Context);

    std::optional<ContextId> BoundContextForViewport(ViewportId Viewport) const;
    std::optional<ViewportId> BoundViewportForContext(ContextId Context) const;
    std::vector<ViewportBinding> ViewportBindings() const;

    TaskHandle EnqueueTask(WorkTask InTask, CompletionTask OnComplete = {});
    void EnqueueThreadTask(std::function<void()> InTask) override;
    void ExecuteQueuedTasks();

    const UIBootstrapSettings& Settings() const;

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

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_UI
