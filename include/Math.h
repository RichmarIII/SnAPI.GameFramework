#pragma once

#include <SnAPI/Math/Types.h>

namespace SnAPI::GameFramework
{

/**
 * @brief Canonical scalar type used by GameFramework math aliases.
 * @remarks Controlled globally through `SNAPI_MATH_USES_DOUBLE`.
 */
using Scalar = SnAPI::Math::Scalar;

/**
 * @brief Canonical 2D vector type used across runtime and serialization.
 * @remarks Alias of `SnAPI::Math::Vec2`.
 */
using Vec2 = SnAPI::Math::Vec2;

/**
 * @brief Canonical 3D vector type used across runtime and serialization.
 * @remarks Alias of `SnAPI::Math::Vec3`.
 */
using Vec3 = SnAPI::Math::Vec3;

/**
 * @brief Canonical 4D vector type used across runtime and serialization.
 * @remarks Alias of `SnAPI::Math::Vec4`.
 */
using Vec4 = SnAPI::Math::Vec4;

/**
 * @brief Canonical quaternion type used for interop with systems that need quaternion rotation.
 * @remarks Alias of `SnAPI::Math::Quat`.
 */
using Quat = SnAPI::Math::Quat;

/**
 * @brief Canonical transform type (position + quaternion rotation).
 * @remarks Alias of `SnAPI::Math::Transform`.
 */
using Transform = SnAPI::Math::Transform;

/**
 * @brief Canonical AABB type.
 * @remarks Alias of `SnAPI::Math::Aabb`.
 */
using Aabb = SnAPI::Math::Aabb;

} // namespace SnAPI::GameFramework
