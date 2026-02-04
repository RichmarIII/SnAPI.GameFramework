#pragma once

#include <cmath>

namespace SnAPI::GameFramework
{

/**
 * @brief Simple 3D vector type.
 * @remarks Lightweight math utility used by core components.
 */
struct Vec3
{
    float X = 0.0f; /**< @brief X component. */
    float Y = 0.0f; /**< @brief Y component. */
    float Z = 0.0f; /**< @brief Z component. */

    /** @brief Construct a zero vector. */
    constexpr Vec3() = default;
    /**
     * @brief Construct a vector from components.
     * @param InX X component.
     * @param InY Y component.
     * @param InZ Z component.
     */
    constexpr Vec3(float InX, float InY, float InZ)
        : X(InX)
        , Y(InY)
        , Z(InZ)
    {
    }

    /**
     * @brief Add another vector in-place.
     * @param Other Vector to add.
     * @return Reference to this vector.
     */
    Vec3& operator+=(const Vec3& Other)
    {
        X += Other.X;
        Y += Other.Y;
        Z += Other.Z;
        return *this;
    }

    /**
     * @brief Subtract another vector in-place.
     * @param Other Vector to subtract.
     * @return Reference to this vector.
     */
    Vec3& operator-=(const Vec3& Other)
    {
        X -= Other.X;
        Y -= Other.Y;
        Z -= Other.Z;
        return *this;
    }

    /**
     * @brief Multiply by a scalar in-place.
     * @param Scalar Scalar multiplier.
     * @return Reference to this vector.
     */
    Vec3& operator*=(float Scalar)
    {
        X *= Scalar;
        Y *= Scalar;
        Z *= Scalar;
        return *this;
    }
};

/**
 * @brief Vector addition.
 * @param Left Left-hand vector.
 * @param Right Right-hand vector.
 * @return Sum of the two vectors.
 */
inline Vec3 operator+(Vec3 Left, const Vec3& Right)
{
    Left += Right;
    return Left;
}

/**
 * @brief Vector subtraction.
 * @param Left Left-hand vector.
 * @param Right Right-hand vector.
 * @return Difference of the two vectors.
 */
inline Vec3 operator-(Vec3 Left, const Vec3& Right)
{
    Left -= Right;
    return Left;
}

/**
 * @brief Scalar multiplication (vector * scalar).
 * @param Left Vector.
 * @param Scalar Scalar multiplier.
 * @return Scaled vector.
 */
inline Vec3 operator*(Vec3 Left, float Scalar)
{
    Left *= Scalar;
    return Left;
}

/**
 * @brief Scalar multiplication (scalar * vector).
 * @param Scalar Scalar multiplier.
 * @param Right Vector.
 * @return Scaled vector.
 */
inline Vec3 operator*(float Scalar, Vec3 Right)
{
    Right *= Scalar;
    return Right;
}

} // namespace SnAPI::GameFramework
