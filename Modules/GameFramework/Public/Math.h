#pragma once

#include <SnAPI/Math/Types.h>

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Canonical scalar type used by GameFramework math aliases.
 *
 * This alias mirrors the scalar selected by `SnAPI::Math`. All public math-facing
 * GameFramework APIs should use this alias family so precision changes remain global and
 * consistent.
 */
using Scalar = SnAPI::Math::Scalar;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Canonical 2D vector type used across runtime, reflection, and serialization.
 * @remarks Coordinate conventions are inherited from `SnAPI::Math`; GameFramework does not redefine them here.
 */
using Vec2 = SnAPI::Math::Vec2;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Canonical 3D vector type used across runtime, transforms, and serialization.
 * @remarks Coordinate conventions are inherited from `SnAPI::Math`; GameFramework does not redefine them here.
 */
using Vec3 = SnAPI::Math::Vec3;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Canonical 4D vector type used for generic parameter blocks, colors, and vector math.
 */
using Vec4 = SnAPI::Math::Vec4;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Canonical quaternion type used for orientation and rotation composition.
 * @remarks Rotation normalization behavior and storage layout are inherited from `SnAPI::Math::Quat`.
 */
using Quat = SnAPI::Math::Quat;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Canonical rigid transform type used when a combined transform object is preferred over separate fields.
 */
using Transform = SnAPI::Math::Transform;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Canonical axis-aligned bounding box type.
 */
using Aabb = SnAPI::Math::Aabb;

} // namespace SnAPI::GameFramework
