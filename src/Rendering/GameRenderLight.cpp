#include "Rendering/GameRenderLight.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <SnAPI/Math/LinearAlgebra.h>
#include <SnAPI/Math/SemanticTransforms.h>
#include <SnAPI/Math/Types.h>

namespace SnAPI::Renderer
{
using Scalar = SnAPI::Math::Scalar;
using Vec2 = SnAPI::Math::Vec2;
using Vec3 = SnAPI::Math::Vec3;
using Vec4 = SnAPI::Math::Vec4;
using Point3 = SnAPI::Math::Point3D;
using Quat = SnAPI::Math::Quat;
using Matrix4 = SnAPI::Math::Matrix4;
using Transform = SnAPI::Math::Transform;
} // namespace SnAPI::Renderer

#include "Lights/TLightFor.h"

namespace SnAPI::GameFramework
{

bool GameRenderLight::Valid() const noexcept
{
    return m_Light.Valid();
}

void GameRenderLight::Reset() noexcept
{
    m_Light = {};
    m_DirectionalDesc.reset();
    m_DebugName.clear();
}

SnAPI::Renderer::LightHandle GameRenderLight::Light() const noexcept
{
    return m_Light;
}

const SnAPI::Renderer::DirectionalLightDesc* GameRenderLight::DirectionalDesc() const noexcept
{
    return m_DirectionalDesc.get();
}

const std::string& GameRenderLight::DebugName() const noexcept
{
    return m_DebugName;
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
