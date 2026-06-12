#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#if defined(SNAPI_GF_ENABLE_LEGACY_RENDERER)
#include <IVertexStreamSource.hpp>
#endif

#include "RenderAssets/MaterialInstanceAsset.h"
#include "RenderAssets/StaticMeshPayload.h"

namespace SnAPI::GameFramework
{

#if defined(SNAPI_GF_ENABLE_RENDERER_NEW)
struct RuntimeMeshStream
{
    EMeshStreamSemantic Semantic = EMeshStreamSemantic::Position;
    std::uint32_t ElementCount = 0;
    std::uint32_t StrideBytes = 0;
    std::vector<std::uint8_t> Bytes{};
};

struct RuntimeMeshSubMesh
{
    std::uint32_t IndexOffset = 0;
    std::uint32_t IndexCount = 0;
    std::uint32_t MaterialSlot = 0;
    std::array<float, 3> BoundsMin{0.0f, 0.0f, 0.0f};
    std::array<float, 3> BoundsMax{0.0f, 0.0f, 0.0f};
};

struct RuntimeMeshData
{
    std::string DebugName{};
    std::uint32_t VertexCount = 0;
    std::uint64_t SourceId = 0;
    std::uint64_t SourceRevision = 0;
    std::vector<RuntimeMeshStream> Streams{};
    std::vector<std::uint32_t> Indices{};
    std::vector<RuntimeMeshSubMesh> SubMeshes{};
};
#endif

/**
 * @ingroup SnAPI_GameFramework
 * @brief Shared runtime representation of one cooked static mesh.
 *
 * The runtime mesh keeps the renderer-facing stream source and the baked material-slot asset refs
 * together in one cached object so gameplay/render components can resolve both through
 * `GetRuntimeShared(...)` without deserializing cooked payloads on the hot path.
 */
struct StaticMeshRuntime
{
#if defined(SNAPI_GF_ENABLE_RENDERER_NEW)
    std::shared_ptr<RuntimeMeshData> MeshData{}; /**< @brief Shared decoded mesh data ready for renderer-runtime upload. */
#elif defined(SNAPI_GF_ENABLE_LEGACY_RENDERER)
    std::shared_ptr<SnAPI::Graphics::IVertexStreamSource> StreamSource{}; /**< @brief Shared renderer vertex stream source built from cooked mesh payload/bulk. */
#endif
    std::vector<MaterialInstanceAssetRef> MaterialRefs{}; /**< @brief Baked material-instance refs authored by the mesh asset, one entry per material slot when available. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Shared runtime representation of one cooked skeletal mesh.
 *
 * The skeletal runtime mesh mirrors `StaticMeshRuntime` by keeping the shared renderer stream source
 * and the baked material-slot refs together. Animation playback still comes from the stream source's
 * cooked skeletal data; the material refs exist so components can apply the authored material instances
 * and then layer any per-component overrides over them.
 */
struct SkeletalMeshRuntime
{
#if defined(SNAPI_GF_ENABLE_RENDERER_NEW)
    std::shared_ptr<RuntimeMeshData> MeshData{}; /**< @brief Shared decoded mesh data ready for renderer-runtime upload. */
#elif defined(SNAPI_GF_ENABLE_LEGACY_RENDERER)
    std::shared_ptr<SnAPI::Graphics::IVertexStreamSource> StreamSource{}; /**< @brief Shared renderer vertex stream source built from cooked skeletal mesh payload/bulk. */
#endif
    std::vector<MaterialInstanceAssetRef> MaterialRefs{}; /**< @brief Baked material-instance refs authored by the skeletal mesh asset, one entry per material slot when available. */
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
