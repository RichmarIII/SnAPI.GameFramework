#pragma once

/**
 * @file ReflectionAnnotations.h
 * @ingroup SnAPI_GameFramework
 * @brief Standalone source-marker macros consumed by the reflection generator.
 *
 * These macros intentionally expand to nothing in normal builds. The reflection generator reads the
 * raw source text, parses these markers, and then binds each marker to the next declaration of the
 * matching kind using libclang source locations.
 *
 * Placement rules:
 * - put `SnType(...)` immediately above the class/struct/enum it describes
 * - put `SnField(...)` immediately above the field it describes
 * - put `SnFunction(...)` immediately above the member function it describes
 * - put `SnEnumValue(...)` immediately above the enum entry it describes
 *
 * Example:
 * @code
 * SnType(
 *     SnName("Player Start"),
 *     SnCategory("Gameplay|Spawning")
 * )
 * struct PlayerStart
 * {
 *     SnField(
 *         SnName("Position"),
 *         SnCategory("Gameplay|Spawning"),
 *         SnRep(SnReliable),
 *         SnSerialized,
 *         SnValue(SnMin(-1000.0), SnMax(1000.0), SnStep(1.0))
 *     )
 *     Vec3 Position{};
 *
 *     SnFunction(
 *         SnName("Activate"),
 *         SnCategory("Gameplay|Actions"),
 *         SnRpc(SnReliable, SnServer)
 *     )
 *     void Activate();
 * };
 * @endcode
 */

/**
 * @def SnType
 * @brief Mark the next class, struct, or enum for reflection generation.
 * @param ... Metadata option macros such as `SnName(...)` or `SnCategory(...)`.
 *
 * Supported options:
 * - `SnName("UI Label")`
 * - `SnCategory("Gameplay|Actors")`
 * - `SnInterface`
 */
#define SnType(...)

/**
 * @def SnField
 * @brief Mark the next public non-static data member for reflection generation.
 * @param ... Metadata option macros such as `SnRep(...)`, `SnSerialized`, or `SnValue(...)`.
 *
 * Supported options:
 * - `SnName("UI Label")`
 * - `SnCategory("Config|Movement")`
 * - `SnReplicated`
 * - `SnRep(SnReliable)` / `SnRep(SnUnreliable)`
 * - `SnSerialized`
 * - `SnValue(SnMin(...), SnMax(...), SnStep(...))`
 */
#define SnField(...)

/**
 * @def SnFunction
 * @brief Mark the next public non-static member function for reflection generation.
 * @param ... Metadata option macros such as `SnRpc(...)`.
 *
 * Supported options:
 * - `SnName("UI Label")`
 * - `SnCategory("Gameplay|RPC")`
 * - `SnEditorAction`
 * - `SnRpc(SnReliable, SnServer)`
 * - `SnRpc(SnUnreliable, SnClient)`
 * - `SnRpc(SnReliable, SnMulticast)`
 */
#define SnFunction(...)

/**
 * @def SnEnumValue
 * @brief Mark the next enum entry with extra reflection metadata.
 * @param ... Metadata option macros such as `SnName(...)`.
 *
 * Supported options:
 * - `SnName("UI Label")`
 */
#define SnEnumValue(...)

/**
 * @def SnName
 * @brief Assign a display name to reflected metadata.
 * @param Value String literal display label.
 */
#define SnName(Value)

/**
 * @def SnCategory
 * @brief Assign a category/path string to reflected metadata.
 * @param Value String literal category path.
 */
#define SnCategory(Value)

/**
 * @def SnInterface
 * @brief Mark a reflected class/struct as an interface contract.
 */
#define SnInterface

/**
 * @def SnReplicated
 * @brief Mark a reflected field as replicated using the default replication mode.
 */
#define SnReplicated

/**
 * @def SnRep
 * @brief Mark a reflected field as replicated with an explicit replication mode.
 * @param ... `SnReliable` or `SnUnreliable`.
 */
#define SnRep(...)

/**
 * @def SnSerialized
 * @brief Mark a reflected field as serialized.
 */
#define SnSerialized

/**
 * @def SnEditorAction
 * @brief Expose a reflected method as an editor action.
 */
#define SnEditorAction

/**
 * @def SnRpc
 * @brief Attach RPC transport metadata to a reflected method.
 * @param ... Any combination of one reliability token and one target token.
 *
 * Supported tokens:
 * - `SnReliable`
 * - `SnUnreliable`
 * - `SnServer`
 * - `SnClient`
 * - `SnMulticast`
 */
#define SnRpc(...)

/**
 * @def SnReliable
 * @brief Reliable transport token used by `SnRpc(...)` and `SnRep(...)`.
 */
#define SnReliable

/**
 * @def SnUnreliable
 * @brief Unreliable transport token used by `SnRpc(...)` and `SnRep(...)`.
 */
#define SnUnreliable

/**
 * @def SnServer
 * @brief Server-target token used by `SnRpc(...)`.
 */
#define SnServer

/**
 * @def SnClient
 * @brief Client-target token used by `SnRpc(...)`.
 */
#define SnClient

/**
 * @def SnMulticast
 * @brief Multicast-target token used by `SnRpc(...)`.
 */
#define SnMulticast

/**
 * @def SnValue
 * @brief Attach numeric editor-range metadata to a reflected field.
 * @param ... Any combination of `SnMin(...)`, `SnMax(...)`, and `SnStep(...)`.
 */
#define SnValue(...)

/**
 * @def SnMin
 * @brief Minimum numeric editor value used inside `SnValue(...)`.
 * @param Value Numeric literal.
 */
#define SnMin(Value)

/**
 * @def SnMax
 * @brief Maximum numeric editor value used inside `SnValue(...)`.
 * @param Value Numeric literal.
 */
#define SnMax(Value)

/**
 * @def SnStep
 * @brief Numeric editor step value used inside `SnValue(...)`.
 * @param Value Numeric literal.
 */
#define SnStep(Value)

/* Backward-compatible aliases for the previous annotation-style surface. */
#define SNAPI_TYPE(...) SnType(__VA_ARGS__)
#define SNAPI_FIELD(...) SnField(__VA_ARGS__)
#define SNAPI_FUNCTION(...) SnFunction(__VA_ARGS__)
#define SNAPI_ENUM_VALUE(...) SnEnumValue(__VA_ARGS__)
#define SNAPI_DISPLAY_NAME(Value) SnName(Value)
#define SNAPI_CATEGORY(Value) SnCategory(Value)
#define SNAPI_INTERFACE SnInterface
#define SNAPI_REPLICATED SnReplicated
#define SNAPI_SERIALIZED SnSerialized
#define SNAPI_EDITOR_ACTION SnEditorAction
#define SNAPI_RPC_RELIABLE SnRpc(SnReliable)
#define SNAPI_RPC_UNRELIABLE SnRpc(SnUnreliable)
#define SNAPI_NET_SERVER SnRpc(SnServer)
#define SNAPI_NET_CLIENT SnRpc(SnClient)
#define SNAPI_NET_MULTICAST SnRpc(SnMulticast)
