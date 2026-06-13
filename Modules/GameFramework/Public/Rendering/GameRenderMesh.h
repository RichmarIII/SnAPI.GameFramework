#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <cstdint>
#include <string>

#include "Math.h"
#include "Types/Handles.h"

namespace SnAPI::GameFramework
{

class RendererSystem;

class GameRenderMesh final
{
public:
    [[nodiscard]] bool Valid() const noexcept;
    void Reset() noexcept;

    [[nodiscard]] SnAPI::Renderer::MeshHandle Mesh() const noexcept;
    [[nodiscard]] SnAPI::Renderer::BufferHandle VertexBuffer() const noexcept;
    [[nodiscard]] SnAPI::Renderer::BufferHandle IndexBuffer() const noexcept;
    [[nodiscard]] std::uint64_t SourceId() const noexcept;
    [[nodiscard]] std::uint64_t SourceRevision() const noexcept;
    [[nodiscard]] const Vec3& LocalBoundsCenter() const noexcept;
    [[nodiscard]] const Vec3& LocalBoundsMin() const noexcept;
    [[nodiscard]] const Vec3& LocalBoundsMax() const noexcept;
    [[nodiscard]] double LocalBoundsRadius() const noexcept;
    [[nodiscard]] bool HasLocalBounds() const noexcept;
    [[nodiscard]] const std::string& DebugName() const noexcept;

private:
    friend class RendererSystem;

    SnAPI::Renderer::MeshHandle m_Mesh{};
    SnAPI::Renderer::BufferHandle m_VertexBuffer{};
    SnAPI::Renderer::BufferHandle m_IndexBuffer{};
    std::uint64_t m_SourceId{0u};
    std::uint64_t m_SourceRevision{0u};
    Vec3 m_LocalBoundsCenter{Vec3::Zero()};
    Vec3 m_LocalBoundsMin{Vec3::Zero()};
    Vec3 m_LocalBoundsMax{Vec3::Zero()};
    double m_LocalBoundsRadius{0.0};
    bool m_HasLocalBounds{false};
    std::string m_DebugName{};
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
