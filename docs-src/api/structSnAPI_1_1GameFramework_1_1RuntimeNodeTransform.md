# SnAPI::GameFramework::RuntimeNodeTransform

Local or world transform used by the dense runtime node hierarchy.

Units and coordinate space:
- `Position` uses the same world-space units as the rest of GameFramework.
- `Rotation` is a quaternion.
- `Scale` is component-wise relative scale.

## Public Members

<div class="snapi-api-card" markdown="1">
### `Vec3 SnAPI::GameFramework::RuntimeNodeTransform::Position`

Translation in local or world space, depending on the API.
</div>
<div class="snapi-api-card" markdown="1">
### `Quat SnAPI::GameFramework::RuntimeNodeTransform::Rotation`

Orientation quaternion.
</div>
<div class="snapi-api-card" markdown="1">
### `Vec3 SnAPI::GameFramework::RuntimeNodeTransform::Scale`

Component-wise scale.
</div>
