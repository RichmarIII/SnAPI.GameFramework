#include "Rendering/GameRenderObject.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <utility>

namespace SnAPI::GameFramework
{

bool GameRenderObject::Valid() const noexcept
{
    return m_Object.Valid();
}

void GameRenderObject::Reset() noexcept
{
    m_Object = {};
    m_Mesh = {};
    m_WorldFromLocal = SnAPI::Math::Matrix4::Identity();
    m_PreviousWorldFromLocal = SnAPI::Math::Matrix4::Identity();
    m_PreviousWorldFromLocalValid = false;
    m_Visible = false;
    m_CastShadows = true;
    m_Mobility = SnAPI::Renderer::ERenderMobility::Dynamic;
    m_FeatureChannels = {};
    m_OwnerNode = {};
    m_DebugName.clear();
}

SnAPI::Renderer::RenderObjectHandle GameRenderObject::Object() const noexcept
{
    return m_Object;
}

SnAPI::Renderer::MeshHandle GameRenderObject::Mesh() const noexcept
{
    return m_Mesh;
}

const SnAPI::Math::Matrix4& GameRenderObject::WorldFromLocal() const noexcept
{
    return m_WorldFromLocal;
}

const SnAPI::Math::Matrix4& GameRenderObject::PreviousWorldFromLocal() const noexcept
{
    return m_PreviousWorldFromLocal;
}

bool GameRenderObject::PreviousWorldFromLocalValid() const noexcept
{
    return m_PreviousWorldFromLocalValid;
}

bool GameRenderObject::Visible() const noexcept
{
    return m_Visible;
}

bool GameRenderObject::CastShadows() const noexcept
{
    return m_CastShadows;
}

SnAPI::Renderer::ERenderMobility GameRenderObject::Mobility() const noexcept
{
    return m_Mobility;
}

SnAPI::Renderer::RenderFeatureChannelMask GameRenderObject::FeatureChannels() const noexcept
{
    return m_FeatureChannels;
}

NodeHandle GameRenderObject::OwnerNode() const noexcept
{
    return m_OwnerNode;
}

void GameRenderObject::SetOwnerNode(NodeHandle ownerNode) noexcept
{
    m_OwnerNode = std::move(ownerNode);
}

const std::string& GameRenderObject::DebugName() const noexcept
{
    return m_DebugName;
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
