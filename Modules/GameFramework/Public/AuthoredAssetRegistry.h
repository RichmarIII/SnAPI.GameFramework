#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Export.h"
#include "IAsset.h"
#include "TypeRegistry.h"

namespace SnAPI::GameFramework
{

struct AuthoredAssetDescriptor
{
    TypeId AssetType{};
    const TypeInfo* Type = nullptr;
    std::string DisplayName{};
    std::string Category{};
    std::string FileExtension{};
    EAssetEditorMode EditorMode = EAssetEditorMode::Inspector;
    bool CanCreate = true;
    bool CanSave = true;
    bool CanDelete = true;
    bool CanRename = true;
    ::SnAPI::AssetPipeline::TypeId SourceAssetKind{};
    ::SnAPI::AssetPipeline::TypeId SourcePayloadType{};
    ::SnAPI::AssetPipeline::TypeId CookedAssetKind{};
    ::SnAPI::AssetPipeline::TypeId CookedPayloadType{};
};

class SNAPI_GAMEFRAMEWORK_API AuthoredAssetRegistry
{
public:
    static AuthoredAssetRegistry& Instance();

    void EnsureBuilt();
    [[nodiscard]] bool IsValid() const { return m_diagnostics.empty(); }
    [[nodiscard]] const std::vector<AuthoredAssetDescriptor>& All() const { return m_assets; }
    [[nodiscard]] const std::vector<std::string>& Diagnostics() const { return m_diagnostics; }
    [[nodiscard]] const AuthoredAssetDescriptor* FindByType(const TypeId& Type) const;
    [[nodiscard]] const AuthoredAssetDescriptor* FindByExtension(std::string_view Extension) const;
    [[nodiscard]] const AuthoredAssetDescriptor* FindBySourceAssetKind(const ::SnAPI::AssetPipeline::TypeId& AssetKind) const;
    [[nodiscard]] const AuthoredAssetDescriptor* FindBySourcePayloadType(const ::SnAPI::AssetPipeline::TypeId& PayloadType) const;
    [[nodiscard]] const AuthoredAssetDescriptor* FindByCookedAssetKind(const ::SnAPI::AssetPipeline::TypeId& AssetKind) const;
    [[nodiscard]] const AuthoredAssetDescriptor* FindByCookedPayloadType(const ::SnAPI::AssetPipeline::TypeId& PayloadType) const;

private:
    void AddDiagnostic(std::string Message);
    void Build();

    bool m_built = false;
    std::vector<AuthoredAssetDescriptor> m_assets{};
    std::vector<std::string> m_diagnostics{};
    std::unordered_map<TypeId, std::size_t, UuidHash> m_indexByType{};
    std::unordered_map<std::string, std::size_t, TransparentStringHash, TransparentStringEqual> m_indexByExtension{};
    std::unordered_map<::SnAPI::AssetPipeline::TypeId, std::size_t, ::SnAPI::AssetPipeline::UuidHash> m_indexBySourceAssetKind{};
    std::unordered_map<::SnAPI::AssetPipeline::TypeId, std::size_t, ::SnAPI::AssetPipeline::UuidHash> m_indexBySourcePayloadType{};
    std::unordered_map<::SnAPI::AssetPipeline::TypeId, std::size_t, ::SnAPI::AssetPipeline::UuidHash> m_indexByCookedAssetKind{};
    std::unordered_map<::SnAPI::AssetPipeline::TypeId, std::size_t, ::SnAPI::AssetPipeline::UuidHash> m_indexByCookedPayloadType{};
};

} // namespace SnAPI::GameFramework
