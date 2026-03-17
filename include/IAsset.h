#pragma once

#include <iosfwd>
#include <string>
#include <string_view>

#include "AssetPipelineIds.h"
#include "Expected.h"
#include "ReflectionAnnotations.h"

namespace SnAPI::GameFramework
{

enum class EAssetEditorMode : std::uint8_t
{
    Inspector = 0,
    ConduitGraph,
    ConduitClass,
};

SnType(SnInterface)
class IAsset
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::IAsset";

    virtual ~IAsset() = default;

    [[nodiscard]] virtual std::string_view DisplayName() const = 0;
    [[nodiscard]] virtual std::string_view FileExtension() const = 0;
    [[nodiscard]] virtual std::string_view Category() const { return "Assets"; }
    [[nodiscard]] virtual EAssetEditorMode EditorMode() const { return EAssetEditorMode::Inspector; }

    [[nodiscard]] virtual bool CanCreate() const { return true; }
    [[nodiscard]] virtual bool CanSave() const { return true; }
    [[nodiscard]] virtual bool CanDelete() const { return true; }
    [[nodiscard]] virtual bool CanRename() const { return true; }

    [[nodiscard]] virtual Result Save(std::ostream& Output) const = 0;
    [[nodiscard]] virtual ::SnAPI::AssetPipeline::TypeId SourceAssetKind() const = 0;
    [[nodiscard]] virtual ::SnAPI::AssetPipeline::TypeId SourcePayloadType() const = 0;
    [[nodiscard]] virtual ::SnAPI::AssetPipeline::TypeId CookedAssetKind() const { return SourceAssetKind(); }
    [[nodiscard]] virtual ::SnAPI::AssetPipeline::TypeId CookedPayloadType() const { return SourcePayloadType(); }
};

} // namespace SnAPI::GameFramework
