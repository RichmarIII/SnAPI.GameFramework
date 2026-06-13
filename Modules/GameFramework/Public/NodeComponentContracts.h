#pragma once

#include <concepts>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Handles.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

class BaseNode;
class IWorld;
class Variant;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Compile-time concept describing the API surface expected of node-like types.
 *
 * `NodeContractConcept` is used by generic code that wants to interact with node objects through
 * structure and semantics rather than concrete inheritance. `BaseNode` provides the canonical
 * implementation, but any compatible type can satisfy this concept.
 *
 * Core semantics:
 * - The concept requires graph identity, parenting, activation, replication, world binding, RPC,
 *   and tick-phase APIs.
 * - Satisfaction is checked entirely at compile time.
 * - This concept verifies shape only; it does not prove the runtime semantics behind those methods.
 *
 * @see BaseNode
 * @see ComponentContractConcept
 */
template<typename TNode>
concept NodeContractConcept =
    requires(TNode& Node,
             const TNode& ConstNode,
             std::string Name,
             NodeHandle HandleValue,
             Uuid IdValue,
             TypeId TypeValue,
             bool BoolValue,
             float DeltaSeconds,
             uint32_t MaskVersionValue,
             IWorld* WorldValue,
             std::string_view MethodName,
             std::span<const Variant> SpanArgs,
             std::initializer_list<Variant> InitArgs)
{
    { ConstNode.Name() } -> std::same_as<const std::string&>;
    { Node.Name(std::move(Name)) } -> std::same_as<void>;

    { ConstNode.Handle() } -> std::same_as<NodeHandle>;
    { Node.Handle(HandleValue) } -> std::same_as<void>;

    { ConstNode.Id() } -> std::same_as<const Uuid&>;
    { Node.Id(std::move(IdValue)) } -> std::same_as<void>;

    { ConstNode.TypeKey() } -> std::same_as<const TypeId&>;
    { Node.TypeKey(TypeValue) } -> std::same_as<void>;

    { ConstNode.Parent() } -> std::same_as<NodeHandle>;
    { Node.Parent(HandleValue) } -> std::same_as<void>;
    { ConstNode.Children() } -> std::same_as<const std::vector<NodeHandle>&>;
    { Node.AddChild(HandleValue) } -> std::same_as<void>;
    { Node.RemoveChild(HandleValue) } -> std::same_as<void>;

    { ConstNode.Active() } -> std::same_as<bool>;
    { Node.Active(BoolValue) } -> std::same_as<void>;
    { ConstNode.Replicated() } -> std::same_as<bool>;
    { Node.Replicated(BoolValue) } -> std::same_as<void>;

    { ConstNode.IsServer() } -> std::same_as<bool>;
    { ConstNode.IsClient() } -> std::same_as<bool>;
    { ConstNode.IsListenServer() } -> std::same_as<bool>;

    { Node.OnPossess(HandleValue) } -> std::same_as<void>;
    { Node.OnUnpossess(HandleValue) } -> std::same_as<void>;
    { Node.OnCreate() } -> std::same_as<void>;
    { Node.OnDestroy() } -> std::same_as<void>;
    { Node.PreTick(DeltaSeconds) } -> std::same_as<void>;
    { Node.Tick(DeltaSeconds) } -> std::same_as<void>;
    { Node.FixedTick(DeltaSeconds) } -> std::same_as<void>;
    { Node.LateTick(DeltaSeconds) } -> std::same_as<void>;
    { Node.PostTick(DeltaSeconds) } -> std::same_as<void>;
#if defined(WITH_EDITOR) && WITH_EDITOR
    { Node.EditorTick(DeltaSeconds) } -> std::same_as<void>;
#endif

    { Node.CallRPC(MethodName, SpanArgs) } -> std::same_as<bool>;
    { Node.CallRPC(MethodName, InitArgs) } -> std::same_as<bool>;

    { Node.ComponentTypes() } -> std::same_as<std::vector<TypeId>&>;
    { ConstNode.ComponentTypes() } -> std::same_as<const std::vector<TypeId>&>;

    { Node.ComponentMask() } -> std::same_as<std::vector<uint64_t>&>;
    { ConstNode.ComponentMask() } -> std::same_as<const std::vector<uint64_t>&>;
    { ConstNode.MaskVersion() } -> std::same_as<uint32_t>;
    { Node.MaskVersion(MaskVersionValue) } -> std::same_as<void>;

    { ConstNode.World() } -> std::same_as<IWorld*>;
    { Node.World(WorldValue) } -> std::same_as<void>;
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Compile-time concept describing the API surface expected of component-like types.
 *
 * `ComponentContractConcept` captures the owner binding, activation, replication, identity, world
 * access, RPC, and tick-phase methods expected by generic component-aware code. `BaseComponent`
 * provides the canonical implementation.
 *
 * @note As with `NodeContractConcept`, this is a structural compile-time check only. It does not
 * guarantee any stronger runtime invariants than the presence of the required signatures.
 *
 * @see BaseComponent
 * @see NodeContractConcept
 */
template<typename TComponent>
concept ComponentContractConcept =
    requires(TComponent& Component,
             const TComponent& ConstComponent,
             NodeHandle OwnerValue,
             Uuid IdValue,
             TypeId TypeValue,
             bool BoolValue,
             float DeltaSeconds,
             std::string_view MethodName,
             std::span<const Variant> SpanArgs,
             std::initializer_list<Variant> InitArgs)
{
    { Component.Owner(OwnerValue) } -> std::same_as<void>;
    { ConstComponent.Owner() } -> std::same_as<NodeHandle>;

    { ConstComponent.Active() } -> std::same_as<bool>;
    { Component.Active(BoolValue) } -> std::same_as<void>;
    { ConstComponent.Replicated() } -> std::same_as<bool>;
    { Component.Replicated(BoolValue) } -> std::same_as<void>;

    { ConstComponent.Id() } -> std::same_as<const Uuid&>;
    { Component.Id(std::move(IdValue)) } -> std::same_as<void>;
    { ConstComponent.TypeKey() } -> std::same_as<const TypeId&>;
    { Component.TypeKey(TypeValue) } -> std::same_as<void>;
    { ConstComponent.Handle() } -> std::same_as<ComponentHandle>;

    { Component.OwnerNode() } -> std::same_as<BaseNode*>;
    { ConstComponent.World() } -> std::same_as<IWorld*>;
    { ConstComponent.IsServer() } -> std::same_as<bool>;
    { ConstComponent.IsClient() } -> std::same_as<bool>;
    { ConstComponent.IsListenServer() } -> std::same_as<bool>;

    { Component.CallRPC(MethodName, SpanArgs) } -> std::same_as<bool>;
    { Component.CallRPC(MethodName, InitArgs) } -> std::same_as<bool>;

    { Component.OnCreate() } -> std::same_as<void>;
    { Component.OnDestroy() } -> std::same_as<void>;
    { Component.PreTick(DeltaSeconds) } -> std::same_as<void>;
    { Component.Tick(DeltaSeconds) } -> std::same_as<void>;
    { Component.FixedTick(DeltaSeconds) } -> std::same_as<void>;
    { Component.LateTick(DeltaSeconds) } -> std::same_as<void>;
    { Component.PostTick(DeltaSeconds) } -> std::same_as<void>;
#if defined(WITH_EDITOR) && WITH_EDITOR
    { Component.EditorTick(DeltaSeconds) } -> std::same_as<void>;
#endif
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Compile-time probe for the presence of `PreTick(float)`.
 */
template<typename TObject>
concept HasPreTickPhase =
    requires(TObject& Object, float DeltaSeconds) {
        { Object.PreTick(DeltaSeconds) } -> std::same_as<void>;
    };

/**
 * @ingroup SnAPI_GameFramework
 * @brief Compile-time probe for the presence of `Tick(float)`.
 */
template<typename TObject>
concept HasTickPhase =
    requires(TObject& Object, float DeltaSeconds) {
        { Object.Tick(DeltaSeconds) } -> std::same_as<void>;
    };

/**
 * @ingroup SnAPI_GameFramework
 * @brief Compile-time probe for the presence of `FixedTick(float)`.
 */
template<typename TObject>
concept HasFixedTickPhase =
    requires(TObject& Object, float DeltaSeconds) {
        { Object.FixedTick(DeltaSeconds) } -> std::same_as<void>;
    };

/**
 * @ingroup SnAPI_GameFramework
 * @brief Compile-time probe for the presence of `LateTick(float)`.
 */
template<typename TObject>
concept HasLateTickPhase =
    requires(TObject& Object, float DeltaSeconds) {
        { Object.LateTick(DeltaSeconds) } -> std::same_as<void>;
    };

/**
 * @ingroup SnAPI_GameFramework
 * @brief Compile-time probe for the presence of `PostTick(float)`.
 */
template<typename TObject>
concept HasPostTickPhase =
    requires(TObject& Object, float DeltaSeconds) {
        { Object.PostTick(DeltaSeconds) } -> std::same_as<void>;
    };

#if defined(WITH_EDITOR) && WITH_EDITOR
/**
 * @ingroup SnAPI_GameFramework
 * @brief Compile-time probe for the presence of `EditorTick(float)`.
 */
template<typename TObject>
concept HasEditorTickPhase =
    requires(TObject& Object, float DeltaSeconds) {
        { Object.EditorTick(DeltaSeconds) } -> std::same_as<void>;
    };
#endif

/**
 * @ingroup SnAPI_GameFramework
 * @brief Compile-time concept satisfied by types exposing at least one recognized tick phase.
 *
 * This concept is useful for generic helpers that only care whether a type participates in any
 * engine tick phase at all, without requiring the full node/component contract.
 */
template<typename TObject>
concept OptionalTickContractConcept =
    HasPreTickPhase<TObject> ||
    HasTickPhase<TObject> ||
    HasFixedTickPhase<TObject> ||
    HasLateTickPhase<TObject> ||
    HasPostTickPhase<TObject>
#if defined(WITH_EDITOR) && WITH_EDITOR
    || HasEditorTickPhase<TObject>
#endif
    ;

} // namespace SnAPI::GameFramework
