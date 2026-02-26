#pragma once

#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "Expected.h"
#include "Handle.h"
#include "Handles.h"
#include "NodeComponentContracts.h"
#include "StaticTypeId.h"
#include "Uuid.h"
#include "WorldEcsRuntime.h"

namespace SnAPI::GameFramework
{

class IWorld;
class ComponentStorageView;
class RelevanceComponent;
class Variant;

/**
 * @brief Canonical concrete node implementation used by world-owned storage.
 * @remarks
 * `BaseNode` provides:
 * - hierarchy bookkeeping (`Parent` / `Children`)
 * - identity and reflection identity (`Handle` / `TypeKey`)
 * - runtime role helpers (`IsServer` / `IsClient` / `IsListenServer`)
 * - component convenience APIs (`Add<T>`, `Component<T>`, `Has<T>`, `Remove<T>`)
 *
 * Ownership model:
 * - Node storage and lifetime are owned externally by `IWorld`/`TObjectPool`.
 * - `m_world` is a non-owning pointer updated by world runtime code.
 * - Pointer stability is tied to pool lifetime; handles remain the public identity boundary.
 *
 * Tick model:
 * - World-owned ECS runtime storages drive all phase dispatch.
 * - Node/component runtime types expose optional `*Impl` hooks checked at compile time.
 * - Absent phases are skipped entirely for that storage.
 */
class BaseNode : public NodeCRTP<BaseNode>
{
public:
    /** @brief Stable type name used for reflection. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::BaseNode";

    /**
     * @brief Construct a node with default name.
     */
    BaseNode()
        : m_typeId(StaticTypeId<BaseNode>())
    {
    }
    /**
     * @brief Construct a node with a custom name.
     * @param InName Node name.
     */
    explicit BaseNode(std::string InName)
        : m_name(std::move(InName))
        , m_typeId(StaticTypeId<BaseNode>())
    {
    }

    ~BaseNode() = default;

    void Tick(float DeltaSeconds) { (void)DeltaSeconds; }
    void FixedTick(float DeltaSeconds) { (void)DeltaSeconds; }
    void LateTick(float DeltaSeconds) { (void)DeltaSeconds; }
    void EndFrame() {}

    /**
     * @brief Get the node name.
     * @return Name string.
     */
    const std::string& Name() const
    {
        return m_name;
    }

    /**
     * @brief Set the node name.
     * @param Name New name.
     */
    void Name(std::string Name)
    {
        m_name = std::move(Name);
    }

    /**
     * @brief Get the node handle.
     * @return NodeHandle for this node.
     */
    NodeHandle Handle() const
    {
        return m_self;
    }

    /**
     * @brief Set the node handle.
     * @param Handle New handle.
     * @remarks
     * Typically assigned exactly once by world-owned storage at creation.
     * Reassigning on a live registered object can invalidate external handle references.
     */
    void Handle(const NodeHandle& Handle)
    {
        m_self = Handle;
    }

    /**
     * @brief Get the node UUID.
     * @return UUID value.
     */
    const Uuid& Id() const
    {
        return m_self.Id;
    }

    /**
     * @brief Set the node UUID.
     * @param Id UUID value.
     * @remarks
     * Mutates identity by replacing the internal handle payload.
     * Callers must synchronize `ObjectRegistry` and any external references when using this.
     */
    void Id(Uuid Id)
    {
        m_self.Id = std::move(Id);
    }

    /**
     * @brief Get the reflected type id for this node.
     * @return TypeId value.
     */
    const TypeId& TypeKey() const
    {
        return m_typeId;
    }

    /**
     * @brief Set the reflected type id for this node.
     * @param Id TypeId value.
     * @remarks
     * Reflection systems (serialization, RPC lookup, replication metadata queries) depend on
     * this value being accurate for the concrete node type.
     */
    void TypeKey(const TypeId& Id)
    {
        m_typeId = Id;
    }

    /**
     * @brief Get the parent node handle.
     * @return Parent handle or null handle if root.
     */
    NodeHandle Parent() const
    {
        return m_parent;
    }

    /**
     * @brief Set the parent node handle.
     * @param Parent Parent handle.
     * @remarks
     * Local assignment only. Correct hierarchy updates should also mutate the parent's
     * child list and root-node membership (`IWorld::AttachChild` / `DetachChild`).
     */
    void Parent(const NodeHandle& Parent)
    {
        m_parent = Parent;
    }

    /**
     * @brief Get the list of child handles.
     * @return Vector of child handles.
     */
    const std::vector<NodeHandle>& Children() const
    {
        return m_children;
    }

    /**
     * @brief Add a child handle to the node.
     * @param Child Child handle.
     * @remarks
     * This appends only to local child bookkeeping; it does not enforce uniqueness and does
     * not modify child-side ownership/parent pointers.
     */
    void AddChild(const NodeHandle& Child)
    {
        m_children.push_back(Child);
        m_childNodes.push_back(nullptr);
    }

    /**
     * @brief Add a child with a resolved pointer cache entry.
     * @param Child Child handle.
     * @param ChildNode Resolved child node pointer.
     * @remarks
     * Internal fast-path used by world-owned hierarchy code to avoid first-frame resolve cost.
     */
    void AddChildResolved(const NodeHandle& Child, BaseNode* ChildNode)
    {
        m_children.push_back(Child);
        m_childNodes.push_back(ChildNode);
    }

    /**
     * @brief Remove a child handle from the node.
     * @param Child Child handle to remove.
     * @remarks
     * Performs first-match erase. If duplicate child handles were inserted, later duplicates
     * remain until explicitly removed.
     */
    void RemoveChild(const NodeHandle& Child)
    {
        for (size_t Index = 0; Index < m_children.size(); ++Index)
        {
            if (m_children[Index] == Child)
            {
                auto ChildIt = m_children.begin() + static_cast<std::vector<NodeHandle>::difference_type>(Index);
                m_children.erase(ChildIt);
                if (Index < m_childNodes.size())
                {
                    auto CacheIt = m_childNodes.begin() + static_cast<std::vector<BaseNode*>::difference_type>(Index);
                    m_childNodes.erase(CacheIt);
                }
                return;
            }
        }
    }

    /**
     * @brief Check if the node is active.
     * @return True if active.
     * @remarks Inactive nodes are skipped during tick.
     */
    bool Active() const
    {
        return m_active;
    }

    /**
     * @brief Set the active state for the node.
     * @param Active New active state.
     * @remarks
     * Active=false suppresses this node's tick hooks during traversal.
     * This is an execution-state toggle, not a destruction or detachment operation.
     */
    void Active(bool Active)
    {
        m_active = Active;
    }

    /**
     * @brief Check if the node is replicated over the network.
     * @return True if replicated.
     */
    bool Replicated() const
    {
        return m_replicated;
    }

    /**
     * @brief Set whether the node is replicated over the network.
     * @param Replicated New replicated state.
     * @remarks
     * Runtime replication gate: node snapshots/spawns are skipped unless true.
     * Field-level replication flags are evaluated only after this object-level gate passes.
     */
    void Replicated(bool Replicated)
    {
        m_replicated = Replicated;
    }

    /**
     * @brief Check whether this node is queued for deferred destruction.
     * @return True when destruction has been scheduled but not yet flushed.
     * @remarks
     * Used by hot-path activity checks to avoid UUID set lookups while preserving
     * end-of-frame deferred destruction semantics.
     */
    bool PendingDestroy() const
    {
        return m_pendingDestroy;
    }

    /**
     * @brief Mark whether this node is queued for deferred destruction.
     * @param Pending New pending-destroy state.
     * @remarks Managed by world destroy/end-frame lifecycle paths.
     */
    void PendingDestroy(bool Pending)
    {
        m_pendingDestroy = Pending;
    }

    /**
     * @brief Check whether this node is editor-transient and should be excluded from persistence.
     * @return True when the node is flagged transient for editor preview/runtime-only use.
     */
    bool EditorTransient() const
    {
        return m_editorTransient;
    }

    /**
     * @brief Mark this node as editor-transient.
     * @param Transient New transient state.
     * @remarks
     * Editor-transient nodes are intended for visualization helpers (for example, preview-only instances)
     * and should not be serialized into level/world assets.
     */
    void EditorTransient(const bool Transient)
    {
        m_editorTransient = Transient;
    }

    /**
     * @brief True when this node executes with server authority.
     * @remarks Derived from world networking role; false when unbound to a world/session.
     */
    bool IsServer() const;
    /**
     * @brief True when this node executes in client context.
     * @remarks Derived from world networking role; false when unbound to a world/session.
     */
    bool IsClient() const;
    /**
     * @brief True when this node executes as listen-server.
     * @remarks True when both server and client roles are active in the attached session.
     */
    bool IsListenServer() const;

    /**
     * @brief Possession callback invoked when a LocalPlayer begins possessing this node.
     * @param PlayerHandle Handle of the possessing LocalPlayer.
     * @remarks Default implementation is a no-op.
     */
    void OnPossess(const NodeHandle& PlayerHandle)
    {
        (void)PlayerHandle;
    }

    /**
     * @brief Possession callback invoked when a LocalPlayer stops possessing this node.
     * @param PlayerHandle Handle of the unpossessing LocalPlayer.
     * @remarks Default implementation is a no-op.
     */
    void OnUnpossess(const NodeHandle& PlayerHandle)
    {
        (void)PlayerHandle;
    }

    /**
     * @brief Dispatch a reflected RPC method for this node.
     * @param MethodName Reflected method name.
     * @param Args Variant-packed arguments.
     * @return True when dispatch succeeded (local invoke or queued network call).
     */
    bool CallRPC(std::string_view MethodName, std::span<const Variant> Args = {});

    /**
     * @brief Initializer-list convenience overload for `CallRPC`.
     */
    bool CallRPC(std::string_view MethodName, std::initializer_list<Variant> Args);

    /**
     * @brief Access the list of component type ids.
     * @return Mutable reference to the type id list.
     * @remarks Maintained by world storage bookkeeping; external direct edits are discouraged.
     */
    std::vector<TypeId>& ComponentTypes()
    {
        return m_componentTypes;
    }

    /**
     * @brief Access the list of component type ids (const).
     * @return Const reference to the type id list.
     */
    const std::vector<TypeId>& ComponentTypes() const
    {
        return m_componentTypes;
    }

    /**
     * @brief Access attached component storages for this node.
     * @remarks
     * This is a hot-path cache used by tick traversal to avoid per-frame
     * type-id map lookups in world storage.
     */
    std::vector<ComponentStorageView*>& ComponentStorages()
    {
        return m_componentStorages;
    }

    /**
     * @brief Access attached component storages for this node (const).
     */
    const std::vector<ComponentStorageView*>& ComponentStorages() const
    {
        return m_componentStorages;
    }

    /**
     * @brief Get cached relevance component pointer for this node.
     * @return Relevance component pointer or nullptr.
     * @remarks
     * Populated by world bookkeeping when a RelevanceComponent is attached.
     * This cache avoids per-frame storage lookups in `IsNodeActive` hot paths.
     */
    RelevanceComponent* RelevanceState()
    {
        return m_relevanceComponent;
    }

    /**
     * @brief Get cached relevance component pointer for this node (const).
     * @return Relevance component pointer or nullptr.
     */
    const RelevanceComponent* RelevanceState() const
    {
        return m_relevanceComponent;
    }

    /**
     * @brief Set cached relevance component pointer for this node.
     * @param Relevance Relevance component pointer.
     * @remarks Updated by world component registration/unregistration paths.
     */
    void RelevanceState(RelevanceComponent* Relevance)
    {
        m_relevanceComponent = Relevance;
    }

    /**
     * @brief Access the component bitmask storage.
     * @return Mutable reference to the component mask.
     * @remarks Used for fast type queries.
     */
    std::vector<uint64_t>& ComponentMask()
    {
        return m_componentMask;
    }

    /**
     * @brief Access the component bitmask storage (const).
     * @return Const reference to the component mask.
     */
    const std::vector<uint64_t>& ComponentMask() const
    {
        return m_componentMask;
    }

    /**
     * @brief Get the component mask version.
     * @return Version id.
     * @remarks Used to resize masks when type registry grows.
     */
    uint32_t MaskVersion() const
    {
        return m_maskVersion;
    }

    /**
     * @brief Set the component mask version.
     * @param Version New version id.
     * @remarks Used alongside `ComponentTypeRegistry::Version()` to detect stale masks.
     */
    void MaskVersion(uint32_t Version)
    {
        m_maskVersion = Version;
    }

    /**
     * @brief Get the owning world for this node.
     * @return Pointer to the world interface or nullptr if unowned.
     */
    IWorld* World() const
    {
        return m_world;
    }

    /**
     * @brief Set the owning world for this node.
     * @param InWorld World interface pointer.
     * @remarks
     * Non-owning pointer propagated by world attachment. Null world is valid for
     * detached/prefab data.
     */
    void World(IWorld* InWorld)
    {
        m_world = InWorld;
    }

    /**
     * @brief Get cached world-runtime node handle for this node.
     * @return Runtime node handle.
     * @remarks
     * Populated by world runtime mirroring paths to avoid repeated UUID lookups
     * in hot transform/runtime queries.
     */
    RuntimeNodeHandle RuntimeNode() const
    {
        return m_runtimeNode;
    }

    /**
     * @brief Set cached world-runtime node handle for this node.
     * @param Handle Runtime node handle.
     */
    void RuntimeNode(const RuntimeNodeHandle Handle)
    {
        m_runtimeNode = Handle;
    }

    /**
     * @brief Add a world-owned runtime ECS component to this node.
     * @tparam T Runtime component type (`RuntimeTickType`).
     * @param args Constructor arguments for the runtime component.
     * @return Runtime typed handle or error.
     * @remarks
     * Uses the world-owned `WorldEcsRuntime` storage path and requires this node
     * to be mirrored into runtime hierarchy (world-bound node).
     */
    template<RuntimeTickType T, typename... Args>
    TExpected<TDenseRuntimeHandle<T>> AddRuntimeComponent(Args&&... args);

    /**
     * @brief Add a world-owned runtime ECS component with explicit UUID.
     * @tparam T Runtime component type (`RuntimeTickType`).
     * @param Id Explicit runtime component UUID.
     * @param args Constructor arguments for the runtime component.
     * @return Runtime typed handle or error.
     */
    template<RuntimeTickType T, typename... Args>
    TExpected<TDenseRuntimeHandle<T>> AddRuntimeComponentWithId(const Uuid& Id, Args&&... args);

    /**
     * @brief Borrow a world-owned runtime ECS component attached to this node.
     * @tparam T Runtime component type.
     * @return Mutable reference wrapper or error.
     */
    template<RuntimeTickType T>
    TExpectedRef<T> RuntimeComponent();

    /**
     * @brief Check whether this node has a world-owned runtime ECS component type.
     * @tparam T Runtime component type.
     * @return True when attached.
     */
    template<RuntimeTickType T>
    bool HasRuntimeComponent() const;

    /**
     * @brief Remove a world-owned runtime ECS component type from this node.
     * @tparam T Runtime component type.
     * @return Success or error.
     */
    template<RuntimeTickType T>
    Result RemoveRuntimeComponent();

    /**
     * @brief Add a component of type T to this node.
     * @tparam T Component type.
     * @param args Constructor arguments.
     * @return Reference wrapper or error.
     * @remarks
     * Delegates to world-owned storage. Fails when node is not bound to a world.
     * Reflection for `T` is ensured on first use before construction.
     */
    template<typename T, typename... Args>
    TExpectedRef<T> Add(Args&&... args);

    /**
     * @brief Get a component of type T from this node.
     * @tparam T Component type.
     * @return Reference wrapper or error.
     * @remarks Requires world ownership; returns `NotReady` when detached.
     */
    template<typename T>
    TExpectedRef<T> Component();

    /**
     * @brief Check if a component of type T exists on this node.
     * @tparam T Component type.
     * @return True if present.
     * @remarks Safe on detached nodes (returns false).
     */
    template<typename T>
    bool Has() const;

    /**
     * @brief Remove a component of type T from this node.
     * @tparam T Component type.
     * @remarks Removal is deferred until end-of-frame.
     */
    template<typename T>
    void Remove();

private:
    [[nodiscard]] RuntimeNodeHandle ResolveRuntimeNodeHandle() const;
    [[nodiscard]] RuntimeNodeHandle ResolveRuntimeNodeHandleAndCache();

    NodeHandle m_self{}; /**< @brief Stable runtime identity handle for this node. */
    NodeHandle m_parent{}; /**< @brief Parent identity; null indicates this node is a root in world hierarchy. */
    std::vector<NodeHandle> m_children{}; /**< @brief Ordered child identity list used for deterministic traversal. */
    std::vector<BaseNode*> m_childNodes{}; /**< @brief Child pointer cache aligned with `m_children` to reduce handle resolves. */
    std::string m_name{"Node"}; /**< @brief Human-readable/debug name (not required to be unique). */
    bool m_active = true; /**< @brief Local execution gate used by tree traversal. */
    bool m_replicated = false; /**< @brief Runtime replication gate for networking bridges. */
    bool m_pendingDestroy = false; /**< @brief True when this node has been scheduled for end-of-frame destruction. */
    bool m_editorTransient = false; /**< @brief True when this node is an editor-only transient helper and must not be persisted. */
    std::vector<TypeId> m_componentTypes{}; /**< @brief Attached component type ids for introspection and fast feature checks. */
    std::vector<ComponentStorageView*> m_componentStorages{}; /**< @brief Attached component storage cache aligned with m_componentTypes. */
    RelevanceComponent* m_relevanceComponent = nullptr; /**< @brief Cached relevance component pointer for hot-path activation checks. */
    std::vector<uint64_t> m_componentMask{}; /**< @brief Dense bitmask mirror of `m_componentTypes` for fast `Has<T>` checks. */
    uint32_t m_maskVersion = 0; /**< @brief Last component-type-registry version this mask was synchronized against. */
    IWorld* m_world = nullptr; /**< @brief Non-owning pointer to world context for subsystem access and role queries. */
    RuntimeNodeHandle m_runtimeNode{}; /**< @brief Cached world-runtime handle for fast runtime hierarchy access. */
    TypeId m_typeId{}; /**< @brief Reflected type identity used by serialization/rpc/replication metadata lookups. */
};

static_assert(NodeContractConcept<BaseNode>);
static_assert(!std::is_polymorphic_v<BaseNode>);

} // namespace SnAPI::GameFramework
