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
 * @brief Compile-time contract for node types.
 * @remarks
 * The contract captures the required node API surface independent of inheritance.
 * BaseNode provides the canonical implementation.
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
 * @brief Compile-time contract for component types.
 * @remarks
 * The contract captures required component API independent of inheritance.
 */
template<typename TComponent>
concept ComponentContractConcept =
    requires(TComponent& Component,
             const TComponent& ConstComponent,
             NodeHandle OwnerValue,
             Uuid IdValue,
             TypeId TypeValue,
             bool BoolValue,
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
};

template<typename TObject>
concept HasPreTickPhase =
    requires(TObject& Object, float DeltaSeconds) {
        { Object.PreTick(DeltaSeconds) } -> std::same_as<void>;
    };

template<typename TObject>
concept HasTickPhase =
    requires(TObject& Object, float DeltaSeconds) {
        { Object.Tick(DeltaSeconds) } -> std::same_as<void>;
    };

template<typename TObject>
concept HasFixedTickPhase =
    requires(TObject& Object, float DeltaSeconds) {
        { Object.FixedTick(DeltaSeconds) } -> std::same_as<void>;
    };

template<typename TObject>
concept HasLateTickPhase =
    requires(TObject& Object, float DeltaSeconds) {
        { Object.LateTick(DeltaSeconds) } -> std::same_as<void>;
    };

template<typename TObject>
concept HasPostTickPhase =
    requires(TObject& Object, float DeltaSeconds) {
        { Object.PostTick(DeltaSeconds) } -> std::same_as<void>;
    };

template<typename TObject>
concept OptionalTickContractConcept =
    HasPreTickPhase<TObject> ||
    HasTickPhase<TObject> ||
    HasFixedTickPhase<TObject> ||
    HasLateTickPhase<TObject> ||
    HasPostTickPhase<TObject>;

} // namespace SnAPI::GameFramework
