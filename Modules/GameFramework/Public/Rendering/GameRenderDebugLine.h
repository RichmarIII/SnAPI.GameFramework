#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <array>

#include "Math.h"

namespace SnAPI::GameFramework
{

struct GameRenderDebugLine
{
    Vec3 StartWorld{Vec3::Zero()};
    Vec3 EndWorld{Vec3::Zero()};
    std::array<float, 4> ColorLinear{1.0f, 1.0f, 1.0f, 1.0f};
    float ThicknessPixels{1.0f};
    bool DepthTest{true};
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
