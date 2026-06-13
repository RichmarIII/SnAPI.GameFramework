#pragma once

/**
 * @file Conduit.h
 * @ingroup SnAPI_GameFramework
 * @brief Umbrella include and Doxygen module entry point for the Conduit visual scripting runtime.
 *
 * @defgroup SnAPI_GameFramework_Conduit Conduit
 * @ingroup SnAPI_GameFramework
 * @brief Reflection-driven visual scripting runtime, binding, and execution primitives.
 *
 * Conduit is the visual scripting subsystem for `SnAPI.GameFramework`.
 * It is designed around a split between:
 * - authored graph data
 * - bound runtime graphs
 * - frame-local slot storage
 * - reflection-backed field/method invocation
 *
 * Runtime philosophy:
 * - keep steady-state execution free of per-node reflection lookup
 * - compile graphs into slot-based execution plans
 * - use handles for durable instance references
 * - reserve `Variant` for boundaries, constants, and tooling rather than every hop
 *
 * Public surface overview:
 * - `Types.h` defines slot ids, labels, node kinds, and execution context
 * - `Value.h` defines durable serialized value payloads used by authored assets and constants
 * - `Resolvers.h` defines handle-family resolution registration
 * - `Frame.h` defines owned frame storage for runtime slot values
 * - `Graph.h` defines compiled graph structures and runtime execute primitives
 * - `Compiler.h` defines `GraphBuilder`, the low-level binding/baking API
 * - `Asset.h` defines authored graph/class asset payloads and compile helpers
 * - `ClassComponent.h` defines the first live runtime host for class assets on world nodes
 *
 * Editor-facing Conduit authoring APIs live under `Conduit/Editor.h` and are re-exported by
 * the editor umbrella header rather than this runtime umbrella.
 *
 * The current public API is intentionally low-level. Higher-level authored graph assets
 * should eventually compile into these runtime primitives rather than bypass them.
 */
#include "Conduit/Types.h"
#include "Conduit/Value.h"
#include "Conduit/Resolvers.h"
#include "Conduit/Frame.h"
#include "Conduit/Graph.h"
#include "Conduit/Compiler.h"
#include "Conduit/Asset.h"
#include "Conduit/ClassComponent.h"
