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

    ::SnAPI::AssetPipeline::AssetId AssetId{};
    std::string LogicalName{};

    /**
     * @brief Default-construct an authored asset identity wrapper.
     */
    IAsset() = default;

    /**
     * @brief Copy authored asset identity state.
     * @param Other Source asset identity wrapper.
     */
    IAsset(const IAsset& Other) = default;

    /**
     * @brief Move authored asset identity state.
     * @param Other Source asset identity wrapper.
     *
     * `IAsset` declares a virtual destructor, so the move operations need to be
     * defaulted explicitly to preserve normal value semantics for derived asset
     * structs that are copied and moved through `TExpected` and other containers.
     */
    IAsset(IAsset&& Other) noexcept = default;

    /**
     * @brief Copy-assign authored asset identity state.
     * @param Other Source asset identity wrapper.
     * @return Reference to this instance.
     */
    IAsset& operator=(const IAsset& Other) = default;

    /**
     * @brief Move-assign authored asset identity state.
     * @param Other Source asset identity wrapper.
     * @return Reference to this instance.
     */
    IAsset& operator=(IAsset&& Other) noexcept = default;

    virtual ~IAsset() = default;

    void SetPersistentIdentity(const ::SnAPI::AssetPipeline::AssetId& InAssetId, std::string_view InLogicalName)
    {
        AssetId = InAssetId;
        LogicalName = std::string(InLogicalName);
    }

    void EnsurePersistentIdentity(std::string_view FallbackLogicalName)
    {
        if (AssetId.IsNull())
        {
            AssetId = ::SnAPI::AssetPipeline::AssetId::Generate();
        }
        if (LogicalName.empty())
        {
            LogicalName = std::string(FallbackLogicalName);
        }
    }

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
