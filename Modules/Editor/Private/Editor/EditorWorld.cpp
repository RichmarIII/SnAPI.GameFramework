#include "Editor/EditorWorld.h"

#include <utility>

namespace SnAPI::GameFramework::Editor
{

EditorWorld::EditorWorld()
    : World("EditorWorld")
{
    ApplyEditorDefaults();
}

EditorWorld::EditorWorld(std::string Name)
    : World(std::move(Name))
{
    ApplyEditorDefaults();
}

void EditorWorld::ApplyEditorDefaults()
{
    SetWorldKind(EWorldKind::Editor);
    SetExecutionProfile(WorldExecutionProfile::Editor());
}

} // namespace SnAPI::GameFramework::Editor
