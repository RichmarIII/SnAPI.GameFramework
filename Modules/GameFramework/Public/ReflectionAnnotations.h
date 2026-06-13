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
 *         SnDisplayName("Position"),
 *         SnCategory("Gameplay|Spawning"),
 *         SnRep(SnReliable),
 *         SnSerialized,
 *         SnAdvanced,
 *         SnValue(SnMin(-1000.0), SnMax(1000.0), SnStep(1.0))
 *     )
 *     Vec3 Position{};
 *
 *     SnFunction(
 *         SnDisplayName("Activate"),
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
 * @param ... Metadata option macros such as `SnDisplayName(...)` or `SnCategory(...)`.
 *
 * Supported options:
 * - `SnDisplayName("UI Label")`
 * - `SnCategory("Gameplay|Actors")`
 * - `SnInterface`
 * - `SnTemplate`
 */
#define SnType(...)

/**
 * @def SnGenerated
 * @brief Inject declarations emitted into the header's generated sidecar.
 *
 * `SnGenerated()` is a class-body marker, not a source-rewrite directive. Authored headers keep
 * the marker in place and include their matching `*.generated.hpp` file, which redefines this
 * macro to expand to line-keyed generated declarations such as:
 * - injected `kTypeName` members
 * - hidden RPC trampolines
 * - required `Impl()` declarations for generated RPC wrappers
 *
 * Usage:
 * - Include the generated header before the owning type declaration.
 * - Place `SnGenerated()` once inside the class/struct body, typically near the top.
 * - Types that use generator-owned RPC wrappers must declare `SnGenerated()`.
 *
 * When no generated sidecar is included, the default definition expands to nothing so normal
 * preprocessing remains valid.
 */
#define SnGenerated()

/**
 * @def SnField
 * @brief Mark the next field declaration or getter-like method for reflection generation.
 * @param ... Metadata option macros such as `SnRep(...)`, `SnSerialized`, or accessor overrides.
 *
 * Supported options:
 * - `SnKey("StableReflectedName")`
 * - `SnDisplayName("UI Label")`
 * - `SnCategory("Config|Movement")`
 * - `SnGetter(MethodName)`
 * - `SnConstGetter(MethodName)`
 * - `SnSetter(MethodName)`
 * - `SnReplicated`
 * - `SnRep(SnReliable)` / `SnRep(SnUnreliable)`
 * - `SnSerialized`
 * - `SnHidden`
 * - `SnReadOnly`
 * - `SnAdvanced`
 * - `SnHeavyData`
 * - `SnValue(SnMin(...), SnMax(...), SnStep(...))`
 *
 * Usage notes:
 * - On a public data member, `SnField(...)` with no accessor overrides reflects the member directly.
 * - On a private data member, pair it with `SnGetter(...)`, `SnConstGetter(...)`, and/or `SnSetter(...)`
 *   so generated code can expose the property through methods instead of direct member access.
 * - On a getter-like method, `SnField(...)` reflects that method as a read-only or accessor-backed field.
 */
#define SnField(...)

/**
 * @def SnFunction
 * @brief Mark the next public non-static member function for reflection generation.
 * @param ... Metadata option macros such as `SnRpc(...)`.
 *
 * Supported options:
 * - `SnKey("StableReflectedName")`
 * - `SnDisplayName("UI Label")`
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
 * @param ... Metadata option macros such as `SnDisplayName(...)`.
 *
 * Supported options:
 * - `SnKey("StableReflectedName")`
 * - `SnDisplayName("UI Label")`
 */
#define SnEnumValue(...)

/**
 * @def SnKey
 * @brief Override the stable reflected name/key used for lookup and serialization.
 * @param Value String literal stable key.
 */
#define SnKey(Value)

/**
 * @def SnDisplayName
 * @brief Assign a display name to reflected metadata.
 * @param Value String literal display label.
 */
#define SnDisplayName(Value)

/**
 * @def SnName
 * @brief Backward-compatible alias for `SnDisplayName`.
 * @param Value String literal display label.
 */
#define SnName(Value) SnDisplayName(Value)

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
 * @def SnTemplate
 * @brief Mark a reflected class template as a primary reflection template.
 *
 * `SnTemplate` is only valid inside `SnType(...)`. The generator will not try to
 * register the primary template itself. Instead, it uses the annotated primary as
 * a prototype and emits reflection for concrete specializations it discovers in the
 * scanned codebase.
 */
#define SnTemplate

/**
 * @def SnReplicated
 * @brief Mark a reflected field as replicated using the default replication mode.
 */
#define SnReplicated

/**
 * @def SnGetter
 * @brief Name the getter or editable-ref accessor used by `SnField(...)`.
 * @param Value Unqualified member-function identifier.
 */
#define SnGetter(Value)

/**
 * @def SnConstGetter
 * @brief Name the const getter paired with `SnGetter(...)` for `EditX()/GetX()`-style properties.
 * @param Value Unqualified member-function identifier.
 */
#define SnConstGetter(Value)

/**
 * @def SnSetter
 * @brief Name the setter paired with `SnGetter(...)` for accessor-backed properties.
 * @param Value Unqualified member-function identifier.
 */
#define SnSetter(Value)

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
 * @def SnHidden
 * @brief Hide a reflected field from generic editor/inspector surfaces.
 */
#define SnHidden

/**
 * @def SnReadOnly
 * @brief Force a reflected field to appear read-only in generic editor/inspector surfaces.
 */
#define SnReadOnly

/**
 * @def SnAdvanced
 * @brief Place a reflected field in an advanced/collapsed editor group when supported.
 */
#define SnAdvanced

/**
 * @def SnHeavyData
 * @brief Mark a reflected field as large or opaque data for generic editor/inspector surfaces.
 */
#define SnHeavyData

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
#define SNAPI_HIDDEN SnHidden
#define SNAPI_READ_ONLY SnReadOnly
#define SNAPI_ADVANCED SnAdvanced
#define SNAPI_HEAVY_DATA SnHeavyData
#define SNAPI_EDITOR_ACTION SnEditorAction
#define SNAPI_RPC_RELIABLE SnRpc(SnReliable)
#define SNAPI_RPC_UNRELIABLE SnRpc(SnUnreliable)
#define SNAPI_NET_SERVER SnRpc(SnServer)
#define SNAPI_NET_CLIENT SnRpc(SnClient)
#define SNAPI_NET_MULTICAST SnRpc(SnMulticast)
