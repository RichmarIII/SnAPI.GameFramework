#include "Rendering/GameRenderMesh.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)

namespace SnAPI::GameFramework
{

bool GameRenderMesh::Valid() const noexcept
{
    return m_Mesh.Valid();
}

void GameRenderMesh::Reset() noexcept
{
    m_Mesh = {};
    m_VertexBuffer = {};
    m_IndexBuffer = {};
    m_SourceId = 0u;
    m_SourceRevision = 0u;
    m_LocalBoundsCenter = Vec3::Zero();
    m_LocalBoundsMin = Vec3::Zero();
    m_LocalBoundsMax = Vec3::Zero();
    m_LocalBoundsRadius = 0.0;
    m_HasLocalBounds = false;
    m_DebugName.clear();
}

SnAPI::Renderer::MeshHandle GameRenderMesh::Mesh() const noexcept
{
    return m_Mesh;
}

SnAPI::Renderer::BufferHandle GameRenderMesh::VertexBuffer() const noexcept
{
    return m_VertexBuffer;
}

SnAPI::Renderer::BufferHandle GameRenderMesh::IndexBuffer() const noexcept
{
    return m_IndexBuffer;
}

std::uint64_t GameRenderMesh::SourceId() const noexcept
{
    return m_SourceId;
}

std::uint64_t GameRenderMesh::SourceRevision() const noexcept
{
    return m_SourceRevision;
}

const Vec3& GameRenderMesh::LocalBoundsCenter() const noexcept
{
    return m_LocalBoundsCenter;
}

const Vec3& GameRenderMesh::LocalBoundsMin() const noexcept
{
    return m_LocalBoundsMin;
}

const Vec3& GameRenderMesh::LocalBoundsMax() const noexcept
{
    return m_LocalBoundsMax;
}

double GameRenderMesh::LocalBoundsRadius() const noexcept
{
    return m_LocalBoundsRadius;
}

bool GameRenderMesh::HasLocalBounds() const noexcept
{
    return m_HasLocalBounds;
}

const std::string& GameRenderMesh::DebugName() const noexcept
{
    return m_DebugName;
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
