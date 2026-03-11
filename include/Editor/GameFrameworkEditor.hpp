#pragma once

/**
 * @defgroup SnAPI_GameFramework_Editor SnAPI.GameFramework.Editor
 * @ingroup SnAPI_GameFramework
 * @brief Editor-facing APIs for tools, authoring workflows, and editor-only runtime helpers.
 *
 * The editor layer builds on top of the core GameFramework runtime rather than replacing it.
 * These headers expose the contracts needed to:
 * - host an editor-flavored `GameRuntime`
 * - register and order editor services
 * - bind editor viewports and camera controls
 * - manage editor selection, scene bootstrapping, and asset workflows
 *
 * Unless documented otherwise, editor APIs follow the same world-lifetime rules as the
 * underlying runtime and are intended for main-thread use.
 */

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Umbrella header for the public editor module.
 *
 * Include this header when a tool or application wants the common editor-facing surface
 * without manually tracking individual editor headers. It intentionally re-exports the
 * most commonly used service, world, selection, and bootstrap types.
 */
#include "Editor/EditorCameraComponent.h"
#include "Editor/EditorAssetService.h"
#include "Editor/EditorCoreServices.h"
#include "Editor/EditorSelectionModel.h"
#include "Editor/EditorWorld.h"
#include "Editor/GameEditor.h"
#include "Editor/IEditorService.h"
#include "Conduit/Editor.h"
