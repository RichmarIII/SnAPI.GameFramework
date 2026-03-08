# SnAPI::GameFramework::NodeTransform

Plain transform value used for hierarchy and world-space calculations.

`NodeTransform` is the value type used by transform helpers throughout the module. Depending on context it may represent local space or world space; call sites define which interpretation is in effect.

## Public Members

<div class="snapi-api-card" markdown="1">
### `Vec3 SnAPI::GameFramework::NodeTransform::Position`

Position in local or world space depending on context.
</div>
<div class="snapi-api-card" markdown="1">
### `Quat SnAPI::GameFramework::NodeTransform::Rotation`

Rotation in local or world space depending on context.

Expected to be normalized by helper APIs.
</div>
<div class="snapi-api-card" markdown="1">
### `Vec3 SnAPI::GameFramework::NodeTransform::Scale`

Scale in local or world space depending on context.
</div>
