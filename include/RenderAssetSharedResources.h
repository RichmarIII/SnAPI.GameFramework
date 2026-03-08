#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <memory>
#include <string_view>
#include <vector>

#include "RenderAssetRuntime.h"

namespace SnAPI::AssetPipeline
{
class AssetManager;
}

namespace SnAPI::Graphics
{
class IRenderObject;
class IVertexStreamSource;
class MaterialInstance;
} // namespace SnAPI::Graphics

namespace SnAPI::GameFramework
{

class RendererSystem;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Acquire or build a shared renderer vertex-stream source for a runtime mesh asset.
 * @param RuntimeMesh Borrowed runtime mesh description.
 * @param StableKey Optional stable cache key. When empty, the helper derives a key from the source asset id,
 * asset name, or object address.
 * @return Shared vertex-stream source, or `nullptr` if the runtime mesh cannot be converted into renderer streams.
 *
 * Core semantics:
 * - Sources are cached globally by key and returned as shared pointers.
 * - Expired cache entries are rebuilt on demand.
 * - The function may decode bulk stream data synchronously.
 */
std::shared_ptr<SnAPI::Graphics::IVertexStreamSource> AcquireSharedRuntimeMeshStreamSource(
    const StaticMeshAssetRuntime& RuntimeMesh,
    std::string_view StableKey);

/**
 * @ingroup SnAPI_GameFramework
 * @brief Assign default gbuffer and shadow material instances to every submesh of a render object.
 * @param RenderObject Borrowed render object to update.
 * @param Renderer Borrowed renderer system that provides the default materials.
 *
 * This is the fallback path used when no runtime material-instance assets are available.
 */
void ApplyDefaultMaterialInstances(SnAPI::Graphics::IRenderObject& RenderObject, RendererSystem& Renderer);

/**
 * @ingroup SnAPI_GameFramework
 * @brief Assign runtime material instances to a render object, falling back to defaults when resolution fails.
 * @param RenderObject Borrowed render object to update.
 * @param Renderer Borrowed renderer system that provides default materials.
 * @param MaterialRefs Borrowed list of material-instance asset references indexed by material slot.
 * @param AssetManager Borrowed asset manager used to resolve runtime material and texture assets, or `nullptr` to force fallback behavior.
 *
 * Material assignment is performed per submesh. If a referenced runtime material instance cannot be
 * resolved, the default gbuffer and shadow materials remain bound for that submesh.
 */
void ApplyRuntimeOrDefaultMaterialInstances(
    SnAPI::Graphics::IRenderObject& RenderObject,
    RendererSystem& Renderer,
    const std::vector<TAssetRef<MaterialInstanceAssetRuntime>>& MaterialRefs,
    ::SnAPI::AssetPipeline::AssetManager* AssetManager);

/**
 * @ingroup SnAPI_GameFramework
 * @brief Invalidate the shared runtime material caches used for mesh material assignment.
 *
 * Call this after saving, reimporting, or reloading material and material-instance assets so future
 * bindings do not reuse stale cached renderer material instances.
 */
void InvalidateRuntimeMaterialCaches();

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
