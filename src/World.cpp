#include "World.h"
#include "AudioListenerComponent.h"
#include "AudioSourceComponent.h"
#include "CameraComponent.h"
#include "CharacterMovementController.h"
#include "ComponentTypeRegistry.h"
#include "FollowTargetComponent.h"
#include "InputComponent.h"
#include "NodeStorageFactoryRegistry.h"
#include "Profiling.h"
#include "Relevance.h"
#include "RigidBodyComponent.h"
#include "Serialization.h"
#include "SkeletalMeshComponent.h"
#include "StaticMeshComponent.h"
#include "TypeRegistry.h"
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <iostream>
#if defined(SNAPI_GF_ENABLE_RENDERER)
#include <LinearAlgebra.hpp>
#include <ICamera.hpp>
#include <FontFace.hpp>
#endif
#if defined(SNAPI_GF_ENABLE_UI)
#include <UIPacketWriter.h>
#include <unordered_map>
#endif

namespace SnAPI::GameFramework
{

namespace
{
#if defined(SNAPI_GF_ENABLE_RENDERER) && defined(SNAPI_GF_ENABLE_UI)
class UiFontMetricsAdapter final : public SnAPI::UI::IFontMetrics
{
public:
    void Bind(SnAPI::Graphics::FontFace* Face)
    {
        if (m_face == Face)
        {
            return;
        }
        m_face = Face;
        m_cachedGlyphs.clear();
        m_cachedRevision = 0;
    }

    const SnAPI::UI::GlyphMetrics* GetGlyph(uint32_t Codepoint) const override
    {
        if (!m_face || !m_face->Valid())
        {
            return nullptr;
        }

        const uint64_t CacheRevision = m_face->GlyphCacheRevision();
        if (CacheRevision != m_cachedRevision)
        {
            m_cachedGlyphs.clear();
            m_cachedRevision = CacheRevision;
        }

        if (const auto Cached = m_cachedGlyphs.find(Codepoint); Cached != m_cachedGlyphs.end())
        {
            return &Cached->second;
        }

        SnAPI::Graphics::FontFace::ResolvedGlyph ResolvedGlyph{};
        if (!m_face->ResolveGlyph(Codepoint, ResolvedGlyph))
        {
            return nullptr;
        }

        const auto& Glyph = ResolvedGlyph.GlyphData;
        const auto GlyphUv = ResolvedGlyph.UV;
        float PaddingX = 0.0f;
        float PaddingY = 0.0f;
        if (ResolvedGlyph.pOwningFace)
        {
            if (const auto* AtlasPage = ResolvedGlyph.pOwningFace->AtlasForCodePoint(ResolvedGlyph.ResolvedCodePoint))
            {
                const auto AtlasSize = AtlasPage->Size();
                if (AtlasSize.x() > 0 && AtlasSize.y() > 0)
                {
                    const float UvWidthPixels = static_cast<float>(GlyphUv.Width()) * static_cast<float>(AtlasSize.x());
                    const float UvHeightPixels = static_cast<float>(GlyphUv.Height()) * static_cast<float>(AtlasSize.y());
                    PaddingX = std::max(0.0f, (UvWidthPixels - static_cast<float>(Glyph.Width)) * 0.5f);
                    PaddingY = std::max(0.0f, (UvHeightPixels - static_cast<float>(Glyph.Height)) * 0.5f);
                }
            }
        }

        SnAPI::UI::GlyphMetrics Metrics{};
        Metrics.U0 = static_cast<float>(GlyphUv.Min.x());
        Metrics.V0 = static_cast<float>(GlyphUv.Min.y());
        Metrics.U1 = static_cast<float>(GlyphUv.Max.x());
        Metrics.V1 = static_cast<float>(GlyphUv.Max.y());
        Metrics.Width = static_cast<float>(Glyph.Width) + PaddingX * 2.0f;
        Metrics.Height = static_cast<float>(Glyph.Height) + PaddingY * 2.0f;
        Metrics.BearingX = static_cast<float>(Glyph.BitmapLeft) - PaddingX;
        // UIPacketWriter expects stb-style y-offset from baseline (usually negative).
        // FreeType BitmapTop is upward-positive, so convert sign for consistent layout.
        Metrics.BearingY = -(static_cast<float>(Glyph.BitmapTop) + PaddingY);
        Metrics.Advance = static_cast<float>(Glyph.Advance.x());
        Metrics.AtlasTextureHandle = static_cast<std::uint64_t>(
            reinterpret_cast<std::uintptr_t>(ResolvedGlyph.pAtlasImage));

        auto [It, Inserted] = m_cachedGlyphs.emplace(Codepoint, Metrics);
        (void)Inserted;
        return &It->second;
    }

    float GetLineHeight() const override
    {
        return (m_face && m_face->Valid()) ? m_face->Height() : 0.0f;
    }

    float GetAscent() const override
    {
        return (m_face && m_face->Valid()) ? m_face->Ascender() : 0.0f;
    }

private:
    SnAPI::Graphics::FontFace* m_face = nullptr;
    mutable std::unordered_map<uint32_t, SnAPI::UI::GlyphMetrics> m_cachedGlyphs{};
    mutable uint64_t m_cachedRevision = 0;
};
#endif

[[nodiscard]] const BaseNode* ResolveComponentOwnerNode(const IWorld& WorldRef,
                                                        NodeHandle& InOutOwner,
                                                        NodeHandle& OutResolvedHandle)
{
    if (InOutOwner.IsNull())
    {
        return nullptr;
    }

    OutResolvedHandle = InOutOwner;
    if (const BaseNode* Node = WorldRef.BorrowedNode(OutResolvedHandle))
    {
        InOutOwner = OutResolvedHandle;
        return Node;
    }

    if (auto HandleResult = WorldRef.NodeHandleById(InOutOwner.Id); HandleResult)
    {
        OutResolvedHandle = *HandleResult;
        if (const BaseNode* Node = WorldRef.BorrowedNode(OutResolvedHandle))
        {
            InOutOwner = OutResolvedHandle;
            return Node;
        }
    }

    return nullptr;
}

[[nodiscard]] BaseNode* ResolveComponentOwnerNode(IWorld& WorldRef, NodeHandle& InOutOwner, NodeHandle& OutResolvedHandle)
{
    return const_cast<BaseNode*>(
        ResolveComponentOwnerNode(static_cast<const IWorld&>(WorldRef), InOutOwner, OutResolvedHandle));
}

void UnregisterRuntimeTypeOnNode(BaseNode& Node, const TypeId& Type)
{
    const uint32_t TypeIndex = ComponentTypeRegistry::TypeIndex(Type);
    const std::size_t Word = TypeIndex / 64u;
    const std::size_t Bit = TypeIndex % 64u;
    if (Word < Node.ComponentMask().size())
    {
        Node.ComponentMask()[Word] &= ~(1ull << Bit);
    }

    auto& Types = Node.ComponentTypes();
    for (std::size_t Index = 0; Index < Types.size(); ++Index)
    {
        if (Types[Index] != Type)
        {
            continue;
        }

        auto TypeIt = Types.begin() + static_cast<std::vector<TypeId>::difference_type>(Index);
        Types.erase(TypeIt);
        break;
    }

    static const TypeId RelevanceType = StaticTypeId<RelevanceComponent>();
    if (Type == RelevanceType)
    {
        Node.RelevanceState(nullptr);
    }
}

void* ResolveRuntimeRawFromStorage(WorldEcsRuntime& Runtime, const NodeHandle& OwnerHandle, const TypeId& Type)
{
    auto RuntimeComponentHandle = Runtime.ComponentHandle(OwnerHandle, Type);
    if (!RuntimeComponentHandle)
    {
        return nullptr;
    }
    return Runtime.ResolveComponentRaw(*RuntimeComponentHandle, Type);
}

const void* ResolveRuntimeRawFromStorage(const WorldEcsRuntime& Runtime,
                                         const NodeHandle& OwnerHandle,
                                         const TypeId& Type)
{
    auto RuntimeComponentHandle = Runtime.ComponentHandle(OwnerHandle, Type);
    if (!RuntimeComponentHandle)
    {
        return nullptr;
    }
    return Runtime.ResolveComponentRaw(*RuntimeComponentHandle, Type);
}

BaseNode* ResolveNodeIncludingPendingDestroy(WorldEcsRuntime& Runtime, const NodeHandle& Handle)
{
    if (Handle.IsNull())
    {
        return nullptr;
    }

    if (Handle.HasRuntimeKey())
    {
        if (BaseNode* Node = Runtime.ResolveNodeIncludingPendingDestroy(Handle))
        {
            return Node;
        }
    }

    NodeHandle ResolvedHandle = Handle;
    if (BaseNode* Node = ResolvedHandle.Borrowed())
    {
        return Node;
    }

    if (auto HandleResult = Runtime.NodeHandleById(Handle.Id); HandleResult)
    {
        return Runtime.ResolveNodeIncludingPendingDestroy(*HandleResult);
    }

    return nullptr;
}
} // namespace

WorldExecutionProfile WorldExecutionProfile::Runtime()
{
    return {};
}

WorldExecutionProfile WorldExecutionProfile::Editor()
{
    auto Profile = Runtime();
    Profile.RunGameplay = false;
    Profile.TickPhysicsSimulation = false;
    Profile.TickAudio = false;
    Profile.PumpNetworking = false;
    return Profile;
}

WorldExecutionProfile WorldExecutionProfile::PIE()
{
    return Runtime();
}

World::World()
    : m_name("World")
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    , m_networkSystem(*this)
#endif
{
    m_worldKind = EWorldKind::Runtime;
    m_executionProfile = WorldExecutionProfile::Runtime();
    RegisterBuiltinScriptBackends(m_scriptRuntime);
}

World::World(std::string Name)
    : m_name(std::move(Name))
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    , m_networkSystem(*this)
#endif
{
    m_worldKind = EWorldKind::Runtime;
    m_executionProfile = WorldExecutionProfile::Runtime();
    RegisterBuiltinScriptBackends(m_scriptRuntime);
}

World::~World()
{
    Clear();
    m_scriptRuntime.Shutdown();
}

const std::string& World::Name() const
{
    return m_name;
}

void World::Name(std::string NameValue)
{
    m_name = std::move(NameValue);
}

TaskHandle World::EnqueueTask(WorkTask InTask, CompletionTask OnComplete)
{
    return m_taskQueue.EnqueueTask(std::move(InTask), std::move(OnComplete));
}

void World::EnqueueThreadTask(std::function<void()> InTask)
{
    
    m_taskQueue.EnqueueThreadTask(std::move(InTask));
}

void World::ExecuteQueuedTasks()
{
    m_taskQueue.ExecuteQueuedTasks(*this, m_threadMutex);
}

EWorldKind World::Kind() const
{
    return m_worldKind;
}

bool World::ShouldRunGameplay() const
{
    return m_executionProfile.RunGameplay;
}

bool World::ShouldTickInput() const
{
    return m_executionProfile.TickInput;
}

bool World::ShouldTickUI() const
{
    return m_executionProfile.TickUI;
}

bool World::ShouldPumpNetworking() const
{
    return m_executionProfile.PumpNetworking;
}

bool World::ShouldSimulatePhysics() const
{
    return m_executionProfile.TickPhysicsSimulation;
}

bool World::ShouldAllowPhysicsQueries() const
{
    return m_executionProfile.AllowPhysicsQueries;
}

bool World::ShouldTickAudio() const
{
    return m_executionProfile.TickAudio;
}

bool World::ShouldRunNodeEndFrame() const
{
    return m_executionProfile.RunNodeEndFrame;
}

bool World::ShouldBuildUiRenderPackets() const
{
    return m_executionProfile.BuildUiRenderPackets;
}

bool World::ShouldRenderFrame() const
{
    return m_executionProfile.RenderFrame;
}

void World::ForEachNode(const NodeVisitor Visitor, void* const UserData)
{
    if (!Visitor)
    {
        return;
    }

    m_ecsRuntime.ForEachNode(Visitor, UserData);
}

void World::ForEachNode(const NodeVisitor Visitor, void* const UserData) const
{
    const_cast<World*>(this)->ForEachNode(Visitor, UserData);
}

TExpected<NodeHandle> World::NodeHandleById(const Uuid& Id) const
{
    return m_ecsRuntime.NodeHandleById(Id);
}

TExpected<NodeHandle> World::CreateNode(const TypeId& Type, std::string Name)
{
    auto* Info = TypeRegistry::Instance().Find(Type);
    if (!Info)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Type not registered"));
    }
    if (!TypeRegistry::Instance().IsA(Type, StaticTypeId<BaseNode>()))
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Type is not a node type"));
    }
    (void)Info;

    if (const Result EnsureStorageResult = NodeStorageFactoryRegistry::Instance().EnsureStorage(Type, m_ecsRuntime);
        !EnsureStorageResult)
    {
        return std::unexpected(EnsureStorageResult.error());
    }

    ScopedComponentOnCreateSuppression SuppressOnCreate{};
    auto HandleResult = m_ecsRuntime.CreateNode(*this, Type, std::move(Name));
    if (!HandleResult)
    {
        return std::unexpected(HandleResult.error());
    }

    m_rootNodes.push_back(*HandleResult);
    return *HandleResult;
}

TExpected<NodeHandle> World::CreateNodeWithId(const TypeId& Type, std::string Name, const Uuid& Id)
{
    auto* Info = TypeRegistry::Instance().Find(Type);
    if (!Info)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Type not registered"));
    }
    if (!TypeRegistry::Instance().IsA(Type, StaticTypeId<BaseNode>()))
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Type is not a node type"));
    }
    (void)Info;

    if (const Result EnsureStorageResult = NodeStorageFactoryRegistry::Instance().EnsureStorage(Type, m_ecsRuntime);
        !EnsureStorageResult)
    {
        return std::unexpected(EnsureStorageResult.error());
    }

    ScopedComponentOnCreateSuppression SuppressOnCreate{};
    auto HandleResult = m_ecsRuntime.CreateNode(*this, Type, std::move(Name), &Id);
    if (!HandleResult)
    {
        return std::unexpected(HandleResult.error());
    }

    m_rootNodes.push_back(*HandleResult);
    return *HandleResult;
}

BaseNode* World::BorrowedNode(NodeHandle& InOutHandle)
{
    return InOutHandle.Borrowed();
}

const BaseNode* World::BorrowedNode(NodeHandle& InOutHandle) const
{
    return InOutHandle.Borrowed();
}

Result World::DestroyNode(NodeHandle& InOutHandle)
{
    if (InOutHandle.IsNull())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Node handle is null"));
    }

    BaseNode* RootNode = ResolveNodeIncludingPendingDestroy(m_ecsRuntime, InOutHandle);
    if (!RootNode)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Node not found"));
    }
    if (RootNode->PendingDestroy())
    {
        return Ok();
    }

    std::vector<NodeHandle> Stack{};
    Stack.push_back(InOutHandle);
    while (!Stack.empty())
    {
        const NodeHandle CurrentHandle = Stack.back();
        Stack.pop_back();

        BaseNode* CurrentNode = ResolveNodeIncludingPendingDestroy(m_ecsRuntime, CurrentHandle);
        if (!CurrentNode || CurrentNode->PendingDestroy())
        {
            continue;
        }

        for (const NodeHandle Child : CurrentNode->Children())
        {
            Stack.push_back(Child);
        }

        if (!m_ecsRuntime.DestroyNodeLater(CurrentHandle))
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Node could not be scheduled for destroy"));
        }

        CurrentNode->PendingDestroy(true);
        m_pendingDestroy.push_back(CurrentHandle);
    }

    return Ok();
}

Result World::AttachChild(NodeHandle& InOutParent, NodeHandle& InOutChild)
{
    BaseNode* ParentNode = BorrowedNode(InOutParent);
    BaseNode* ChildNode = BorrowedNode(InOutChild);
    if (!ParentNode || !ChildNode)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Parent or child not found"));
    }
    if (!ChildNode->Parent().IsNull())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Child already has a parent"));
    }

    ParentNode->AddChild(InOutChild);
    ChildNode->Parent(InOutParent);
    ChildNode->World(this);

    m_rootNodes.erase(std::remove(m_rootNodes.begin(), m_rootNodes.end(), InOutChild), m_rootNodes.end());

    return Ok();
}

Result World::DetachChild(NodeHandle& InOutChild)
{
    BaseNode* ChildNode = BorrowedNode(InOutChild);
    if (!ChildNode)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Child not found"));
    }

    if (!ChildNode->Parent().IsNull())
    {
        NodeHandle ParentHandle = ChildNode->Parent();
        if (BaseNode* ParentNode = BorrowedNode(ParentHandle))
        {
            ParentNode->RemoveChild(InOutChild);
        }
        ChildNode->Parent({});
    }

    if (std::find(m_rootNodes.begin(), m_rootNodes.end(), InOutChild) == m_rootNodes.end())
    {
        m_rootNodes.push_back(InOutChild);
    }
    return Ok();
}

void* World::BorrowedComponent(NodeHandle& InOutOwner, const TypeId& Type)
{
    NodeHandle ResolvedOwner{};
    BaseNode* Node = ResolveComponentOwnerNode(*this, InOutOwner, ResolvedOwner);
    if (!Node)
    {
        return nullptr;
    }

    if (!ResolvedOwner.IsNull())
    {
        if (void* RuntimeComponent = ResolveRuntimeRawFromStorage(m_ecsRuntime, ResolvedOwner, Type))
        {
            return RuntimeComponent;
        }
    }
    return nullptr;
}

const void* World::BorrowedComponent(NodeHandle& InOutOwner, const TypeId& Type) const
{
    NodeHandle ResolvedOwner{};
    const BaseNode* Node = ResolveComponentOwnerNode(*this, InOutOwner, ResolvedOwner);
    if (!Node)
    {
        return nullptr;
    }

    if (!ResolvedOwner.IsNull())
    {
        if (const void* RuntimeComponent = ResolveRuntimeRawFromStorage(m_ecsRuntime, ResolvedOwner, Type))
        {
            return RuntimeComponent;
        }
    }
    return nullptr;
}

BaseComponent* World::BorrowedComponent(ComponentHandle& InOutHandle)
{
    return InOutHandle.Borrowed();
}

const BaseComponent* World::BorrowedComponent(ComponentHandle& InOutHandle) const
{
    return InOutHandle.Borrowed();
}

Result World::RemoveComponentByType(NodeHandle& InOutOwner, const TypeId& Type)
{
    BaseNode* Node = BorrowedNode(InOutOwner);
    if (!Node)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Node not found"));
    }

    if (InOutOwner.IsNull())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Owner node handle was not found"));
    }

    auto RuntimeComponentHandle = m_ecsRuntime.ComponentHandle(InOutOwner, Type);
    if (!RuntimeComponentHandle)
    {
        return std::unexpected(RuntimeComponentHandle.error());
    }

    auto RemoveRuntimeResult = m_ecsRuntime.RemoveComponent(*this, InOutOwner, Type);
    if (!RemoveRuntimeResult)
    {
        return std::unexpected(RemoveRuntimeResult.error());
    }

    UnregisterRuntimeTypeOnNode(*Node, Type);
    return Ok();
}

TExpected<void*> World::CreateComponent(NodeHandle& InOutOwner, const TypeId& Type)
{
    return ComponentSerializationRegistry::Instance().Create(*this, InOutOwner, Type);
}

TExpected<void*> World::CreateComponentWithId(NodeHandle& InOutOwner, const TypeId& Type, const Uuid& Id)
{
    return ComponentSerializationRegistry::Instance().CreateWithId(*this, InOutOwner, Type, Id);
}

Result World::RequestNodeOnCreate(NodeHandle& InOutHandle)
{
    if (InOutHandle.IsNull())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Node handle is null"));
    }

    BaseNode* Node = BorrowedNode(InOutHandle);
    if (!Node)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Node not found"));
    }

    if (m_deferNodeOnCreateCallbacks)
    {
        if (std::find(m_pendingNodeOnCreate.begin(), m_pendingNodeOnCreate.end(), InOutHandle) == m_pendingNodeOnCreate.end())
        {
            m_pendingNodeOnCreate.push_back(InOutHandle);
        }
        return Ok();
    }

    (void)Node;
    (void)m_ecsRuntime.FlushPendingNodeOnCreate(*this, InOutHandle);
    return Ok();
}

bool World::AreNodeOnCreateCallbacksDeferred() const
{
    return m_deferNodeOnCreateCallbacks;
}

bool World::IsServer() const
{
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    return m_networkSystem.IsServer();
#else
    return true;
#endif
}

bool World::IsClient() const
{
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    return m_networkSystem.IsClient();
#else
    return false;
#endif
}

bool World::IsListenServer() const
{
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    return m_networkSystem.IsListenServer();
#else
    return false;
#endif
}

void World::SetWorldKind(const EWorldKind Kind)
{
    m_worldKind = Kind;
}

const WorldExecutionProfile& World::ExecutionProfile() const
{
    return m_executionProfile;
}

void World::SetExecutionProfile(const WorldExecutionProfile& Profile)
{
    m_executionProfile = Profile;
}

void World::DeferNodeOnCreateCallbacks(const bool Deferred)
{
    m_deferNodeOnCreateCallbacks = Deferred;
}

Result World::FlushDeferredNodeOnCreate()
{
    m_deferNodeOnCreateCallbacks = false;

    std::vector<NodeHandle> Pending = std::move(m_pendingNodeOnCreate);
    m_pendingNodeOnCreate.clear();

    for (const NodeHandle& Handle : Pending)
    {
        NodeHandle ResolvedHandle = Handle;
        if (!BorrowedNode(ResolvedHandle))
        {
            continue;
        }

        (void)m_ecsRuntime.FlushPendingNodeOnCreate(*this, ResolvedHandle);
    }

    return Ok();
}

void World::Tick(const float DeltaSeconds)
{
    TaskDispatcherScope DispatcherScope(*this);
    ExecuteQueuedTasks();
    if (auto HotReloadResult = m_scriptRuntime.TickHotReload(); !HotReloadResult)
    {
        std::cerr << "Warning: Script hot reload tick failed: " << HotReloadResult.error().Message << '\n';
    }
#if defined(SNAPI_GF_ENABLE_INPUT)
    if (ShouldTickInput() && m_inputSystem.IsInitialized())
    {
        (void)m_inputSystem.Pump();
    }
#endif
#if defined(SNAPI_GF_ENABLE_UI)
    if (ShouldTickUI() && m_uiSystem.IsInitialized())
    {
        m_uiSystem.Tick(DeltaSeconds);
    }
#endif
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    if (ShouldPumpNetworking())
    {
        m_networkSystem.ExecuteQueuedTasks();
        if (auto* Session = m_networkSystem.Session())
        {
            
            Session->Pump(Networking::Clock::now());
        }
    }
#endif
    if (ShouldRunGameplay())
    {
        m_ecsRuntime.Tick(*this, DeltaSeconds);
    }
#if defined(WITH_EDITOR) && WITH_EDITOR
    else if (Kind() == EWorldKind::Editor)
    {
        m_ecsRuntime.EditorTick(*this, DeltaSeconds);
    }
#endif
#if defined(SNAPI_GF_ENABLE_PHYSICS)
    if (ShouldSimulatePhysics() && m_physicsSystem.IsInitialized() && m_physicsSystem.TickInVariableTick())
    {
        
        (void)m_physicsSystem.Step(DeltaSeconds);
    }
#endif
#if defined(SNAPI_GF_ENABLE_AUDIO)
    if (ShouldTickAudio())
    {
        
        m_audioSystem.Update(DeltaSeconds);
    }
#endif
}

void World::FixedTick(float DeltaSeconds)
{

    TaskDispatcherScope DispatcherScope(*this);
    ExecuteQueuedTasks();
    if (ShouldRunGameplay())
    {
        m_ecsRuntime.FixedTick(*this, DeltaSeconds);
    }
#if defined(SNAPI_GF_ENABLE_PHYSICS)
    const bool RunPhysicsFixedStep = [this]() {
        
        return ShouldSimulatePhysics() && m_physicsSystem.IsInitialized() && m_physicsSystem.TickInFixedTick();
    }();
    if (RunPhysicsFixedStep)
    {
        if (m_physicsSystem.Settings().AutoRebaseFloatingOrigin)
        {
#if defined(SNAPI_GF_ENABLE_RENDERER)
            if (const auto* ActiveCamera = m_rendererSystem.ActiveCamera())
            {
                {
                    
                    const auto CameraPos = ActiveCamera->Position();
                    const SnAPI::Physics::Vec3 AnchorWorld{
                        static_cast<SnAPI::Physics::Vec3::Scalar>(CameraPos.x()),
                        static_cast<SnAPI::Physics::Vec3::Scalar>(CameraPos.y()),
                        static_cast<SnAPI::Physics::Vec3::Scalar>(CameraPos.z())};
                    (void)m_physicsSystem.EnsureFloatingOriginNear(AnchorWorld);
                }
            }
#endif
        }
        
        (void)m_physicsSystem.Step(DeltaSeconds);
    }
#endif
}

void World::LateTick(const float DeltaSeconds)
{

    TaskDispatcherScope DispatcherScope(*this);
    ExecuteQueuedTasks();
    if (ShouldRunGameplay())
    {
        m_ecsRuntime.LateTick(*this, DeltaSeconds);
    }
}

void World::EndFrame()
{

    TaskDispatcherScope DispatcherScope(*this);
    ExecuteQueuedTasks();
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    if (ShouldPumpNetworking())
    {
        m_networkSystem.ExecuteQueuedTasks();
    }
#endif
    if (ShouldRunNodeEndFrame())
    {
        for (const NodeHandle& Handle : m_pendingDestroy)
        {
            BaseNode* Node = ResolveNodeIncludingPendingDestroy(m_ecsRuntime, Handle);
            if (!Node)
            {
                continue;
            }

            if (!Node->Parent().IsNull())
            {
                if (BaseNode* ParentNode = ResolveNodeIncludingPendingDestroy(m_ecsRuntime, Node->Parent()))
                {
                    ParentNode->RemoveChild(Handle);
                }
            }
            else
            {
                m_rootNodes.erase(std::remove(m_rootNodes.begin(), m_rootNodes.end(), Handle), m_rootNodes.end());
            }

            m_ecsRuntime.DestroyComponentsOnNode(*this, Handle);
            ObjectRegistry::Instance().Unregister(Handle.Id);
        }

        m_ecsRuntime.EndFrame(*this);
        m_pendingDestroy.clear();
    }
#if defined(SNAPI_GF_ENABLE_RENDERER)
#if defined(SNAPI_GF_ENABLE_UI)
    if (ShouldBuildUiRenderPackets() && m_rendererSystem.IsInitialized() && m_uiSystem.IsInitialized())
    {
        auto BindFontMetrics = [&](SnAPI::UI::UIContext* UiContext, SnAPI::UI::IFontMetrics* Metrics) {
            if (UiContext)
            {
                UiContext->GetPacketWriter().SetFontMetrics(Metrics);
            }
        };

        SnAPI::UI::IFontMetrics* Metrics = nullptr;
        static UiFontMetricsAdapter FontMetricsAdapter{};
        if (auto* FontFace = m_rendererSystem.EnsureDefaultFontFace())
        {
            FontMetricsAdapter.Bind(FontFace);
            Metrics = &FontMetricsAdapter;
        }

        const auto ContextIds = m_uiSystem.ContextIds();
        for (const auto ContextId : ContextIds)
        {
            BindFontMetrics(m_uiSystem.Context(ContextId), Metrics);
        }

        std::vector<UISystem::ViewportPacketBatch> ViewportBatches{};
        if (auto BuildViewportPacketsResult = m_uiSystem.BuildBoundViewportRenderPackets(ViewportBatches); BuildViewportPacketsResult)
        {
            for (auto& Batch : ViewportBatches)
            {
                if (!Batch.ContextPtr || Batch.Viewport == 0)
                {
                    continue;
                }

                (void)m_rendererSystem.QueueUiRenderPackets(Batch.Viewport, *Batch.ContextPtr, Batch.Packets);
            }
        }
    }
#endif
    if (ShouldRenderFrame())
    {
        
        m_rendererSystem.EndFrame();
    }
#endif
}

void World::Clear()
{
    m_pendingNodeOnCreate.clear();
    m_deferNodeOnCreateCallbacks = false;
    m_rootNodes.clear();
    m_pendingDestroy.clear();
    m_ecsRuntime.Clear(*this);
}

bool World::FixedTickEnabled() const
{
    return m_fixedTickEnabled;
}

float World::FixedTickDeltaSeconds() const
{
    return m_fixedTickDeltaSeconds;
}

float World::FixedTickInterpolationAlpha() const
{
    return m_fixedTickInterpolationAlpha;
}

void World::SetFixedTickFrameState(const bool Enabled, const float FixedDeltaSeconds, const float InterpolationAlpha)
{
    m_fixedTickEnabled = Enabled;
    m_fixedTickDeltaSeconds = Enabled ? std::max(0.0f, FixedDeltaSeconds) : 0.0f;
    m_fixedTickInterpolationAlpha = Enabled ? std::clamp(InterpolationAlpha, 0.0f, 1.0f) : 1.0f;
}

TExpected<NodeHandle> World::CreateLevel(std::string Name)
{
    
    return CreateNode<Level>(std::move(Name));
}

TExpectedRef<Level> World::LevelRef(NodeHandle& InOutHandle)
{
    if (auto* Node = BorrowedNode(InOutHandle))
    {
        if (TypeRegistry::Instance().IsA(Node->TypeKey(), StaticTypeId<Level>()))
        {
            return *static_cast<Level*>(Node);
        }
    }
    return std::unexpected(MakeError(EErrorCode::NotFound, "Level not found"));
}

TExpected<RuntimeComponentHandle> World::AddRuntimeComponent(NodeHandle& InOutOwner, const TypeId& Type)
{
    return m_ecsRuntime.AddComponent(*this, InOutOwner, Type);
}

TExpected<RuntimeComponentHandle> World::AddRuntimeComponentWithId(NodeHandle& InOutOwner,
                                                                   const TypeId& Type,
                                                                   const Uuid& Id)
{
    return m_ecsRuntime.AddComponentWithId(*this, InOutOwner, Type, Id);
}

Result World::RemoveRuntimeComponent(NodeHandle& InOutOwner, const TypeId& Type)
{
    return m_ecsRuntime.RemoveComponent(*this, InOutOwner, Type);
}

bool World::HasRuntimeComponent(NodeHandle& InOutOwner, const TypeId& Type) const
{
    return m_ecsRuntime.HasComponent(InOutOwner, Type);
}

TExpected<RuntimeComponentHandle> World::RuntimeComponentByType(NodeHandle& InOutOwner,
                                                                const TypeId& Type) const
{
    return m_ecsRuntime.ComponentHandle(InOutOwner, Type);
}

void* World::ResolveRuntimeComponentRaw(const RuntimeComponentHandle Handle, const TypeId& Type)
{
    return m_ecsRuntime.ResolveComponentRaw(Handle, Type);
}

const void* World::ResolveRuntimeComponentRaw(const RuntimeComponentHandle Handle, const TypeId& Type) const
{
    return m_ecsRuntime.ResolveComponentRaw(Handle, Type);
}

std::vector<NodeHandle> World::Levels() const
{
    std::vector<NodeHandle> Result;
    const_cast<WorldEcsRuntime&>(m_ecsRuntime).ForEachNode([](void* UserData, const NodeHandle& Handle, BaseNode& Node) {
        auto& OutLevels = *static_cast<std::vector<NodeHandle>*>(UserData);
        if (TypeRegistry::Instance().IsA(Node.TypeKey(), StaticTypeId<Level>()))
        {
            OutLevels.push_back(Handle);
        }
    }, &Result);
    return Result;
}

void World::SetGameplayHost(GameplayHost* Host)
{
    m_gameplayHost = Host;
}

GameplayHost* World::GameplayHostPtr()
{
    return m_gameplayHost;
}

const GameplayHost* World::GameplayHostPtr() const
{
    return m_gameplayHost;
}

JobSystem& World::Jobs()
{
    
    return m_jobSystem;
}

WorldEcsRuntime& World::EcsRuntime()
{
    return m_ecsRuntime;
}

const WorldEcsRuntime& World::EcsRuntime() const
{
    return m_ecsRuntime;
}

#if defined(SNAPI_GF_ENABLE_INPUT)
InputSystem& World::Input()
{
    
    return m_inputSystem;
}

const InputSystem& World::Input() const
{
    
    return m_inputSystem;
}
#endif

#if defined(SNAPI_GF_ENABLE_UI)
UISystem& World::UI()
{
    
    return m_uiSystem;
}

const UISystem& World::UI() const
{
    
    return m_uiSystem;
}
#endif

#if defined(SNAPI_GF_ENABLE_AUDIO)
AudioSystem& World::Audio()
{
    
    return m_audioSystem;
}

const AudioSystem& World::Audio() const
{
    
    return m_audioSystem;
}
#endif

#if defined(SNAPI_GF_ENABLE_NETWORKING)
NetworkSystem& World::Networking()
{
    
    return m_networkSystem;
}

const NetworkSystem& World::Networking() const
{
    
    return m_networkSystem;
}
#endif

#if defined(SNAPI_GF_ENABLE_PHYSICS)
PhysicsSystem& World::Physics()
{
    
    return m_physicsSystem;
}

const PhysicsSystem& World::Physics() const
{
    
    return m_physicsSystem;
}
#endif

#if defined(SNAPI_GF_ENABLE_RENDERER)
RendererSystem& World::Renderer()
{
    
    return m_rendererSystem;
}

const RendererSystem& World::Renderer() const
{
    
    return m_rendererSystem;
}
#endif

ScriptRuntimeService& World::Scripts()
{
    return m_scriptRuntime;
}

const ScriptRuntimeService& World::Scripts() const
{
    return m_scriptRuntime;
}

} // namespace SnAPI::GameFramework
