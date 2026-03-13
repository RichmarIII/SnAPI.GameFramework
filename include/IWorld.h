#pragma once

#include <cstdint>
#include <string>
#include <type_traits>

#include "Expected.h"
#include "Handles.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief High-level world role used by runtime, editor, and Play-In-Editor flows.
 * @remarks
 * The world kind is an execution-context label, not an ownership boundary. The same concrete
 * `World` implementation can run with different policies depending on whether it is being used
 * for gameplay, tool-time editing, or a PIE session.
 */
enum class EWorldKind : std::uint8_t
{
    Runtime, /**< @brief Normal gameplay/runtime execution context. */
    Editor,  /**< @brief Tool-time editor context where gameplay simulation may be disabled. */
    PIE      /**< @brief Play-In-Editor context using runtime-like simulation rules. */
};

class Level;
class BaseNode;
class BaseComponent;
class WorldEcsRuntime;
template<typename TObject>
struct TDenseRuntimeHandle;
struct RuntimeComponentRecord;
using RuntimeComponentHandle = TDenseRuntimeHandle<RuntimeComponentRecord>;
#if defined(SNAPI_GF_ENABLE_INPUT)
class InputSystem;
#endif
#if defined(SNAPI_GF_ENABLE_UI)
class UISystem;
#endif
#if defined(SNAPI_GF_ENABLE_AUDIO)
class AudioSystem;
#endif
#if defined(SNAPI_GF_ENABLE_NETWORKING)
class NetworkSystem;
#endif
#if defined(SNAPI_GF_ENABLE_PHYSICS)
class PhysicsSystem;
#endif
#if defined(SNAPI_GF_ENABLE_RENDERER)
class RendererSystem;
#endif
class ScriptRuntimeService;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Root world contract for graph ownership, subsystem access, and frame execution.
 *
 * `IWorld` is the central runtime abstraction that every higher-level gameplay or editor
 * system talks to. It owns the authoritative node/component graph, exposes the optional
 * subsystems bound into that graph, and defines the frame lifecycle used by `GameRuntime`
 * and editor tooling.
 *
 * Design intent:
 * - centralize object ownership so nodes and components have one authoritative lifetime
 * - separate public world semantics from the concrete `World` implementation
 * - let runtime, editor, and PIE share one API while using different execution profiles
 *
 * Ownership and lifetime:
 * - The world owns node storage, runtime node/component records, and subsystem instances.
 * - Pointers and references returned from world lookup APIs are borrowed views.
 * - Implementations may defer actual destruction until `EndFrame` to preserve frame-stable handles.
 *
 * Threading model:
 * - Unless a method explicitly says otherwise, graph mutation and direct node/component access are main-thread only.
 * - The interface itself is not generally thread-safe; external synchronization is required for concurrent use.
 *
 * Invariants:
 * - `NodeHandle` / `ComponentHandle` are the stable public identity boundary.
 * - Execution policy queries (`ShouldRunGameplay()`, `ShouldTickUI()`, and similar) describe what the world will do this frame.
 * - Subsystem accessors return live subsystem instances owned by the world.
 *
 * @see World
 * @see GameRuntime
 * @see BaseNode
 * @see BaseComponent
 */
class IWorld
{
public:
    /**
     * @brief Callback used when iterating concrete world-owned node objects.
     * @param UserData Opaque caller-owned context pointer.
     * @param Handle Stable handle for the visited node.
     * @param Node Borrowed reference to the visited node object.
     */
    using NodeVisitor = void(*)(void* UserData, const NodeHandle& Handle, BaseNode& Node);

    /** @brief Virtual destructor. */
    virtual ~IWorld() = default;

    /**
     * @brief World role classification.
     * @return Active world kind.
     */
    virtual EWorldKind Kind() const = 0;

    /**
     * @brief Whether high-level gameplay orchestration should run for this world.
     * @remarks
     * GameRuntime uses this to gate `GameplayHost::Tick` and world ECS runtime phases.
     */
    virtual bool ShouldRunGameplay() const = 0;
    /**
     * @brief Whether input pumping should run during variable tick.
     */
    virtual bool ShouldTickInput() const = 0;
    /**
     * @brief Whether UI context tick should run during variable tick.
     */
    virtual bool ShouldTickUI() const = 0;
    /**
     * @brief Whether networking queues/session pumps should run.
     */
    virtual bool ShouldPumpNetworking() const = 0;
    /**
     * @brief Whether physics simulation stepping should run.
     * @remarks
     * Physics queries can still be allowed independently via
     * `ShouldAllowPhysicsQueries()`.
     */
    virtual bool ShouldSimulatePhysics() const = 0;
    /**
     * @brief Whether physics query access should be considered valid.
     * @remarks
     * Editor worlds typically return true while `ShouldSimulatePhysics()` is false.
     */
    virtual bool ShouldAllowPhysicsQueries() const = 0;
    /**
     * @brief Whether audio subsystem update should run.
     */
    virtual bool ShouldTickAudio() const = 0;
    /**
     * @brief Whether node/component end-frame flush should run.
     */
    virtual bool ShouldRunNodeEndFrame() const = 0;
    /**
     * @brief Whether UI render packet generation/queueing should run.
     */
    virtual bool ShouldBuildUiRenderPackets() const = 0;
    /**
     * @brief Whether renderer end-frame submission should run.
     */
    virtual bool ShouldRenderFrame() const = 0;

    /**
     * @brief Iterate all world-owned nodes.
     * @param Visitor Callback invoked for each node.
     * @param UserData Opaque callback context pointer.
     */
    virtual void ForEachNode(NodeVisitor Visitor, void* UserData) = 0;

    template<typename Visitor>
    void ForEachNode(Visitor&& VisitorFn)
    {
        using TVisitor = std::remove_reference_t<Visitor>;
        TVisitor& BoundVisitor = VisitorFn;
        ForEachNode(
            [](void* UserData, const NodeHandle& Handle, BaseNode& Node) {
                (*static_cast<TVisitor*>(UserData))(Handle, Node);
            },
            &BoundVisitor);
    }
    /**
     * @brief Resolve node handle by UUID (slow path).
     * @param Id Node UUID.
     * @return Node handle or error.
     */
    virtual TExpected<NodeHandle> NodeHandleById(const Uuid& Id) const = 0;
    /**
     * @brief Create a node by reflected type.
     * @param Type Reflected node type id.
     * @param Name Node name.
     * @return Node handle or error.
     */
    virtual TExpected<NodeHandle> CreateNode(const TypeId& Type, std::string Name) = 0;
    /**
     * @brief Create a node by reflected type with explicit UUID.
     * @param Type Reflected node type id.
     * @param Name Node name.
     * @param Id Explicit node UUID.
     * @return Node handle or error.
     */
    virtual TExpected<NodeHandle> CreateNodeWithId(const TypeId& Type, std::string Name, const Uuid& Id) = 0;
    /**
     * @brief Borrow a node by handle, hydrating the handle's runtime key on success.
     * @param InOutHandle Node handle to resolve and refresh.
     * @return Borrowed node pointer or nullptr when missing.
     */
    virtual BaseNode* BorrowedNode(NodeHandle& InOutHandle) = 0;
    /**
     * @brief Borrow a node by handle, hydrating the handle's runtime key on success.
     * @param InOutHandle Node handle to resolve and refresh.
     * @return Borrowed node pointer or nullptr when missing.
     */
    virtual const BaseNode* BorrowedNode(NodeHandle& InOutHandle) const = 0;
    /**
     * @brief Destroy a node.
     * @param Handle Node handle.
     * @return Success or error.
     */
    virtual Result DestroyNode(NodeHandle& InOutHandle) = 0;
    /**
     * @brief Attach child under parent.
     * @param Parent Parent node handle.
     * @param Child Child node handle.
     * @return Success or error.
     */
    virtual Result AttachChild(NodeHandle& InOutParent, NodeHandle& InOutChild) = 0;
    /**
     * @brief Detach child from parent.
     * @param Child Child node handle.
     * @return Success or error.
     */
    virtual Result DetachChild(NodeHandle& InOutChild) = 0;
    /**
     * @brief Borrow component instance by owner/type.
     * @param Owner Owner node handle.
     * @param Type Component reflected type id.
     * @return Component pointer or nullptr.
     */
    virtual void* BorrowedComponent(NodeHandle& InOutOwner, const TypeId& Type) = 0;
    /**
     * @brief Borrow component instance by owner/type (const).
     * @param Owner Owner node handle.
     * @param Type Component reflected type id.
     * @return Component pointer or nullptr.
     */
    virtual const void* BorrowedComponent(NodeHandle& InOutOwner, const TypeId& Type) const = 0;
    /**
     * @brief Borrow a component by handle, hydrating the handle's runtime key on success.
     * @param InOutHandle Component handle to resolve and refresh.
     * @return Borrowed component pointer or nullptr when missing.
     */
    virtual BaseComponent* BorrowedComponent(ComponentHandle& InOutHandle) = 0;
    /**
     * @brief Borrow a component by handle, hydrating the handle's runtime key on success.
     * @param InOutHandle Component handle to resolve and refresh.
     * @return Borrowed component pointer or nullptr when missing.
     */
    virtual const BaseComponent* BorrowedComponent(ComponentHandle& InOutHandle) const = 0;
    /**
     * @brief Remove a component by owner/type.
     * @param Owner Owner node handle.
     * @param Type Component reflected type id.
     * @return Success or error.
     */
    virtual Result RemoveComponentByType(NodeHandle& InOutOwner, const TypeId& Type) = 0;
    /**
     * @brief Create a component by owner/type.
     * @param Owner Owner node handle.
     * @param Type Component reflected type id.
     * @return Raw component pointer or error.
     */
    virtual TExpected<void*> CreateComponent(NodeHandle& InOutOwner, const TypeId& Type) = 0;
    /**
     * @brief Create a component by owner/type with explicit UUID.
     * @param Owner Owner node handle.
     * @param Type Component reflected type id.
     * @param Id Explicit component UUID.
     * @return Raw component pointer or error.
     */
    virtual TExpected<void*> CreateComponentWithId(NodeHandle& InOutOwner, const TypeId& Type, const Uuid& Id) = 0;

    /**
     * @brief Request node `OnCreate` execution for a world-owned node.
     * @param Handle Target node handle.
     * @return Success or error.
     * @remarks
     * Worlds may defer invocation temporarily during bootstrap and flush it once
     * dependent subsystems are ready.
     */
    virtual Result RequestNodeOnCreate(NodeHandle& InOutHandle) = 0;
    /**
     * @brief Check whether node `OnCreate` invocations are currently deferred.
     * @return True when node create callbacks will be queued instead of invoked immediately.
     */
    virtual bool AreNodeOnCreateCallbacksDeferred() const = 0;

    /**
     * @brief Per-frame tick.
     * @param DeltaSeconds Time since last tick.
     */
    virtual void Tick(float DeltaSeconds) = 0;
    /**
     * @brief Fixed-step tick.
     * @param DeltaSeconds Fixed time step.
     */
    virtual void FixedTick(float DeltaSeconds) = 0;
    /**
     * @brief Late tick.
     * @param DeltaSeconds Time since last tick.
     */
    virtual void LateTick(float DeltaSeconds) = 0;
    /**
     * @brief End-of-frame processing.
     * @remarks Flushes deferred destruction queues and finalizes frame-consistent state transitions.
     */
    virtual void EndFrame() = 0;

    /**
     * @brief Report whether the runtime currently drives a fixed-step simulation loop.
     * @return True when fixed-step simulation is enabled for the current frame.
     * @remarks
     * Components that interpolate fixed-step results for rendering should check this
     * first. When false, interpolation alpha should be treated as 1.
     */
    virtual bool FixedTickEnabled() const = 0;

    /**
     * @brief Get active fixed-step delta seconds.
     * @return Fixed simulation step interval in seconds (0 when fixed tick is disabled).
     * @remarks
     * This value is provided for systems that need deterministic step size metadata.
     */
    virtual float FixedTickDeltaSeconds() const = 0;

    /**
     * @brief Get current render interpolation alpha between fixed simulation samples.
     * @return Alpha in range [0, 1].
     * @remarks
     * Convention:
     * - 0 means "at previous fixed sample"
     * - 1 means "at current fixed sample"
     * When fixed tick is disabled this returns 1.
     */
    virtual float FixedTickInterpolationAlpha() const = 0;

    /**
     * @brief Create a level as a child node.
     * @param Name Level name.
     * @return Handle to the created level or error.
     * @remarks New levels are world-owned and participate in world tick traversal.
     */
    virtual TExpected<NodeHandle> CreateLevel(std::string Name) = 0;
    /**
     * @brief Access a level by handle.
     * @param Handle Level handle.
     * @return Reference wrapper or error.
     * @remarks Returns typed level reference if handle resolves and is level-compatible.
     */
    virtual TExpectedRef<Level> LevelRef(NodeHandle& InOutHandle) = 0;

    /**
     * @brief Add a runtime component to a node by reflected type.
     * @param Owner Owner node handle.
     * @param Type Runtime component type id.
     * @return Runtime component handle or error.
     * @remarks
     * This path requires a pre-registered runtime storage for the type and a
     * default constructible runtime type.
     */
    virtual TExpected<RuntimeComponentHandle> AddRuntimeComponent(NodeHandle& InOutOwner, const TypeId& Type) = 0;
    /**
     * @brief Add a runtime component with explicit UUID identity.
     * @param Owner Owner node handle.
     * @param Type Runtime component type id.
     * @param Id Explicit runtime component UUID.
     * @return Runtime component handle or error.
     */
    virtual TExpected<RuntimeComponentHandle> AddRuntimeComponentWithId(NodeHandle& InOutOwner,
                                                                        const TypeId& Type,
                                                                        const Uuid& Id) = 0;
    /**
     * @brief Remove a runtime component from a node by type.
     * @param Owner Owner node handle.
     * @param Type Runtime component type id.
     * @return Success or error.
     */
    virtual Result RemoveRuntimeComponent(NodeHandle& InOutOwner, const TypeId& Type) = 0;
    /**
     * @brief Check if a node has a runtime component type attached.
     * @param Owner Owner node handle.
     * @param Type Runtime component type id.
     * @return True when attached.
     */
    virtual bool HasRuntimeComponent(NodeHandle& InOutOwner, const TypeId& Type) const = 0;
    /**
     * @brief Get runtime component handle attached to a node by type.
     * @param Owner Owner node handle.
     * @param Type Runtime component type id.
     * @return Runtime component handle or error.
     */
    virtual TExpected<RuntimeComponentHandle> RuntimeComponentByType(NodeHandle& InOutOwner,
                                                                     const TypeId& Type) const = 0;
    /**
     * @brief Resolve runtime component raw pointer from handle and type.
     * @param Handle Runtime component handle.
     * @param Type Runtime component type id.
     * @return Mutable raw pointer or nullptr.
     */
    virtual void* ResolveRuntimeComponentRaw(RuntimeComponentHandle Handle, const TypeId& Type) = 0;
    /**
     * @brief Resolve runtime component raw pointer from handle and type (const).
     * @param Handle Runtime component handle.
     * @param Type Runtime component type id.
     * @return Const raw pointer or nullptr.
     */
    virtual const void* ResolveRuntimeComponentRaw(RuntimeComponentHandle Handle, const TypeId& Type) const = 0;

    /**
     * @brief Access world-owned ECS typed storage runtime.
     * @return Mutable runtime storage registry.
     * @remarks
     * This is the centralized owner for next-generation node/component storage.
     */
    virtual WorldEcsRuntime& EcsRuntime() = 0;
    /**
     * @brief Access world-owned ECS typed storage runtime (const).
     * @return Const runtime storage registry.
     */
    virtual const WorldEcsRuntime& EcsRuntime() const = 0;

#if defined(SNAPI_GF_ENABLE_INPUT)
    /**
     * @brief Access the input subsystem for this world.
     * @return Reference to InputSystem.
     */
    virtual InputSystem& Input() = 0;
    /**
     * @brief Access the input subsystem for this world (const).
     * @return Const reference to InputSystem.
     */
    virtual const InputSystem& Input() const = 0;
#endif

#if defined(SNAPI_GF_ENABLE_UI)
    /**
     * @brief Access the UI subsystem for this world.
     * @return Reference to UISystem.
     */
    virtual UISystem& UI() = 0;
    /**
     * @brief Access the UI subsystem for this world (const).
     * @return Const reference to UISystem.
     */
    virtual const UISystem& UI() const = 0;
#endif

#if defined(SNAPI_GF_ENABLE_AUDIO)
    /**
     * @brief Access the audio system for this world.
     * @return Reference to AudioSystem.
     */
    virtual AudioSystem& Audio() = 0;
    /**
     * @brief Access the audio system for this world (const).
     * @return Const reference to AudioSystem.
     */
    virtual const AudioSystem& Audio() const = 0;
#endif

#if defined(SNAPI_GF_ENABLE_NETWORKING)
    /**
     * @brief Access the networking subsystem for this world.
     * @return Reference to NetworkSystem.
     * @remarks World networking owns session bridge wiring for replication/RPC.
     */
    virtual NetworkSystem& Networking() = 0;
    /**
     * @brief Access the networking subsystem for this world (const).
     * @return Const reference to NetworkSystem.
     */
    virtual const NetworkSystem& Networking() const = 0;
#endif

#if defined(SNAPI_GF_ENABLE_PHYSICS)
    /**
     * @brief Access the physics subsystem for this world.
     * @return Reference to PhysicsSystem.
     */
    virtual PhysicsSystem& Physics() = 0;
    /**
     * @brief Access the physics subsystem for this world (const).
     * @return Const reference to PhysicsSystem.
     */
    virtual const PhysicsSystem& Physics() const = 0;
#endif

#if defined(SNAPI_GF_ENABLE_RENDERER)
    /**
     * @brief Access the renderer subsystem for this world.
     * @return Reference to RendererSystem.
     */
    virtual RendererSystem& Renderer() = 0;
    /**
     * @brief Access the renderer subsystem for this world (const).
     * @return Const reference to RendererSystem.
     */
    virtual const RendererSystem& Renderer() const = 0;
#endif

    /**
     * @brief Access the scripting runtime service for this world.
     * @return Mutable scripting runtime service.
     */
    virtual ScriptRuntimeService& Scripts() = 0;
    /**
     * @brief Access the scripting runtime service for this world (const).
     * @return Const scripting runtime service.
     */
    virtual const ScriptRuntimeService& Scripts() const = 0;
};

} // namespace SnAPI::GameFramework
