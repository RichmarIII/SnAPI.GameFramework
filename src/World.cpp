#include "World.h"
#include "AudioListenerComponent.h"
#include "AudioSourceComponent.h"
#include "CameraComponent.h"
#include "CharacterMovementController.h"
#include "FollowTargetComponent.h"
#include "InputComponent.h"
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

RuntimeNodeHandle ResolveRuntimeOwnerHandle(IWorld& WorldRef, BaseNode& Node)
{
    RuntimeNodeHandle OwnerRuntime = Node.RuntimeNode();
    if (!OwnerRuntime.IsNull() && WorldRef.EcsRuntime().Nodes().Resolve(OwnerRuntime))
    {
        return OwnerRuntime;
    }

    auto RuntimeHandleResult = WorldRef.RuntimeNodeById(Node.Id());
    if (!RuntimeHandleResult)
    {
        Node.RuntimeNode({});
        return {};
    }

    OwnerRuntime = RuntimeHandleResult.value();
    Node.RuntimeNode(OwnerRuntime);
    return OwnerRuntime;
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
    auto& Storages = Node.ComponentStorages();
    for (std::size_t Index = 0; Index < Types.size(); ++Index)
    {
        if (Types[Index] != Type)
        {
            continue;
        }

        auto TypeIt = Types.begin() + static_cast<std::vector<TypeId>::difference_type>(Index);
        Types.erase(TypeIt);
        if (Index < Storages.size())
        {
            auto StorageIt = Storages.begin() + static_cast<std::vector<ComponentStorageView*>::difference_type>(Index);
            Storages.erase(StorageIt);
        }
        break;
    }

    static const TypeId RelevanceType = StaticTypeId<RelevanceComponent>();
    if (Type == RelevanceType)
    {
        Node.RelevanceState(nullptr);
    }
}

void* ResolveRuntimeRawFromStorage(WorldEcsRuntime& Runtime, const RuntimeNodeHandle OwnerRuntime, const TypeId& Type)
{
    auto RuntimeComponentHandle = Runtime.ComponentHandle(OwnerRuntime, Type);
    if (!RuntimeComponentHandle)
    {
        return nullptr;
    }
    return Runtime.ResolveComponentRaw(*RuntimeComponentHandle, Type);
}

const void* ResolveRuntimeRawFromStorage(const WorldEcsRuntime& Runtime,
                                         const RuntimeNodeHandle OwnerRuntime,
                                         const TypeId& Type)
{
    auto RuntimeComponentHandle = Runtime.ComponentHandle(OwnerRuntime, Type);
    if (!RuntimeComponentHandle)
    {
        return nullptr;
    }
    return Runtime.ResolveComponentRaw(*RuntimeComponentHandle, Type);
}

BaseNode* ResolveNodeIncludingPendingDestroy(const std::shared_ptr<TObjectPool<BaseNode>>& NodePool,
                                             const NodeHandle& Handle)
{
    if (!NodePool)
    {
        return nullptr;
    }

    if (BaseNode* Node = NodePool->Borrowed(Handle))
    {
        return Node;
    }

    return ObjectRegistry::Instance().Resolve<BaseNode>(Handle.Id);
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
    : Level("World")
    , m_nodePool(std::make_shared<TObjectPool<BaseNode>>())
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    , m_networkSystem(*this)
#endif
{
    
    TypeKey(StaticTypeId<World>());
    BaseNode::World(this);
    m_worldKind = EWorldKind::Runtime;
    m_executionProfile = WorldExecutionProfile::Runtime();
}

World::World(std::string Name)
    : Level(std::move(Name))
    , m_nodePool(std::make_shared<TObjectPool<BaseNode>>())
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    , m_networkSystem(*this)
#endif
{
    
    TypeKey(StaticTypeId<World>());
    BaseNode::World(this);
    m_worldKind = EWorldKind::Runtime;
    m_executionProfile = WorldExecutionProfile::Runtime();
}

World::~World()
{
    Clear();
    BaseNode::World(nullptr);
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

bool World::ShouldTickEcsRuntime() const
{
    return m_executionProfile.TickEcsRuntime;
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

TObjectPool<BaseNode>& World::NodePool()
{
    if (!m_nodePool)
    {
        m_nodePool = std::make_shared<TObjectPool<BaseNode>>();
    }
    return *m_nodePool;
}

const TObjectPool<BaseNode>& World::NodePool() const
{
    return const_cast<World*>(this)->NodePool();
}

void World::ForEachNode(const NodeVisitor Visitor, void* const UserData)
{
    if (!Visitor)
    {
        return;
    }

    NodePool().ForEach([Visitor, UserData](const NodeHandle& Handle, BaseNode& Node) {
        Visitor(UserData, Handle, Node);
    });
}

TExpected<NodeHandle> World::NodeHandleById(const Uuid& Id) const
{
    if (!m_nodePool)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Node storage is not initialized"));
    }
    return m_nodePool->HandleByIdSlow(Id);
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

    const ConstructorInfo* Ctor = nullptr;
    for (const auto& Candidate : Info->Constructors)
    {
        if (Candidate.ParamTypes.empty())
        {
            Ctor = &Candidate;
            break;
        }
    }
    if (!Ctor)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No default constructor registered"));
    }

    std::span<const Variant> EmptyArgs;
    auto InstanceResult = Ctor->Construct(EmptyArgs);
    if (!InstanceResult)
    {
        return std::unexpected(InstanceResult.error());
    }

    auto BasePtr = std::static_pointer_cast<BaseNode>(InstanceResult.value());
    if (!BasePtr)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Node type mismatch"));
    }

    if (!m_nodePool)
    {
        m_nodePool = std::make_shared<TObjectPool<BaseNode>>();
    }

    auto HandleResult = m_nodePool->CreateFromShared(std::move(BasePtr));
    if (!HandleResult)
    {
        return std::unexpected(HandleResult.error());
    }

    const NodeHandle Handle = *HandleResult;
    BaseNode* Node = m_nodePool->Borrowed(Handle);
    if (!Node)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Created node could not be resolved"));
    }

    Node->Handle(Handle);
    Node->Name(std::move(Name));
    Node->World(this);
    Node->RuntimeNode({});
    Node->PendingDestroy(false);
    Node->Parent({});
    Node->TypeKey(Type);

    ObjectRegistry::Instance().RegisterNode(
        Node->Id(),
        Node,
        Handle.RuntimePoolToken,
        Handle.RuntimeIndex,
        Handle.RuntimeGeneration);

    auto RuntimeCreateResult = m_ecsRuntime.Nodes().CreateNodeWithId(*this, Node->Id(), Node->Name(), Node->TypeKey());
    if (!RuntimeCreateResult)
    {
        ObjectRegistry::Instance().Unregister(Node->Id());
        (void)m_nodePool->DestroyLater(Handle);
        m_nodePool->EndFrame();
        return std::unexpected(RuntimeCreateResult.error());
    }
    Node->RuntimeNode(*RuntimeCreateResult);
    m_rootNodes.push_back(Handle);
    return Handle;
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

    const ConstructorInfo* Ctor = nullptr;
    for (const auto& Candidate : Info->Constructors)
    {
        if (Candidate.ParamTypes.empty())
        {
            Ctor = &Candidate;
            break;
        }
    }
    if (!Ctor)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No default constructor registered"));
    }

    std::span<const Variant> EmptyArgs;
    auto InstanceResult = Ctor->Construct(EmptyArgs);
    if (!InstanceResult)
    {
        return std::unexpected(InstanceResult.error());
    }

    auto BasePtr = std::static_pointer_cast<BaseNode>(InstanceResult.value());
    if (!BasePtr)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Node type mismatch"));
    }

    if (!m_nodePool)
    {
        m_nodePool = std::make_shared<TObjectPool<BaseNode>>();
    }

    auto HandleResult = m_nodePool->CreateFromSharedWithId(std::move(BasePtr), Id);
    if (!HandleResult)
    {
        return std::unexpected(HandleResult.error());
    }

    const NodeHandle Handle = *HandleResult;
    BaseNode* Node = m_nodePool->Borrowed(Handle);
    if (!Node)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Created node could not be resolved"));
    }

    Node->Handle(Handle);
    Node->Name(std::move(Name));
    Node->World(this);
    Node->RuntimeNode({});
    Node->PendingDestroy(false);
    Node->Parent({});
    Node->TypeKey(Type);

    ObjectRegistry::Instance().RegisterNode(
        Node->Id(),
        Node,
        Handle.RuntimePoolToken,
        Handle.RuntimeIndex,
        Handle.RuntimeGeneration);

    auto RuntimeCreateResult = m_ecsRuntime.Nodes().CreateNodeWithId(*this, Node->Id(), Node->Name(), Node->TypeKey());
    if (!RuntimeCreateResult)
    {
        ObjectRegistry::Instance().Unregister(Node->Id());
        (void)m_nodePool->DestroyLater(Handle);
        m_nodePool->EndFrame();
        return std::unexpected(RuntimeCreateResult.error());
    }
    Node->RuntimeNode(*RuntimeCreateResult);
    m_rootNodes.push_back(Handle);
    return Handle;
}

Result World::DestroyNode(const NodeHandle& Handle)
{
    if (Handle.IsNull())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Node handle is null"));
    }
    if (!m_nodePool)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Node storage is not initialized"));
    }

    BaseNode* RootNode = m_nodePool->Borrowed(Handle);
    if (!RootNode)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Node not found"));
    }

    std::vector<NodeHandle> Stack{};
    Stack.push_back(Handle);
    while (!Stack.empty())
    {
        const NodeHandle CurrentHandle = Stack.back();
        Stack.pop_back();

        BaseNode* CurrentNode = m_nodePool->Borrowed(CurrentHandle);
        if (!CurrentNode || CurrentNode->PendingDestroy())
        {
            continue;
        }

        for (const NodeHandle Child : CurrentNode->Children())
        {
            Stack.push_back(Child);
        }

        auto DestroyLaterResult = m_nodePool->DestroyLater(CurrentHandle);
        if (!DestroyLaterResult)
        {
            return std::unexpected(DestroyLaterResult.error());
        }

        CurrentNode->PendingDestroy(true);
        m_pendingDestroy.push_back(CurrentHandle);
    }

    return Ok();
}

Result World::AttachChild(const NodeHandle& Parent, const NodeHandle& Child)
{
    if (!m_nodePool)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Node storage is not initialized"));
    }

    BaseNode* ParentNode = m_nodePool->Borrowed(Parent);
    BaseNode* ChildNode = m_nodePool->Borrowed(Child);
    if (!ParentNode || !ChildNode)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Parent or child not found"));
    }
    if (!ChildNode->Parent().IsNull())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Child already has a parent"));
    }

    ParentNode->AddChildResolved(Child, ChildNode);
    ChildNode->Parent(Parent);
    ChildNode->World(this);

    m_rootNodes.erase(std::remove(m_rootNodes.begin(), m_rootNodes.end(), Child), m_rootNodes.end());

    const RuntimeNodeHandle ParentRuntime = ResolveRuntimeOwnerHandle(*this, *ParentNode);
    const RuntimeNodeHandle ChildRuntime = ResolveRuntimeOwnerHandle(*this, *ChildNode);
    if (!ParentRuntime.IsNull() && !ChildRuntime.IsNull())
    {
        auto AttachRuntimeResult = m_ecsRuntime.Nodes().AttachChild(ParentRuntime, ChildRuntime);
        if (!AttachRuntimeResult)
        {
            ParentNode->RemoveChild(Child);
            ChildNode->Parent({});
            if (std::find(m_rootNodes.begin(), m_rootNodes.end(), Child) == m_rootNodes.end())
            {
                m_rootNodes.push_back(Child);
            }
            return std::unexpected(AttachRuntimeResult.error());
        }
    }

    return Ok();
}

Result World::DetachChild(const NodeHandle& Child)
{
    if (!m_nodePool)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Node storage is not initialized"));
    }

    BaseNode* ChildNode = m_nodePool->Borrowed(Child);
    if (!ChildNode)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Child not found"));
    }

    if (!ChildNode->Parent().IsNull())
    {
        if (BaseNode* ParentNode = m_nodePool->Borrowed(ChildNode->Parent()))
        {
            ParentNode->RemoveChild(Child);
        }
        ChildNode->Parent({});
    }

    const RuntimeNodeHandle ChildRuntime = ResolveRuntimeOwnerHandle(*this, *ChildNode);
    if (!ChildRuntime.IsNull())
    {
        auto DetachRuntimeResult = m_ecsRuntime.Nodes().DetachChild(ChildRuntime);
        if (!DetachRuntimeResult)
        {
            return std::unexpected(DetachRuntimeResult.error());
        }
    }

    if (std::find(m_rootNodes.begin(), m_rootNodes.end(), Child) == m_rootNodes.end())
    {
        m_rootNodes.push_back(Child);
    }
    return Ok();
}

void* World::BorrowedComponent(const NodeHandle& Owner, const TypeId& Type)
{
    BaseNode* Node = NodePool().Borrowed(Owner);
    if (!Node)
    {
        return nullptr;
    }

    const RuntimeNodeHandle OwnerRuntime = ResolveRuntimeOwnerHandle(*this, *Node);
    if (!OwnerRuntime.IsNull())
    {
        if (void* RuntimeComponent = ResolveRuntimeRawFromStorage(m_ecsRuntime, OwnerRuntime, Type))
        {
            return RuntimeComponent;
        }
    }
    return nullptr;
}

const void* World::BorrowedComponent(const NodeHandle& Owner, const TypeId& Type) const
{
    const BaseNode* Node = NodePool().Borrowed(Owner);
    if (!Node)
    {
        return nullptr;
    }

    RuntimeNodeHandle OwnerRuntime = Node->RuntimeNode();
    if (OwnerRuntime.IsNull() || !m_ecsRuntime.Nodes().Resolve(OwnerRuntime))
    {
        auto RuntimeHandleResult = RuntimeNodeById(Node->Id());
        if (RuntimeHandleResult)
        {
            OwnerRuntime = RuntimeHandleResult.value();
        }
        else
        {
            OwnerRuntime = {};
        }
    }
    if (!OwnerRuntime.IsNull())
    {
        if (const void* RuntimeComponent = ResolveRuntimeRawFromStorage(m_ecsRuntime, OwnerRuntime, Type))
        {
            return RuntimeComponent;
        }
    }
    return nullptr;
}

Result World::RemoveComponentByType(const NodeHandle& Owner, const TypeId& Type)
{
    BaseNode* Node = NodePool().Borrowed(Owner);
    if (!Node)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Node not found"));
    }

    const RuntimeNodeHandle OwnerRuntime = ResolveRuntimeOwnerHandle(*this, *Node);
    if (OwnerRuntime.IsNull())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Runtime owner node was not found"));
    }

    auto RuntimeComponentHandle = m_ecsRuntime.ComponentHandle(OwnerRuntime, Type);
    if (!RuntimeComponentHandle)
    {
        return std::unexpected(RuntimeComponentHandle.error());
    }

    auto RemoveRuntimeResult = m_ecsRuntime.RemoveComponent(*this, OwnerRuntime, Type);
    if (!RemoveRuntimeResult)
    {
        return std::unexpected(RemoveRuntimeResult.error());
    }

    UnregisterRuntimeTypeOnNode(*Node, Type);
    return Ok();
}

TExpected<void*> World::CreateComponent(const NodeHandle& Owner, const TypeId& Type)
{
    return ComponentSerializationRegistry::Instance().Create(*this, Owner, Type);
}

TExpected<void*> World::CreateComponentWithId(const NodeHandle& Owner, const TypeId& Type, const Uuid& Id)
{
    return ComponentSerializationRegistry::Instance().CreateWithId(*this, Owner, Type, Id);
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

void World::Tick(const float DeltaSeconds)
{
    TaskDispatcherScope DispatcherScope(*this);
    ExecuteQueuedTasks();
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
    if (ShouldTickEcsRuntime())
    {
        m_ecsRuntime.Tick(*this, DeltaSeconds);
    }
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
    if (ShouldTickEcsRuntime())
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
    if (ShouldTickEcsRuntime())
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
            BaseNode* Node = ResolveNodeIncludingPendingDestroy(m_nodePool, Handle);
            if (!Node)
            {
                continue;
            }

            if (!Node->Parent().IsNull())
            {
                if (BaseNode* ParentNode = ResolveNodeIncludingPendingDestroy(m_nodePool, Node->Parent()))
                {
                    ParentNode->RemoveChild(Handle);
                }
            }
            else
            {
                m_rootNodes.erase(std::remove(m_rootNodes.begin(), m_rootNodes.end(), Handle), m_rootNodes.end());
            }

            const RuntimeNodeHandle RuntimeHandle = Node->RuntimeNode();
            if (!RuntimeHandle.IsNull())
            {
                (void)m_ecsRuntime.DestroyRuntimeNode(*this, RuntimeHandle);
                Node->RuntimeNode({});
            }

            ObjectRegistry::Instance().Unregister(Handle.Id);
        }

        if (m_nodePool)
        {
            m_nodePool->EndFrame();
        }
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
    if (m_nodePool)
    {
        m_nodePool->ForEachAll([&](const NodeHandle& Handle, BaseNode& Node) {
            (void)Node;
            ObjectRegistry::Instance().Unregister(Handle.Id);
        });
        m_nodePool->Clear();
    }

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

TExpectedRef<Level> World::LevelRef(const NodeHandle& Handle)
{
    
    if (auto* Node = Handle.Borrowed())
    {
        if (TypeRegistry::Instance().IsA(Node->TypeKey(), StaticTypeId<Level>()))
        {
            return *static_cast<Level*>(Node);
        }
    }
    return std::unexpected(MakeError(EErrorCode::NotFound, "Level not found"));
}

TExpected<RuntimeNodeHandle> World::CreateRuntimeNode(std::string Name, const TypeId& Type)
{
    return m_ecsRuntime.Nodes().CreateNode(*this, std::move(Name), Type);
}

TExpected<RuntimeNodeHandle> World::CreateRuntimeNodeWithId(const Uuid& Id, std::string Name, const TypeId& Type)
{
    return m_ecsRuntime.Nodes().CreateNodeWithId(*this, Id, std::move(Name), Type);
}

Result World::DestroyRuntimeNode(const RuntimeNodeHandle Handle)
{
    return m_ecsRuntime.DestroyRuntimeNode(*this, Handle);
}

Result World::AttachRuntimeChild(const RuntimeNodeHandle Parent, const RuntimeNodeHandle Child)
{
    return m_ecsRuntime.Nodes().AttachChild(Parent, Child);
}

Result World::DetachRuntimeChild(const RuntimeNodeHandle Child)
{
    return m_ecsRuntime.Nodes().DetachChild(Child);
}

TExpected<RuntimeNodeHandle> World::RuntimeNodeById(const Uuid& Id) const
{
    return m_ecsRuntime.Nodes().HandleById(Id);
}

RuntimeNodeHandle World::RuntimeParent(const RuntimeNodeHandle Child) const
{
    return m_ecsRuntime.Nodes().Parent(Child);
}

std::vector<RuntimeNodeHandle> World::RuntimeChildren(const RuntimeNodeHandle Parent) const
{
    return m_ecsRuntime.Nodes().Children(Parent);
}

void World::ForEachRuntimeChild(const RuntimeNodeHandle Parent,
                                const RuntimeChildVisitor Visitor,
                                void* const UserData) const
{
    if (!Visitor)
    {
        return;
    }

    m_ecsRuntime.Nodes().ForEachChild(Parent, [&](const RuntimeNodeHandle Child) {
        Visitor(UserData, Child);
    });
}

std::vector<RuntimeNodeHandle> World::RuntimeRoots() const
{
    const auto& Roots = m_ecsRuntime.Nodes().Roots();
    return {Roots.begin(), Roots.end()};
}

TExpected<RuntimeComponentHandle> World::AddRuntimeComponent(const RuntimeNodeHandle Owner, const TypeId& Type)
{
    return m_ecsRuntime.AddComponent(*this, Owner, Type);
}

TExpected<RuntimeComponentHandle> World::AddRuntimeComponentWithId(const RuntimeNodeHandle Owner,
                                                                   const TypeId& Type,
                                                                   const Uuid& Id)
{
    return m_ecsRuntime.AddComponentWithId(*this, Owner, Type, Id);
}

Result World::RemoveRuntimeComponent(const RuntimeNodeHandle Owner, const TypeId& Type)
{
    return m_ecsRuntime.RemoveComponent(*this, Owner, Type);
}

bool World::HasRuntimeComponent(const RuntimeNodeHandle Owner, const TypeId& Type) const
{
    return m_ecsRuntime.HasComponent(Owner, Type);
}

TExpected<RuntimeComponentHandle> World::RuntimeComponentByType(const RuntimeNodeHandle Owner,
                                                                const TypeId& Type) const
{
    return m_ecsRuntime.ComponentHandle(Owner, Type);
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
    NodePool().ForEach([&](const NodeHandle& Handle, BaseNode& Node) {
        if (TypeRegistry::Instance().IsA(Node.TypeKey(), StaticTypeId<Level>()))
        {
            Result.push_back(Handle);
        }
    });
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

} // namespace SnAPI::GameFramework
