#include "UISystem.h"
#include "GameThreading.h"

#if defined(SNAPI_GF_ENABLE_UI)

#include "Profiling.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace SnAPI::GameFramework
{

namespace
{

[[nodiscard]] bool IsPositiveFinite(const float Value)
{
    return std::isfinite(Value) && Value > 0.0f;
}

[[nodiscard]] bool IsNonNegativeFinite(const float Value)
{
    return std::isfinite(Value) && Value >= 0.0f;
}

} // namespace

UISystem::~UISystem()
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    Shutdown();
}

UISystem::UISystem(UISystem&& Other) noexcept
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    GameLockGuard Lock(Other.m_mutex);
    m_taskQueue = std::move(Other.m_taskQueue);
    m_settings = std::move(Other.m_settings);
    m_contextNodes = std::move(Other.m_contextNodes);
    m_contextIdsByPointer = std::move(Other.m_contextIdsByPointer);
    m_viewportToContext = std::move(Other.m_viewportToContext);
    m_contextToViewport = std::move(Other.m_contextToViewport);
    m_registeredExternalElementThemeHashes = std::move(Other.m_registeredExternalElementThemeHashes);
    m_rootContextId = Other.m_rootContextId;
    m_nextContextId = Other.m_nextContextId;
    m_activeInputContext = Other.m_activeInputContext;
    m_pointerCaptureContext = Other.m_pointerCaptureContext;
    m_pointerLeftDown = Other.m_pointerLeftDown;
    m_pointerRightDown = Other.m_pointerRightDown;
    m_pointerMiddleDown = Other.m_pointerMiddleDown;
    m_lastPointerPosition = Other.m_lastPointerPosition;
    m_hasLastPointerPosition = Other.m_hasLastPointerPosition;
    m_initialized = Other.m_initialized;

    Other.m_rootContextId = 0;
    Other.m_nextContextId = 0;
    Other.m_activeInputContext = 0;
    Other.m_pointerCaptureContext = 0;
    Other.m_pointerLeftDown = false;
    Other.m_pointerRightDown = false;
    Other.m_pointerMiddleDown = false;
    Other.m_lastPointerPosition = {};
    Other.m_hasLastPointerPosition = false;
    Other.m_initialized = false;
}

UISystem& UISystem::operator=(UISystem&& Other) noexcept
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    if (this == &Other)
    {
        return *this;
    }

    std::scoped_lock Lock(m_mutex, Other.m_mutex);
    ShutdownUnlocked();

    m_taskQueue = std::move(Other.m_taskQueue);
    m_settings = std::move(Other.m_settings);
    m_contextNodes = std::move(Other.m_contextNodes);
    m_contextIdsByPointer = std::move(Other.m_contextIdsByPointer);
    m_viewportToContext = std::move(Other.m_viewportToContext);
    m_contextToViewport = std::move(Other.m_contextToViewport);
    m_registeredExternalElementThemeHashes = std::move(Other.m_registeredExternalElementThemeHashes);
    m_rootContextId = Other.m_rootContextId;
    m_nextContextId = Other.m_nextContextId;
    m_activeInputContext = Other.m_activeInputContext;
    m_pointerCaptureContext = Other.m_pointerCaptureContext;
    m_pointerLeftDown = Other.m_pointerLeftDown;
    m_pointerRightDown = Other.m_pointerRightDown;
    m_pointerMiddleDown = Other.m_pointerMiddleDown;
    m_lastPointerPosition = Other.m_lastPointerPosition;
    m_hasLastPointerPosition = Other.m_hasLastPointerPosition;
    m_initialized = Other.m_initialized;

    Other.m_rootContextId = 0;
    Other.m_nextContextId = 0;
    Other.m_activeInputContext = 0;
    Other.m_pointerCaptureContext = 0;
    Other.m_pointerLeftDown = false;
    Other.m_pointerRightDown = false;
    Other.m_pointerMiddleDown = false;
    Other.m_lastPointerPosition = {};
    Other.m_hasLastPointerPosition = false;
    Other.m_initialized = false;

    return *this;
}

std::unique_ptr<SnAPI::UI::UIContext> UISystem::CreateInitializedContext() const
{
    auto Context = std::make_unique<SnAPI::UI::UIContext>();
    Context->EnsureDefaultSetup();
    Context->SetViewportSize(m_settings.ViewportWidth, m_settings.ViewportHeight);
    if (m_settings.DpiScaleOverride)
    {
        Context->SetDpiScale(*m_settings.DpiScaleOverride);
    }

    for (const auto& [ElementTypeHash, ThemeTypeHash] : m_registeredExternalElementThemeHashes)
    {
        Context->RegisterElementTypeHash(ElementTypeHash, ThemeTypeHash);
    }

    return Context;
}

UISystem::ContextId UISystem::CreateContextLocked(const ContextId ParentContext, std::unique_ptr<SnAPI::UI::UIContext> Context)
{
    if (!Context)
    {
        return 0;
    }

    const ContextId ContextIdValue = ++m_nextContextId;

    ContextNode Node{};
    Node.Id = ContextIdValue;
    Node.Parent = ParentContext;
    Node.Context = std::move(Context);

    auto* ContextPtr = Node.Context.get();
    auto [It, Inserted] = m_contextNodes.emplace(ContextIdValue, std::move(Node));
    if (!Inserted || !It->second.Context)
    {
        return 0;
    }

    m_contextIdsByPointer[ContextPtr] = ContextIdValue;

    if (ParentContext != 0)
    {
        if (auto ParentIt = m_contextNodes.find(ParentContext); ParentIt != m_contextNodes.end())
        {
            ParentIt->second.Children.push_back(ContextIdValue);
        }
    }

    return ContextIdValue;
}

SnAPI::UI::UIContext* UISystem::FindContextLocked(const ContextId Context)
{
    if (const auto It = m_contextNodes.find(Context); It != m_contextNodes.end())
    {
        return It->second.Context.get();
    }

    return nullptr;
}

const SnAPI::UI::UIContext* UISystem::FindContextLocked(const ContextId Context) const
{
    if (const auto It = m_contextNodes.find(Context); It != m_contextNodes.end())
    {
        return It->second.Context.get();
    }

    return nullptr;
}

bool UISystem::IsContextPointEligibleLocked(const ContextId Context, const SnAPI::UI::UIPoint Position) const
{
    const auto* ContextValue = FindContextLocked(Context);
    if (!ContextValue)
    {
        return false;
    }

    if (!ContextValue->ContainsScreenPoint(Position))
    {
        return false;
    }

    // Bound contexts are constrained to their screen rect (which tracks bound viewport rect),
    // so this hit check is the authoritative viewport gate.
    return true;
}

bool UISystem::IsContextKeyboardEligibleLocked(const ContextId Context) const
{
    if (Context == 0 || !FindContextLocked(Context))
    {
        return false;
    }

    // Unbound contexts (root/global) are always eligible.
    if (!m_contextToViewport.contains(Context))
    {
        return true;
    }

    // Bound child contexts only receive key/text while pointer is inside their viewport area.
    if (!m_hasLastPointerPosition)
    {
        return false;
    }

    return IsContextPointEligibleLocked(Context, m_lastPointerPosition);
}

void UISystem::BuildContextOrderLocked(const ContextId RootContext, std::vector<ContextId>& OutOrder) const
{
    if (RootContext == 0)
    {
        return;
    }

    const auto It = m_contextNodes.find(RootContext);
    if (It == m_contextNodes.end() || !It->second.Context)
    {
        return;
    }

    OutOrder.push_back(RootContext);
    for (const auto ChildId : It->second.Children)
    {
        BuildContextOrderLocked(ChildId, OutOrder);
    }
}

UISystem::ContextId UISystem::FindDeepestPointerTargetLocked(const ContextId RootContext, const SnAPI::UI::UIPoint Position) const
{
    if (RootContext == 0)
    {
        return 0;
    }

    const auto It = m_contextNodes.find(RootContext);
    if (It == m_contextNodes.end() || !It->second.Context)
    {
        return 0;
    }

    if (!IsContextPointEligibleLocked(RootContext, Position))
    {
        return 0;
    }

    for (auto ChildIt = It->second.Children.rbegin(); ChildIt != It->second.Children.rend(); ++ChildIt)
    {
        if (const ContextId ChildHit = FindDeepestPointerTargetLocked(*ChildIt, Position); ChildHit != 0)
        {
            return ChildHit;
        }
    }

    return RootContext;
}

Result UISystem::Initialize()
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    return Initialize(UIBootstrapSettings{});
}

Result UISystem::Initialize(const UIBootstrapSettings& SettingsValue)
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    TaskDispatcherScope DispatcherScope(*this);
    ExecuteQueuedTasks();
    GameLockGuard Lock(m_mutex);

    if (!IsPositiveFinite(SettingsValue.ViewportWidth) || !IsPositiveFinite(SettingsValue.ViewportHeight))
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                         "UI viewport dimensions must be finite and greater than zero"));
    }
    if (SettingsValue.DpiScaleOverride && !IsPositiveFinite(*SettingsValue.DpiScaleOverride))
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                         "UI DPI scale override must be finite and greater than zero"));
    }

    ShutdownUnlocked();
    m_settings = SettingsValue;

    auto RootContext = CreateInitializedContext();
    if (!RootContext)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to create root UI context"));
    }

    m_rootContextId = CreateContextLocked(0, std::move(RootContext));
    if (m_rootContextId == 0)
    {
        ShutdownUnlocked();
        return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to register root UI context"));
    }

    if (auto* Root = FindContextLocked(m_rootContextId))
    {
        Root->SetScreenRect(SnAPI::UI::UIRect{0.0f, 0.0f, m_settings.ViewportWidth, m_settings.ViewportHeight});
    }

    m_activeInputContext = m_rootContextId;
    m_initialized = true;
    return Ok();
}

void UISystem::Shutdown()
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    TaskDispatcherScope DispatcherScope(*this);
    ExecuteQueuedTasks();
    GameLockGuard Lock(m_mutex);
    ShutdownUnlocked();
}

bool UISystem::IsInitialized() const
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    GameLockGuard Lock(m_mutex);
    return m_initialized && m_rootContextId != 0 && m_contextNodes.contains(m_rootContextId);
}

void UISystem::Tick(const float DeltaSeconds)
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    TaskDispatcherScope DispatcherScope(*this);
    ExecuteQueuedTasks();
    std::vector<SnAPI::UI::UIContext*> ContextOrder{};
    {
        GameLockGuard Lock(m_mutex);
        if (!m_initialized || m_rootContextId == 0)
        {
            return;
        }

        std::vector<ContextId> ContextIds{};
        ContextIds.reserve(m_contextNodes.size());
        BuildContextOrderLocked(m_rootContextId, ContextIds);

        ContextOrder.reserve(ContextIds.size());
        for (const auto ContextIdValue : ContextIds)
        {
            if (auto* ContextValue = FindContextLocked(ContextIdValue))
            {
                ContextOrder.push_back(ContextValue);
            }
        }
    }

    for (auto* ContextValue : ContextOrder)
    {
        if (ContextValue)
        {
            ContextValue->Tick(DeltaSeconds);
        }
    }
}

Result UISystem::BuildRenderPackets(const ContextId Context, SnAPI::UI::RenderPacketList& OutPackets)
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    TaskDispatcherScope DispatcherScope(*this);
    ExecuteQueuedTasks();
    SnAPI::UI::UIContext* ContextValue = nullptr;
    {
        GameLockGuard Lock(m_mutex);
        if (!m_initialized || m_rootContextId == 0)
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "UI system is not initialized"));
        }

        ContextValue = FindContextLocked(Context);
        if (!ContextValue)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "UI context was not found"));
        }
    }

    ContextValue->BuildRenderPackets(OutPackets);
    return Ok();
}

Result UISystem::BuildBoundViewportRenderPackets(std::vector<ViewportPacketBatch>& OutBatches)
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    TaskDispatcherScope DispatcherScope(*this);
    ExecuteQueuedTasks();

    struct BuildEntry
    {
        ViewportId Viewport = 0;
        ContextId Context = 0;
        SnAPI::UI::UIContext* ContextPtr = nullptr;
    };

    std::vector<BuildEntry> BuildEntries{};
    {
        GameLockGuard Lock(m_mutex);
        if (!m_initialized || m_rootContextId == 0)
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "UI system is not initialized"));
        }

        BuildEntries.reserve(m_viewportToContext.size());
        for (const auto& [Viewport, ContextIdValue] : m_viewportToContext)
        {
            auto* ContextValue = FindContextLocked(ContextIdValue);
            if (!ContextValue)
            {
                continue;
            }

            BuildEntries.push_back(BuildEntry{Viewport, ContextIdValue, ContextValue});
        }
    }

    OutBatches.clear();
    OutBatches.reserve(BuildEntries.size());

    for (const auto& Entry : BuildEntries)
    {
        ViewportPacketBatch Batch{};
        Batch.Viewport = Entry.Viewport;
        Batch.Context = Entry.Context;
        Batch.ContextPtr = Entry.ContextPtr;
        Entry.ContextPtr->BuildRenderPackets(Batch.Packets);
        OutBatches.emplace_back(std::move(Batch));
    }

    return Ok();
}

void UISystem::PushInput(const SnAPI::UI::PointerEvent& EventValue)
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    GameLockGuard Lock(m_mutex);
    if (!m_initialized || m_rootContextId == 0)
    {
        return;
    }

    m_lastPointerPosition = EventValue.Position;
    m_hasLastPointerPosition = true;

    std::vector<ContextId> ContextOrder{};
    ContextOrder.reserve(m_contextNodes.size());
    BuildContextOrderLocked(m_rootContextId, ContextOrder);

    for (const auto ContextIdValue : ContextOrder)
    {
        if (auto* ContextValue = FindContextLocked(ContextIdValue))
        {
            ContextValue->PushInput(EventValue);
        }
    }

    m_pointerLeftDown = EventValue.LeftDown;
    m_pointerRightDown = EventValue.RightDown;
    m_pointerMiddleDown = EventValue.MiddleDown;
}

void UISystem::PushInput(const SnAPI::UI::KeyEvent& EventValue)
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    GameLockGuard Lock(m_mutex);
    if (!m_initialized || m_rootContextId == 0)
    {
        return;
    }

    std::vector<ContextId> ContextOrder{};
    ContextOrder.reserve(m_contextNodes.size());
    BuildContextOrderLocked(m_rootContextId, ContextOrder);

    for (const auto ContextIdValue : ContextOrder)
    {
        if (auto* ContextValue = FindContextLocked(ContextIdValue))
        {
            ContextValue->PushInput(EventValue);
        }
    }
}

void UISystem::PushInput(const SnAPI::UI::TextInputEvent& EventValue)
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    GameLockGuard Lock(m_mutex);
    if (!m_initialized || m_rootContextId == 0)
    {
        return;
    }

    std::vector<ContextId> ContextOrder{};
    ContextOrder.reserve(m_contextNodes.size());
    BuildContextOrderLocked(m_rootContextId, ContextOrder);

    for (const auto ContextIdValue : ContextOrder)
    {
        if (auto* ContextValue = FindContextLocked(ContextIdValue))
        {
            ContextValue->PushInput(EventValue);
        }
    }
}

void UISystem::PushInput(const SnAPI::UI::WheelEvent& EventValue)
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    GameLockGuard Lock(m_mutex);
    if (!m_initialized || m_rootContextId == 0)
    {
        return;
    }

    m_lastPointerPosition = EventValue.Position;
    m_hasLastPointerPosition = true;

    std::vector<ContextId> ContextOrder{};
    ContextOrder.reserve(m_contextNodes.size());
    BuildContextOrderLocked(m_rootContextId, ContextOrder);

    for (const auto ContextIdValue : ContextOrder)
    {
        if (auto* ContextValue = FindContextLocked(ContextIdValue))
        {
            ContextValue->PushInput(EventValue);
        }
    }
}

Result UISystem::SetViewportSize(const float Width, const float Height)
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    if (!IsPositiveFinite(Width) || !IsPositiveFinite(Height))
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                         "UI viewport dimensions must be finite and greater than zero"));
    }

    GameLockGuard Lock(m_mutex);
    if (!m_initialized || m_rootContextId == 0)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "UI system is not initialized"));
    }

    auto* RootContext = FindContextLocked(m_rootContextId);
    if (!RootContext)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Root UI context was not found"));
    }

    m_settings.ViewportWidth = Width;
    m_settings.ViewportHeight = Height;

    const auto RootRect = RootContext->GetScreenRect();
    RootContext->SetViewportSize(Width, Height);
    RootContext->SetScreenRect(SnAPI::UI::UIRect{RootRect.X, RootRect.Y, Width, Height});

    return Ok();
}

Result UISystem::SetDpiScale(const float Scale)
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    if (!IsPositiveFinite(Scale))
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                         "UI DPI scale must be finite and greater than zero"));
    }

    GameLockGuard Lock(m_mutex);
    if (!m_initialized || m_rootContextId == 0)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "UI system is not initialized"));
    }

    m_settings.DpiScaleOverride = Scale;
    for (auto& [_, Node] : m_contextNodes)
    {
        if (Node.Context)
        {
            Node.Context->SetDpiScale(Scale);
        }
    }

    return Ok();
}

UISystem::ContextId UISystem::RootContextId() const
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    GameLockGuard Lock(m_mutex);
    return m_rootContextId;
}

Result UISystem::CreateContext(const ContextId ParentContext, ContextId& OutContextId)
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    OutContextId = 0;

    if (ParentContext == 0)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Parent context id must be greater than zero"));
    }

    GameLockGuard Lock(m_mutex);
    if (!m_initialized || m_rootContextId == 0)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "UI system is not initialized"));
    }

    auto* Parent = FindContextLocked(ParentContext);
    if (!Parent)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Parent UI context was not found"));
    }

    auto Context = CreateInitializedContext();
    if (!Context)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to create UI context"));
    }

    Context->SetActiveTheme(Parent->GetActiveTheme());
    Context->SetScreenRect(Parent->GetScreenRect());

    OutContextId = CreateContextLocked(ParentContext, std::move(Context));
    if (OutContextId == 0)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to register UI context"));
    }

    return Ok();
}

void UISystem::DestroyContextRecursiveLocked(const ContextId Context)
{
    const auto It = m_contextNodes.find(Context);
    if (It == m_contextNodes.end())
    {
        return;
    }

    const std::vector<ContextId> Children = It->second.Children;
    for (const auto ChildId : Children)
    {
        DestroyContextRecursiveLocked(ChildId);
    }

    const ContextId ParentId = It->second.Parent;
    const auto* ContextPtr = It->second.Context.get();

    if (const auto ContextViewportIt = m_contextToViewport.find(Context); ContextViewportIt != m_contextToViewport.end())
    {
        m_viewportToContext.erase(ContextViewportIt->second);
        m_contextToViewport.erase(ContextViewportIt);
    }

    if (ParentId != 0)
    {
        if (auto ParentIt = m_contextNodes.find(ParentId); ParentIt != m_contextNodes.end())
        {
            auto& ChildrenIds = ParentIt->second.Children;
            ChildrenIds.erase(std::remove(ChildrenIds.begin(), ChildrenIds.end(), Context), ChildrenIds.end());
        }
    }

    if (ContextPtr)
    {
        m_contextIdsByPointer.erase(ContextPtr);
    }

    if (m_activeInputContext == Context)
    {
        m_activeInputContext = m_rootContextId;
    }
    if (m_pointerCaptureContext == Context)
    {
        m_pointerCaptureContext = 0;
    }

    m_contextNodes.erase(Context);
}

Result UISystem::DestroyContext(const ContextId Context)
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    if (Context == 0)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Context id must be greater than zero"));
    }

    GameLockGuard Lock(m_mutex);
    if (!m_initialized || m_rootContextId == 0)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "UI system is not initialized"));
    }

    if (Context == m_rootContextId)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Root context cannot be destroyed"));
    }

    if (!m_contextNodes.contains(Context))
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "UI context was not found"));
    }

    DestroyContextRecursiveLocked(Context);
    return Ok();
}

SnAPI::UI::UIContext* UISystem::Context(const ContextId Context)
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    GameLockGuard Lock(m_mutex);
    return FindContextLocked(Context);
}

const SnAPI::UI::UIContext* UISystem::Context(const ContextId Context) const
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    GameLockGuard Lock(m_mutex);
    return FindContextLocked(Context);
}

UISystem::ContextId UISystem::ContextIdFor(const SnAPI::UI::UIContext* Context) const
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    GameLockGuard Lock(m_mutex);
    if (!Context)
    {
        return 0;
    }

    if (const auto It = m_contextIdsByPointer.find(Context); It != m_contextIdsByPointer.end())
    {
        return It->second;
    }

    return 0;
}

std::vector<UISystem::ContextId> UISystem::ContextIds() const
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    GameLockGuard Lock(m_mutex);

    std::vector<ContextId> OutContextIds{};
    OutContextIds.reserve(m_contextNodes.size());
    BuildContextOrderLocked(m_rootContextId, OutContextIds);
    return OutContextIds;
}

Result UISystem::SetContextScreenRect(const ContextId Context, const float X, const float Y, const float Width, const float Height)
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    if (!IsNonNegativeFinite(Width) || !IsNonNegativeFinite(Height) || !std::isfinite(X) || !std::isfinite(Y))
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                         "Context rect values must be finite and size must be non-negative"));
    }

    GameLockGuard Lock(m_mutex);
    if (!m_initialized || m_rootContextId == 0)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "UI system is not initialized"));
    }

    auto* ContextValue = FindContextLocked(Context);
    if (!ContextValue)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "UI context was not found"));
    }

    ContextValue->SetScreenRect(SnAPI::UI::UIRect{X, Y, Width, Height});
    return Ok();
}

Result UISystem::BindViewportContext(const ViewportId Viewport, const ContextId Context)
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    if (Viewport == 0 || Context == 0)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                         "Viewport and context ids must be greater than zero"));
    }

    GameLockGuard Lock(m_mutex);
    if (!m_initialized || m_rootContextId == 0)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "UI system is not initialized"));
    }

    if (!m_contextNodes.contains(Context))
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "UI context was not found"));
    }

    if (const auto ExistingViewportIt = m_viewportToContext.find(Viewport); ExistingViewportIt != m_viewportToContext.end())
    {
        m_contextToViewport.erase(ExistingViewportIt->second);
        m_viewportToContext.erase(ExistingViewportIt);
    }

    if (const auto ExistingContextIt = m_contextToViewport.find(Context); ExistingContextIt != m_contextToViewport.end())
    {
        m_viewportToContext.erase(ExistingContextIt->second);
        m_contextToViewport.erase(ExistingContextIt);
    }

    m_viewportToContext[Viewport] = Context;
    m_contextToViewport[Context] = Viewport;
    return Ok();
}

Result UISystem::UnbindViewportContext(const ViewportId Viewport)
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    if (Viewport == 0)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Viewport id must be greater than zero"));
    }

    GameLockGuard Lock(m_mutex);
    if (!m_initialized || m_rootContextId == 0)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "UI system is not initialized"));
    }

    if (const auto It = m_viewportToContext.find(Viewport); It != m_viewportToContext.end())
    {
        m_contextToViewport.erase(It->second);
        m_viewportToContext.erase(It);
    }

    return Ok();
}

Result UISystem::UnbindContext(const ContextId Context)
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    if (Context == 0)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Context id must be greater than zero"));
    }

    GameLockGuard Lock(m_mutex);
    if (!m_initialized || m_rootContextId == 0)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "UI system is not initialized"));
    }

    if (const auto It = m_contextToViewport.find(Context); It != m_contextToViewport.end())
    {
        m_viewportToContext.erase(It->second);
        m_contextToViewport.erase(It);
    }

    return Ok();
}

std::optional<UISystem::ContextId> UISystem::BoundContextForViewport(const ViewportId Viewport) const
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    GameLockGuard Lock(m_mutex);
    if (const auto It = m_viewportToContext.find(Viewport); It != m_viewportToContext.end())
    {
        return It->second;
    }

    return std::nullopt;
}

std::optional<UISystem::ViewportId> UISystem::BoundViewportForContext(const ContextId Context) const
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    GameLockGuard Lock(m_mutex);
    if (const auto It = m_contextToViewport.find(Context); It != m_contextToViewport.end())
    {
        return It->second;
    }

    return std::nullopt;
}

std::vector<UISystem::ViewportBinding> UISystem::ViewportBindings() const
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    GameLockGuard Lock(m_mutex);

    std::vector<ViewportBinding> OutBindings{};
    OutBindings.reserve(m_viewportToContext.size());

    for (const auto& [Viewport, Context] : m_viewportToContext)
    {
        OutBindings.push_back(ViewportBinding{Viewport, Context});
    }

    return OutBindings;
}

TaskHandle UISystem::EnqueueTask(WorkTask InTask, CompletionTask OnComplete)
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    return m_taskQueue.EnqueueTask(std::move(InTask), std::move(OnComplete));
}

void UISystem::EnqueueThreadTask(std::function<void()> InTask)
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    m_taskQueue.EnqueueThreadTask(std::move(InTask));
}

void UISystem::ExecuteQueuedTasks()
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    m_taskQueue.ExecuteQueuedTasks(*this, m_mutex);
}

const UIBootstrapSettings& UISystem::Settings() const
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    GameLockGuard Lock(m_mutex);
    return m_settings;
}

void UISystem::ShutdownUnlocked()
{
    SNAPI_GF_PROFILE_FUNCTION("UI");
    m_contextNodes.clear();
    m_contextIdsByPointer.clear();
    m_viewportToContext.clear();
    m_contextToViewport.clear();

    m_rootContextId = 0;
    m_nextContextId = 0;
    m_activeInputContext = 0;
    m_pointerCaptureContext = 0;
    m_pointerLeftDown = false;
    m_pointerRightDown = false;
    m_pointerMiddleDown = false;
    m_lastPointerPosition = {};
    m_hasLastPointerPosition = false;

    m_initialized = false;
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_UI
