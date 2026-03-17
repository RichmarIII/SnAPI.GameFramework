#include "Editor/EditorCameraComponent.h"
#include "Editor/EditorImportSettings.h"

#include "TypeAutoRegistration.h"
#include "TypeRegistry.h"

#if defined(SNAPI_GF_ENABLE_INPUT) && defined(SNAPI_GF_ENABLE_RENDERER)

namespace SnAPI::GameFramework
{

namespace
{
void RegisterEditorEnum(
    const char* Name,
    const size_t Size,
    const size_t Align,
    const bool IsSigned,
    const std::initializer_list<EnumValueInfo> Values)
{
    TypeInfo Info{};
    Info.Id = TypeIdFromName(Name);
    Info.Name = Name;
    Info.Size = Size;
    Info.Align = Align;
    Info.IsEnum = true;
    Info.EnumIsSigned = IsSigned;
    Info.EnumValues.assign(Values.begin(), Values.end());
    (void)TypeRegistry::Instance().Register(std::move(Info));
}

[[maybe_unused]] const bool g_editorEnumsRegistered = []() {
    

    
    return true;
}();
} // namespace

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_INPUT && SNAPI_GF_ENABLE_RENDERER
