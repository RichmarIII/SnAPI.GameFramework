#include "ColliderComponent.h"

#if defined(SNAPI_GF_ENABLE_PHYSICS)

#include "BaseNode.h"
#include "BaseNode.inl"
#include "RigidBodyComponent.h"

namespace SnAPI::GameFramework
{

namespace
{
#if defined(WITH_EDITOR) && WITH_EDITOR
bool IsColliderSettingsField(const std::string_view Name)
{
    return Name == "Settings"
        || Name == "Shape"
        || Name == "HalfExtent"
        || Name == "Radius"
        || Name == "HalfHeight"
        || Name == "LocalPosition"
        || Name == "LocalRotation"
        || Name == "Density"
        || Name == "Friction"
        || Name == "Restitution"
        || Name == "Layer"
        || Name == "Mask"
        || Name == "IsTrigger";
}
#endif
} // namespace

#if defined(WITH_EDITOR) && WITH_EDITOR
void ColliderComponent::EditorOnPropertyChanged(const std::string_view Name)
{
    if (!IsColliderSettingsField(Name))
    {
        return;
    }

    auto* Owner = OwnerNode();
    if (!Owner)
    {
        return;
    }

    if (auto BodyResult = Owner->Component<RigidBodyComponent>())
    {
        BodyResult->EditSettings();
        (void)BodyResult->RecreateBody();
    }
}
#endif

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_PHYSICS
