# SnAPI::GameFramework::Vec3

Simple 3D vector type.

## Public Members

<div class="snapi-api-card" markdown="1">
### `float SnAPI::GameFramework::Vec3::X`

X component.
</div>
<div class="snapi-api-card" markdown="1">
### `float SnAPI::GameFramework::Vec3::Y`

Y component.
</div>
<div class="snapi-api-card" markdown="1">
### `float SnAPI::GameFramework::Vec3::Z`

Z component.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::Vec3::Vec3()=default`

Construct a zero vector.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::Vec3::Vec3(float InX, float InY, float InZ)`

Construct a vector from components.

**Parameters**

- `InX`: X component.
- `InY`: Y component.
- `InZ`: Z component.
</div>
<div class="snapi-api-card" markdown="1">
### `Vec3 & SnAPI::GameFramework::Vec3::operator+=(const Vec3 &Other)`

Add another vector in-place.

**Parameters**

- `Other`: Vector to add.

**Returns:** Reference to this vector.
</div>
<div class="snapi-api-card" markdown="1">
### `Vec3 & SnAPI::GameFramework::Vec3::operator-=(const Vec3 &Other)`

Subtract another vector in-place.

**Parameters**

- `Other`: Vector to subtract.

**Returns:** Reference to this vector.
</div>
<div class="snapi-api-card" markdown="1">
### `Vec3 & SnAPI::GameFramework::Vec3::operator*=(float Scalar)`

Multiply by a scalar in-place.

**Parameters**

- `Scalar`: Scalar multiplier.

**Returns:** Reference to this vector.
</div>
