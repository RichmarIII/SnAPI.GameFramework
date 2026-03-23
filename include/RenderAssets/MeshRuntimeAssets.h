#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <memory>
#include <vector>

#include <IVertexStreamSource.hpp>

#include "RenderAssets/MaterialInstanceAsset.h"

namespace SnAPI::GameFramework
{

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
    std::shared_ptr<SnAPI::Graphics::IVertexStreamSource> StreamSource{}; /**< @brief Shared renderer vertex stream source built from cooked mesh payload/bulk. */
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
    std::shared_ptr<SnAPI::Graphics::IVertexStreamSource> StreamSource{}; /**< @brief Shared renderer vertex stream source built from cooked skeletal mesh payload/bulk. */
    std::vector<MaterialInstanceAssetRef> MaterialRefs{}; /**< @brief Baked material-instance refs authored by the skeletal mesh asset, one entry per material slot when available. */
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
