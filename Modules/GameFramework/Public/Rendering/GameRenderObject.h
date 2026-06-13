#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <cstdint>
#include <string>

#include <SnAPI/Math/LinearAlgebra.h>

#include "Handles.h"
#include "Scene/Mobility.h"
#include "Scene/RenderChannels.h"
#include "Types/Handles.h"

namespace SnAPI::GameFramework
{

class RendererSystem;

class GameRenderObject final
{
public:
    [[nodiscard]] bool Valid() const noexcept;
    void Reset() noexcept;

    [[nodiscard]] SnAPI::Renderer::RenderObjectHandle Object() const noexcept;
    [[nodiscard]] SnAPI::Renderer::MeshHandle Mesh() const noexcept;
    [[nodiscard]] const SnAPI::Math::Matrix4& WorldFromLocal() const noexcept;
    [[nodiscard]] const SnAPI::Math::Matrix4& PreviousWorldFromLocal() const noexcept;
    [[nodiscard]] bool PreviousWorldFromLocalValid() const noexcept;
    [[nodiscard]] bool Visible() const noexcept;
    [[nodiscard]] bool CastShadows() const noexcept;
    [[nodiscard]] SnAPI::Renderer::ERenderMobility Mobility() const noexcept;
    [[nodiscard]] SnAPI::Renderer::RenderFeatureChannelMask FeatureChannels() const noexcept;
    [[nodiscard]] NodeHandle OwnerNode() const noexcept;
    void SetOwnerNode(NodeHandle ownerNode) noexcept;
    [[nodiscard]] const std::string& DebugName() const noexcept;

private:
    friend class RendererSystem;

    SnAPI::Renderer::RenderObjectHandle m_Object{};
    SnAPI::Renderer::MeshHandle m_Mesh{};
    SnAPI::Math::Matrix4 m_WorldFromLocal{SnAPI::Math::Matrix4::Identity()};
    SnAPI::Math::Matrix4 m_PreviousWorldFromLocal{SnAPI::Math::Matrix4::Identity()};
    bool m_PreviousWorldFromLocalValid{false};
    bool m_Visible{false};
    bool m_CastShadows{true};
    SnAPI::Renderer::ERenderMobility m_Mobility{SnAPI::Renderer::ERenderMobility::Dynamic};
    SnAPI::Renderer::RenderFeatureChannelMask m_FeatureChannels{};
    NodeHandle m_OwnerNode{};
    std::string m_DebugName{};
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
