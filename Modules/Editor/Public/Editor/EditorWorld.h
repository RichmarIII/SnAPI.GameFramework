#pragma once

#include "Editor/EditorExport.h"
#include "World.h"

#include <string>

namespace SnAPI::GameFramework::Editor
{

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief `World` specialization configured for editor execution.
 *
 * `EditorWorld` exists so tools can opt into a predictable editor execution profile without
 * repeating the same world-kind and subsystem-policy setup at every call site.
 *
 * Core semantics:
 * - `EWorldKind` is forced to `Editor`.
 * - `WorldExecutionProfile::Editor()` is applied during construction.
 * - Gameplay orchestration, autonomous physics stepping, audio pumping, and networking
 *   simulation are disabled by default, while non-simulating queries remain available.
 *
 * Ownership and lifetime:
 * - Same as `World`; this type adds no extra ownership rules.
 *
 * Threading model:
 * - Follows `World`: main-thread mutation unless a narrower subsystem contract states otherwise.
 *
 * @see World
 * @see WorldExecutionProfile
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorWorld final : public World
{
public:
    /**
     * @brief Construct an editor world with the default name `"EditorWorld"`.
     */
    EditorWorld();
    /**
     * @brief Construct an editor world with a caller-provided name.
     * @param Name World name copied into the base `World`.
     */
    explicit EditorWorld(std::string Name);

private:
    void ApplyEditorDefaults();
};

} // namespace SnAPI::GameFramework::Editor
