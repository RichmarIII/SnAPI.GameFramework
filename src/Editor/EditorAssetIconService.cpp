#include "Editor/EditorAssetIconService.h"

#include "AssetPipelineIds.h"
#include "AssetRef.h"
#include "GameRuntime.h"
#include "RenderAssetSourcePayloads.h"
#include "TextureCompressorIds.h"
#include "World.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)
#include <Image.hpp>
#endif

namespace SnAPI::GameFramework::Editor
{
struct EditorAssetIconService::TextureBinding
{
    ::SnAPI::AssetPipeline::AssetId AssetId{};
    const SnAPI::UI::UIContext* Context = nullptr;
    std::uint32_t TextureId = 0;
    std::uint32_t TextureWidth = 0;
    std::uint32_t TextureHeight = 0;
    std::shared_ptr<::SnAPI::Graphics::IGPUImage> RuntimeTexture{};
};

EditorAssetIconService::~EditorAssetIconService() = default;

std::string_view EditorAssetIconService::Name() const
{
    return "EditorAssetIconService";
}

std::vector<std::type_index> EditorAssetIconService::Dependencies() const
{
    return {std::type_index(typeid(EditorAssetService))};
}

Result EditorAssetIconService::Initialize(EditorServiceContext& Context)
{
    (void)Context;
    m_boundContext = nullptr;
    m_textureBindingsByAssetKey.clear();
    m_nextTextureId = 0x70000000u;
    ++m_revision;
    return Ok();
}

void EditorAssetIconService::Shutdown(EditorServiceContext& Context)
{
    ResetAllBindings(Context);
    m_boundContext = nullptr;
}

void EditorAssetIconService::Synchronize(EditorServiceContext& Context,
                                         const std::vector<EditorAssetService::DiscoveredAsset>& Assets,
                                         const SnAPI::UI::UIContext* UiContext)
{
    if (UiContext != m_boundContext)
    {
        ResetAllBindings(Context);
        m_boundContext = UiContext;
    }

    if (m_textureBindingsByAssetKey.empty())
    {
        return;
    }

    std::unordered_map<std::string, const EditorAssetService::DiscoveredAsset*> TextureAssetsByKey{};
    TextureAssetsByKey.reserve(Assets.size());
    for (const auto& Asset : Assets)
    {
        if (Asset.AssetKind == TextureCompressorPlugin::AssetKind_CompressedTexture)
        {
            TextureAssetsByKey.emplace(Asset.Key, &Asset);
        }
    }

    std::vector<std::string> KeysToRemove{};
    KeysToRemove.reserve(m_textureBindingsByAssetKey.size());

#if defined(SNAPI_GF_ENABLE_RENDERER)
    auto* WorldPtr = Context.Runtime().WorldPtr();
#endif

    for (auto& [AssetKey, Binding] : m_textureBindingsByAssetKey)
    {
        const auto AssetIt = TextureAssetsByKey.find(AssetKey);
        if (AssetIt == TextureAssetsByKey.end() || !Binding || Binding->TextureId == 0)
        {
            KeysToRemove.push_back(AssetKey);
            continue;
        }
        if (UiContext == nullptr || Binding->Context != UiContext)
        {
            KeysToRemove.push_back(AssetKey);
            continue;
        }

#if defined(SNAPI_GF_ENABLE_RENDERER)
        const auto& Asset = *AssetIt->second;

        TAssetRef<TextureAsset> TextureRef{};
        TextureRef.EditAssetName() = Asset.Name;
        TextureRef.EditAssetId() = Asset.AssetId.ToString();
        if (auto TextureResult = TextureRef.GetRuntimeShared<::SnAPI::Graphics::IGPUImage>(); TextureResult && *TextureResult)
        {
            Binding->AssetId = Asset.AssetId;
            Binding->RuntimeTexture = *TextureResult;
        }

        auto* Image = Binding->RuntimeTexture.get();
        if (!WorldPtr || !Image ||
            !WorldPtr->Renderer().RegisterExternalImageUiTexture(*UiContext, Binding->TextureId, Image, true))
        {
            KeysToRemove.push_back(AssetKey);
        }
#else
        (void)AssetIt;
#endif
    }

    for (const auto& AssetKey : KeysToRemove)
    {
        RemoveBinding(Context, AssetKey);
    }
}

void EditorAssetIconService::InvalidateAsset(EditorServiceContext& Context, std::string_view AssetKey)
{
    if (AssetKey.empty())
    {
        return;
    }
    RemoveBinding(Context, AssetKey);
}

EditorAssetIconService::AssetIconMetadata EditorAssetIconService::ResolveAssetIcon(
    EditorServiceContext& Context,
    const EditorAssetService::DiscoveredAsset& Asset,
    const SnAPI::UI::UIContext* UiContext)
{
    AssetIconMetadata Metadata = BuildFallbackIcon(Asset);
    if (!UiContext || Asset.AssetKind != TextureCompressorPlugin::AssetKind_CompressedTexture)
    {
        return Metadata;
    }

    if (UiContext != m_boundContext)
    {
        ResetAllBindings(Context);
        m_boundContext = UiContext;
    }

    if (const auto ExistingIt = m_textureBindingsByAssetKey.find(Asset.Key);
        ExistingIt != m_textureBindingsByAssetKey.end())
    {
        const TextureBinding& Existing = *ExistingIt->second;
        if (Existing.AssetId == Asset.AssetId && Existing.Context == UiContext && Existing.TextureId != 0)
        {
            Metadata.TextureId = Existing.TextureId;
            Metadata.TextureWidth = Existing.TextureWidth;
            Metadata.TextureHeight = Existing.TextureHeight;
            return Metadata;
        }
        RemoveBinding(Context, Asset.Key);
    }

    TAssetRef<TextureAsset> TextureRef{};
    TextureRef.EditAssetName() = Asset.Name;
    TextureRef.EditAssetId() = Asset.AssetId.ToString();
    auto TextureResult = TextureRef.GetRuntimeShared<::SnAPI::Graphics::IGPUImage>();
    if (!TextureResult || !*TextureResult)
    {
        return Metadata;
    }

    auto* WorldPtr = Context.Runtime().WorldPtr();
    if (!WorldPtr)
    {
        return Metadata;
    }

#if defined(SNAPI_GF_ENABLE_RENDERER)
    const std::uint32_t TextureId = AllocateTextureId();
    if (TextureId == 0)
    {
        return Metadata;
    }

    if (!WorldPtr->Renderer().RegisterExternalImageUiTexture(*UiContext, TextureId, TextureResult->get(), true))
    {
        return Metadata;
    }

    auto Binding = std::make_shared<TextureBinding>();
    const auto Extent = (*TextureResult)->Extent();
    Binding->AssetId = Asset.AssetId;
    Binding->Context = UiContext;
    Binding->TextureId = TextureId;
    Binding->TextureWidth = Extent.x();
    Binding->TextureHeight = Extent.y();
    Binding->RuntimeTexture = *TextureResult;
    m_textureBindingsByAssetKey[Asset.Key] = std::move(Binding);
    Metadata.TextureId = TextureId;
    Metadata.TextureWidth = Extent.x();
    Metadata.TextureHeight = Extent.y();
    ++m_revision;
#endif
    return Metadata;
}

EditorAssetIconService::AssetIconMetadata EditorAssetIconService::BuildFallbackIcon(
    const EditorAssetService::DiscoveredAsset& Asset) const
{
    AssetIconMetadata Metadata{};
    if (Asset.AssetKind == TextureCompressorPlugin::AssetKind_CompressedTexture)
    {
        Metadata.IconSource = "editor://Assets/sphere.svg";
    }
    else if (Asset.AssetKind == AssetKindMaterial())
    {
        Metadata.IconSource = "editor://Assets/component.svg";
    }
    else if (Asset.AssetKind == AssetKindMaterialInstance())
    {
        Metadata.IconSource = "editor://Assets/box.svg";
    }
    else if (Asset.AssetKind == AssetKindStaticMesh() ||
             Asset.AssetKind == AssetKindSkeletalMesh())
    {
        Metadata.IconSource = "editor://Assets/box.svg";
    }
    else if (Asset.AssetKind == AssetKindLevel())
    {
        Metadata.IconSource = "editor://Assets/level.svg";
    }
    else if (Asset.AssetKind == AssetKindWorld())
    {
        Metadata.IconSource = "editor://Assets/world.svg";
    }
    else
    {
        Metadata.IconSource = "editor://Assets/component.svg";
    }
    return Metadata;
}

std::uint32_t EditorAssetIconService::AllocateTextureId()
{
    // Reserve a high-id range for editor-owned external-image bindings.
    if (m_nextTextureId == 0u)
    {
        m_nextTextureId = 0x70000000u;
    }
    return m_nextTextureId++;
}

void EditorAssetIconService::RemoveBinding(EditorServiceContext& Context, std::string_view AssetKey)
{
    const auto It = m_textureBindingsByAssetKey.find(std::string(AssetKey));
    if (It == m_textureBindingsByAssetKey.end())
    {
        return;
    }

#if defined(SNAPI_GF_ENABLE_RENDERER)
    auto* WorldPtr = Context.Runtime().WorldPtr();
    if (WorldPtr && It->second && It->second->Context && It->second->TextureId != 0)
    {
        (void)WorldPtr->Renderer().UnregisterExternalImageUiTexture(*It->second->Context, It->second->TextureId);
    }
#endif

    m_textureBindingsByAssetKey.erase(It);
    ++m_revision;
}

void EditorAssetIconService::ResetAllBindings(EditorServiceContext& Context)
{
    if (m_textureBindingsByAssetKey.empty())
    {
        return;
    }

    std::vector<std::string> Keys{};
    Keys.reserve(m_textureBindingsByAssetKey.size());
    for (const auto& [AssetKey, _] : m_textureBindingsByAssetKey)
    {
        Keys.push_back(AssetKey);
    }
    for (const auto& AssetKey : Keys)
    {
        RemoveBinding(Context, AssetKey);
    }
}


} // namespace SnAPI::GameFramework::Editor
