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
#include "ReflectionAnnotations.h"
#include "StaticTypeId.h"
#include "Uuid.h"
#include "WorldEcsRuntime.h"

namespace SnAPI::GameFramework
{

class IWorld;
class BaseComponent;
class RelevanceComponent;
class Variant;
template<typename TBase>
class TSubClassOf;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Canonical base type for world-owned scene graph nodes.
 *
 * `BaseNode` is the user-visible object that represents one graph element in a `World`.
 * It carries the durable identity, hierarchy state, reflected type information, and the
 * convenience API used to query or attach runtime components. Most gameplay-facing node
 * types should derive from `BaseNode` rather than inventing a parallel ownership model.
 *
 * Why this type exists:
 * - it gives gameplay code one canonical node object API while the world owns storage
 * - it keeps hierarchy, identity, and reflected type metadata in one place
 * - it keeps generic `BaseNode` instances on the same dense runtime contract as concrete node types
 *
 * Ownership and lifetime:
 * - `IWorld` owns node lifetime and backing storage.
 * - `BaseNode` never owns its parent, children, world, or attached components directly.
 * - `Handle()` is the canonical public identity. Raw pointers obtained through handles or iteration are borrowed views.
 * - `OnCreate()` delivery may be deferred by the world during bootstrap until dependent subsystems are ready.
 *
 * Threading model:
 * - Main-thread only for hierarchy mutation, component mutation, and most direct node access.
 * - Read-only access from other threads is not guaranteed safe unless external synchronization is provided.
 *
 * Invariants:
 * - `TypeKey()` must match the concrete reflected node type.
 * - `Handle().Id` is the stable node identity used for serialization, replication, and registry lookup.
 * - `World()` is non-owning and may be null only when the node is detached from a live world.
 *
 * Performance notes:
 * - Child lists and component masks are stored directly on the node for hot-path traversal.
 * - Use handles across frames; borrowed pointers should be treated as temporary frame-local views.
 *
 * @see IWorld
 * @see World
 * @see BaseComponent
 * @see WorldEcsRuntime
 */
SnType()
class BaseNode : public NodeCRTP<BaseNode>
{
public:
    /** @brief Stable type name used for reflection. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::BaseNode";

    /**
     * @brief Construct a node with default name.
     * @remarks Constructors must stay side-effect free; world/backend setup belongs in `OnCreate()`.
     */
    BaseNode()
        : m_typeId(StaticTypeId<BaseNode>())
    {
    }
    /**
     * @brief Construct a node with a custom name.
     * @param InName Node name.
     * @remarks Constructors must stay side-effect free; world/backend setup belongs in `OnCreate()`.
     */
    explicit BaseNode(std::string InName)
        : m_name(std::move(InName))
        , m_typeId(StaticTypeId<BaseNode>())
    {
    }

    BaseNode(const BaseNode&) = delete;
    BaseNode& operator=(const BaseNode&) = delete;
    BaseNode(BaseNode&&) noexcept = default;
    BaseNode& operator=(BaseNode&&) noexcept = default;
    /** @brief Default destructor. Runtime/backend teardown belongs in `OnDestroy()`, not here. */
    ~BaseNode() = default;

    /**
     * @brief Node construction lifecycle hook.
     *
     * Override in derived types to perform work that requires the node to already be
     * registered with its world and fully assigned an identity. Worlds may defer this
     * callback during bootstrap so render/UI-dependent nodes do not run before subsystems are ready.
     *
     * Threading:
     * - Main-thread only.
     */
    void OnCreate() {}
    /**
     * @brief Node destruction lifecycle hook.
     *
     * Called before the node is finally removed from world-owned storage. Use this to release
     * world-facing runtime state that should be torn down while the world and its subsystems are still valid.
     *
     * Threading:
     * - Main-thread only.
     */
    void OnDestroy() {}
    /** @brief Early variable-step update hook executed before `Tick`. @param DeltaSeconds Frame delta time in seconds. */
    void PreTick(float DeltaSeconds) { (void)DeltaSeconds; }
    /** @brief Primary variable-step update hook. @param DeltaSeconds Frame delta time in seconds. */
    void Tick(float DeltaSeconds) { (void)DeltaSeconds; }
    /** @brief Fixed-step update hook used for deterministic simulation. @param DeltaSeconds Fixed simulation step in seconds. */
    void FixedTick(float DeltaSeconds) { (void)DeltaSeconds; }
    /** @brief Late variable-step hook executed after `Tick`. @param DeltaSeconds Frame delta time in seconds. */
    void LateTick(float DeltaSeconds) { (void)DeltaSeconds; }
    /** @brief Post-update hook executed after the regular variable-step phases. @param DeltaSeconds Frame delta time in seconds. */
    void PostTick(float DeltaSeconds) { (void)DeltaSeconds; }
#if defined(WITH_EDITOR) && WITH_EDITOR
    /** @brief Editor-only update hook used when the world executes in editor mode. @param DeltaSeconds Frame delta time in seconds. */
    void EditorTick(float DeltaSeconds) { (void)DeltaSeconds; }
    /** @brief Editor-only notification fired after a reflected property changes. @param Name Reflected property name. */
    void EditorOnPropertyChanged(std::string_view Name) { (void)Name; }
#endif
    /** @brief End-of-frame hook executed during `World::EndFrame` when enabled by the world execution profile. */
    void EndFrame() {}

    /**
     * @brief Get the node name.
     * @return Name string.
     */
    SnField(SnKey("Name"), SnSetter(Name))
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
    SnFunction(SnKey("Handle"))
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
    SnFunction(SnKey("Id"))
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
    SnFunction(SnKey("TypeKey"))
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
    SnFunction(SnKey("Parent"))
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
    SnFunction()
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
    SnFunction()
    void AddChild(const NodeHandle& Child)
    {
        m_children.push_back(Child);
    }

    /**
     * @brief Remove a child handle from the node.
     * @param Child Child handle to remove.
     * @remarks
     * Performs first-match erase. If duplicate child handles were inserted, later duplicates
     * remain until explicitly removed.
     */
    SnFunction()
    void RemoveChild(const NodeHandle& Child)
    {
        for (size_t Index = 0; Index < m_children.size(); ++Index)
        {
            if (m_children[Index] == Child)
            {
                auto ChildIt = m_children.begin() + static_cast<std::vector<NodeHandle>::difference_type>(Index);
                m_children.erase(ChildIt);
                return;
            }
        }
    }

    /**
     * @brief Check if the node is active.
     * @return True if active.
     * @remarks Inactive nodes are skipped during tick.
     */
    SnFunction(SnKey("Active"))
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
    SnFunction(SnKey("SetActive"))
    void Active(bool Active)
    {
        m_active = Active;
    }

    /**
     * @brief Check if the node is replicated over the network.
     * @return True if replicated.
     */
    SnFunction(SnKey("Replicated"))
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
    SnFunction(SnKey("SetReplicated"))
    void Replicated(bool Replicated)
    {
        m_replicated = Replicated;
    }

    /**
     * @brief Get the owning network-connection id for this node.
     * @return Stable connection id, or `0` for local authority / unowned nodes.
     *
     * This is gameplay/runtime ownership metadata rather than transport state. It is used by
     * generated owner-targeted client RPC routing to resolve which remote connection should
     * receive `SnRpc(SnClient)` calls for this node.
     */
    SnField(SnKey("OwnerConnectionId"), SnConstGetter(GetOwnerConnectionId))
    std::uint64_t& EditOwnerConnectionId()
    {
        return m_ownerConnectionId;
    }

    const std::uint64_t& GetOwnerConnectionId() const
    {
        return m_ownerConnectionId;
    }

    /**
     * @brief Set the owning network-connection id for this node.
     * @param OwnerConnectionId Stable owning connection id, or `0` for local authority.
     */
    void SetOwnerConnectionId(const std::uint64_t OwnerConnectionId)
    {
        m_ownerConnectionId = OwnerConnectionId;
    }

    /**
     * @brief Check whether this node is queued for deferred destruction.
     * @return True when destruction has been scheduled but not yet flushed.
     * @remarks
     * Used by hot-path activity checks to avoid UUID set lookups while preserving
     * end-of-frame deferred destruction semantics.
     */
    SnFunction(SnKey("PendingDestroy"))
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
    SnFunction(SnKey("EditorTransient"))
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
    SnFunction(SnKey("SetEditorTransient"))
    void EditorTransient(const bool Transient)
    {
        m_editorTransient = Transient;
    }

    /**
     * @brief True when this node executes with server authority.
     * @remarks Derived from world networking role; false when unbound to a world/session.
     */
    SnFunction(SnKey("IsServer"))
    bool IsServer() const;
    /**
     * @brief True when this node executes in client context.
     * @remarks Derived from world networking role; false when unbound to a world/session.
     */
    SnFunction(SnKey("IsClient"))
    bool IsClient() const;
    /**
     * @brief True when this node executes as listen-server.
     * @remarks True when both server and client roles are active in the attached session.
     */
    SnFunction(SnKey("IsListenServer"))
    bool IsListenServer() const;

    /**
     * @brief Possession callback invoked when a LocalPlayer begins possessing this node.
     * @param PlayerHandle Handle of the possessing LocalPlayer.
     * @remarks Default implementation is a no-op.
     */
    SnFunction(SnKey("OnPossess"))
    void OnPossess(const NodeHandle& PlayerHandle)
    {
        (void)PlayerHandle;
    }

    /**
     * @brief Possession callback invoked when a LocalPlayer stops possessing this node.
     * @param PlayerHandle Handle of the unpossessing LocalPlayer.
     * @remarks Default implementation is a no-op.
     */
    SnFunction(SnKey("OnUnpossess"))
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
    SnFunction(SnKey("World"))
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
     * @brief Get a component handle by reflected component type.
     * @param Type Runtime component type id.
     * @return Stable component handle or null handle when absent.
     * @remarks Safe on detached nodes; returns a null handle when this node is not world-bound.
     */
    SnFunction()
    [[nodiscard]] ComponentHandle Component(const TSubClassOf<BaseComponent>& Type) const;

    /**
     * @brief Check if a component of type T exists on this node.
     * @tparam T Component type.
     * @return True if present.
     * @remarks Safe on detached nodes (returns false).
     */
    template<typename T>
    bool Has() const;

    /**
     * @brief Check if a component of reflected type exists on this node.
     * @param Type Runtime component type id.
     * @return True if present.
     */
    SnFunction()
    [[nodiscard]] bool Has(const TSubClassOf<BaseComponent>& Type) const;

    /**
     * @brief Remove a component of type T from this node.
     * @tparam T Component type.
     * @remarks Removal is deferred until end-of-frame.
     */
    template<typename T>
    void Remove();

private:
    void RefreshComponentMaskCache();
    [[nodiscard]] bool HasComponentBit(const TypeId& Type) const;

    NodeHandle m_self{}; /**< @brief Stable runtime identity handle for this node. */
    NodeHandle m_parent{}; /**< @brief Parent identity; null indicates this node is a root in world hierarchy. */
    std::vector<NodeHandle> m_children{}; /**< @brief Ordered child identity list used for deterministic traversal. */
    std::string m_name{"Node"}; /**< @brief Human-readable/debug name (not required to be unique). */
    bool m_active = true; /**< @brief Local execution gate used by tree traversal. */
    bool m_replicated = false; /**< @brief Runtime replication gate for networking bridges. */
    bool m_pendingDestroy = false; /**< @brief True when this node has been scheduled for end-of-frame destruction. */
    bool m_editorTransient = false; /**< @brief True when this node is an editor-only transient helper and must not be persisted. */
    std::uint64_t m_ownerConnectionId = 0; /**< @brief Stable owning-connection id used for owner-targeted client RPC routing. */
    std::vector<TypeId> m_componentTypes{}; /**< @brief Attached component type ids for introspection and fast feature checks. */
    RelevanceComponent* m_relevanceComponent = nullptr; /**< @brief Cached relevance component pointer for hot-path activation checks. */
    std::vector<uint64_t> m_componentMask{}; /**< @brief Dense bitmask mirror of `m_componentTypes` for fast `Has<T>` checks. */
    uint32_t m_maskVersion = 0; /**< @brief Last component-type-registry version this mask was synchronized against. */
    IWorld* m_world = nullptr; /**< @brief Non-owning pointer to world context for subsystem access and role queries. */
    TypeId m_typeId{}; /**< @brief Reflected type identity used by serialization/rpc/replication metadata lookups. */
};

static_assert(NodeContractConcept<BaseNode>);
static_assert(!std::is_polymorphic_v<BaseNode>);

} // namespace SnAPI::GameFramework
