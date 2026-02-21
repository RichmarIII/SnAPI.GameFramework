#pragma once

#include "Editor/EditorExport.h"
#include "World.h"

#include <string>

namespace SnAPI::GameFramework::Editor
{

/**
 * @brief Editor-specific world variant.
 * @remarks
 * Uses editor execution defaults:
 * - no gameplay host tick
 * - no physics simulation stepping (queries still allowed)
 * - no networking/audio pumps
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorWorld final : public World
{
public:
    EditorWorld();
    explicit EditorWorld(std::string Name);

private:
    void ApplyEditorDefaults();
};

} // namespace SnAPI::GameFramework::Editor

