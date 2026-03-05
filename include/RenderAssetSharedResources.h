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

std::shared_ptr<SnAPI::Graphics::IVertexStreamSource> AcquireSharedRuntimeMeshStreamSource(
    const StaticMeshAssetRuntime& RuntimeMesh,
    std::string_view StableKey);

void ApplyDefaultMaterialInstances(SnAPI::Graphics::IRenderObject& RenderObject, RendererSystem& Renderer);

void ApplyRuntimeOrDefaultMaterialInstances(
    SnAPI::Graphics::IRenderObject& RenderObject,
    RendererSystem& Renderer,
    const std::vector<TAssetRef<MaterialInstanceAssetRuntime>>& MaterialRefs,
    ::SnAPI::AssetPipeline::AssetManager* AssetManager);

/**
 * @brief Invalidate shared runtime material caches used for mesh material assignment.
 * @remarks
 * Call this after saving/reloading material or material-instance assets so new bindings
 * do not reuse stale cached MaterialInstances.
 */
void InvalidateRuntimeMaterialCaches();

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
