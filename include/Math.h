#pragma once

#include <cmath>

namespace SnAPI::GameFramework
{

struct Vec3
{
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;

    constexpr Vec3() = default;
    constexpr Vec3(float InX, float InY, float InZ)
        : X(InX)
        , Y(InY)
        , Z(InZ)
    {
    }

    Vec3& operator+=(const Vec3& Other)
    {
        X += Other.X;
        Y += Other.Y;
        Z += Other.Z;
        return *this;
    }

    Vec3& operator-=(const Vec3& Other)
    {
        X -= Other.X;
        Y -= Other.Y;
        Z -= Other.Z;
        return *this;
    }

    Vec3& operator*=(float Scalar)
    {
        X *= Scalar;
        Y *= Scalar;
        Z *= Scalar;
        return *this;
    }
};

inline Vec3 operator+(Vec3 Left, const Vec3& Right)
{
    Left += Right;
    return Left;
}

inline Vec3 operator-(Vec3 Left, const Vec3& Right)
{
    Left -= Right;
    return Left;
}

inline Vec3 operator*(Vec3 Left, float Scalar)
{
    Left *= Scalar;
    return Left;
}

inline Vec3 operator*(float Scalar, Vec3 Right)
{
    Right *= Scalar;
    return Right;
}

} // namespace SnAPI::GameFramework
