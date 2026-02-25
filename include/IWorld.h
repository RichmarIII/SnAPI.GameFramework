#pragma once

#include <cstdint>
#include <string>

#include "Expected.h"
#include "Handles.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

/**
 * @brief High-level world role used by runtime/editor flows.
 */
enum class EWorldKind : std::uint8_t
{
    Runtime,
    Editor,
    PIE
};

class Level;
class BaseNode;
class WorldEcsRuntime;
template<typename TObject>
class TObjectPool;
template<typename TObject>
struct TDenseRuntimeHandle;
struct RuntimeNodeRecord;
using RuntimeNodeHandle = TDenseRuntimeHandle<RuntimeNodeRecord>;
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

/**
 * @brief Root runtime container contract for gameplay sessions.
 * @remarks
 * A world is the top-level execution root that owns levels and optional subsystem
 * integrations (input/ui/audio/networking/physics/renderer). Worlds drive frame
 * lifecycle (`Tick`/`EndFrame`) and establish authoritative context for
 * contained node graphs.
 */
class IWorld
{
public:
    using RuntimeChildVisitor = void(*)(void* UserData, RuntimeNodeHandle Child);
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
     * GameRuntime uses this to gate `GameplayHost::Tick`.
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
     * @brief Whether ECS runtime storage phases should run.
     */
    virtual bool ShouldTickEcsRuntime() const = 0;
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
     * @brief Access world-owned node pool storage.
     * @return Mutable node pool reference.
     * @remarks
     * World is the single owner of node object storage in the ECS-only model.
     */
    virtual TObjectPool<BaseNode>& NodePool() = 0;
    /**
     * @brief Access world-owned node pool storage (const).
     * @return Const node pool reference.
     */
    virtual const TObjectPool<BaseNode>& NodePool() const = 0;

    /**
     * @brief Iterate all world-owned nodes.
     * @param Visitor Callback invoked for each node.
     * @param UserData Opaque callback context pointer.
     */
    virtual void ForEachNode(NodeVisitor Visitor, void* UserData) = 0;
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
     * @brief Destroy a node.
     * @param Handle Node handle.
     * @return Success or error.
     */
    virtual Result DestroyNode(const NodeHandle& Handle) = 0;
    /**
     * @brief Attach child under parent.
     * @param Parent Parent node handle.
     * @param Child Child node handle.
     * @return Success or error.
     */
    virtual Result AttachChild(const NodeHandle& Parent, const NodeHandle& Child) = 0;
    /**
     * @brief Detach child from parent.
     * @param Child Child node handle.
     * @return Success or error.
     */
    virtual Result DetachChild(const NodeHandle& Child) = 0;
    /**
     * @brief Borrow component instance by owner/type.
     * @param Owner Owner node handle.
     * @param Type Component reflected type id.
     * @return Component pointer or nullptr.
     */
    virtual void* BorrowedComponent(const NodeHandle& Owner, const TypeId& Type) = 0;
    /**
     * @brief Borrow component instance by owner/type (const).
     * @param Owner Owner node handle.
     * @param Type Component reflected type id.
     * @return Component pointer or nullptr.
     */
    virtual const void* BorrowedComponent(const NodeHandle& Owner, const TypeId& Type) const = 0;
    /**
     * @brief Remove a component by owner/type.
     * @param Owner Owner node handle.
     * @param Type Component reflected type id.
     * @return Success or error.
     */
    virtual Result RemoveComponentByType(const NodeHandle& Owner, const TypeId& Type) = 0;
    /**
     * @brief Create a component by owner/type.
     * @param Owner Owner node handle.
     * @param Type Component reflected type id.
     * @return Raw component pointer or error.
     */
    virtual TExpected<void*> CreateComponent(const NodeHandle& Owner, const TypeId& Type) = 0;
    /**
     * @brief Create a component by owner/type with explicit UUID.
     * @param Owner Owner node handle.
     * @param Type Component reflected type id.
     * @param Id Explicit component UUID.
     * @return Raw component pointer or error.
     */
    virtual TExpected<void*> CreateComponentWithId(const NodeHandle& Owner, const TypeId& Type, const Uuid& Id) = 0;

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
    virtual TExpectedRef<Level> LevelRef(const NodeHandle& Handle) = 0;

    /**
     * @brief Create a world-owned runtime node record in ECS storage.
     * @param Name Node display/debug name.
     * @param Type Runtime type id.
     * @return Runtime node handle or error.
     */
    virtual TExpected<RuntimeNodeHandle> CreateRuntimeNode(std::string Name, const TypeId& Type) = 0;
    /**
     * @brief Create a world-owned runtime node record with explicit UUID.
     * @param Id Explicit node UUID.
     * @param Name Node display/debug name.
     * @param Type Runtime type id.
     * @return Runtime node handle or error.
     */
    virtual TExpected<RuntimeNodeHandle> CreateRuntimeNodeWithId(const Uuid& Id, std::string Name, const TypeId& Type) = 0;
    /**
     * @brief Destroy a runtime node (recursive for descendants).
     * @param Handle Runtime node handle.
     * @return Success or error.
     */
    virtual Result DestroyRuntimeNode(RuntimeNodeHandle Handle) = 0;
    /**
     * @brief Attach a runtime child node to a parent.
     * @param Parent Parent runtime node handle.
     * @param Child Child runtime node handle.
     * @return Success or error.
     */
    virtual Result AttachRuntimeChild(RuntimeNodeHandle Parent, RuntimeNodeHandle Child) = 0;
    /**
     * @brief Detach a runtime child node from its parent.
     * @param Child Child runtime node handle.
     * @return Success or error.
     */
    virtual Result DetachRuntimeChild(RuntimeNodeHandle Child) = 0;
    /**
     * @brief Resolve runtime node handle by UUID.
     * @param Id Runtime node UUID.
     * @return Runtime node handle or error.
     */
    virtual TExpected<RuntimeNodeHandle> RuntimeNodeById(const Uuid& Id) const = 0;
    /**
     * @brief Get runtime parent for a node.
     * @param Child Child runtime node handle.
     * @return Parent runtime node handle (null when root or invalid).
     */
    virtual RuntimeNodeHandle RuntimeParent(RuntimeNodeHandle Child) const = 0;
    /**
     * @brief Get runtime children for a node.
     * @param Parent Parent runtime node handle.
     * @return Child runtime handles.
     */
    virtual std::vector<RuntimeNodeHandle> RuntimeChildren(RuntimeNodeHandle Parent) const = 0;
    /**
     * @brief Iterate runtime children for a node without allocating snapshots.
     * @param Parent Parent runtime node handle.
     * @param Visitor Callback invoked for each alive child.
     * @param UserData Opaque callback context pointer.
     */
    virtual void ForEachRuntimeChild(RuntimeNodeHandle Parent, RuntimeChildVisitor Visitor, void* UserData) const = 0;
    /**
     * @brief Get runtime root nodes for the world.
     * @return Root runtime handles.
     */
    virtual std::vector<RuntimeNodeHandle> RuntimeRoots() const = 0;
    /**
     * @brief Add a runtime component to a runtime node by reflected type.
     * @param Owner Runtime owner node handle.
     * @param Type Runtime component type id.
     * @return Runtime component handle or error.
     * @remarks
     * This path requires a pre-registered runtime storage for the type and a
     * default constructible runtime type.
     */
    virtual TExpected<RuntimeComponentHandle> AddRuntimeComponent(RuntimeNodeHandle Owner, const TypeId& Type) = 0;
    /**
     * @brief Add a runtime component with explicit UUID identity.
     * @param Owner Runtime owner node handle.
     * @param Type Runtime component type id.
     * @param Id Explicit runtime component UUID.
     * @return Runtime component handle or error.
     */
    virtual TExpected<RuntimeComponentHandle> AddRuntimeComponentWithId(RuntimeNodeHandle Owner,
                                                                        const TypeId& Type,
                                                                        const Uuid& Id) = 0;
    /**
     * @brief Remove a runtime component from a runtime node by type.
     * @param Owner Runtime owner node handle.
     * @param Type Runtime component type id.
     * @return Success or error.
     */
    virtual Result RemoveRuntimeComponent(RuntimeNodeHandle Owner, const TypeId& Type) = 0;
    /**
     * @brief Check if runtime node has a runtime component type attached.
     * @param Owner Runtime owner node handle.
     * @param Type Runtime component type id.
     * @return True when attached.
     */
    virtual bool HasRuntimeComponent(RuntimeNodeHandle Owner, const TypeId& Type) const = 0;
    /**
     * @brief Get runtime component handle attached to runtime node by type.
     * @param Owner Runtime owner node handle.
     * @param Type Runtime component type id.
     * @return Runtime component handle or error.
     */
    virtual TExpected<RuntimeComponentHandle> RuntimeComponentByType(RuntimeNodeHandle Owner,
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
};

} // namespace SnAPI::GameFramework
