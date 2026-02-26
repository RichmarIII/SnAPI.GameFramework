#pragma once

#include <algorithm>
#include <any>
#include <cctype>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "AssetManager.h"
#include "AssetPipelineFactories.h"
#include "AssetPipelineIds.h"
#include "BaseNode.h"
#include "Export.h"
#include "IWorld.h"
#include "StaticTypeId.h"
#include "TypeRegistry.h"

namespace SnAPI::GameFramework
{

using TAssetManagerResolver = std::function<::SnAPI::AssetPipeline::AssetManager*()>;

SNAPI_GAMEFRAMEWORK_API void SetDefaultAssetManagerResolver(TAssetManagerResolver Resolver);
SNAPI_GAMEFRAMEWORK_API void ClearDefaultAssetManagerResolver();
SNAPI_GAMEFRAMEWORK_API ::SnAPI::AssetPipeline::AssetManager* ResolveDefaultAssetManager();

template<typename TTag, typename = void>
struct THasAssetRefDefaultName : std::false_type
{
};

template<typename TTag>
struct THasAssetRefDefaultName<TTag, std::void_t<decltype(TTag::Value)>> : std::true_type
{
};

template<typename TBase, typename TNameTag = void>
class TAssetRef
{
public:
    using BaseType = TBase;
    using LoadedObjectType = std::conditional_t<std::is_base_of_v<BaseNode, TBase>, BaseNode, TBase>;
    using TLoadResult = std::expected<std::unique_ptr<LoadedObjectType>, std::string>;
    using TAsyncResult = ::SnAPI::AssetPipeline::AsyncLoadResult<LoadedObjectType>;
    using TAsyncCallback = std::function<void(TAsyncResult)>;

    struct TEntry
    {
        std::string Label{};
        std::string Name{};
        std::string AssetId{};
    };

    TAssetRef()
    {
        if (const std::string DefaultName = DefaultAssetName(); !DefaultName.empty())
        {
            m_assetName = DefaultName;
        }
    }

    explicit TAssetRef(std::string AssetName)
        : TAssetRef()
    {
        m_assetName = TrimCopy(AssetName);
    }

    TAssetRef(std::string AssetName, std::string AssetId)
        : TAssetRef()
    {
        m_assetName = TrimCopy(AssetName);
        m_assetId = TrimCopy(AssetId);
    }

    [[nodiscard]] const std::string& GetAssetName() const
    {
        return m_assetName;
    }

    [[nodiscard]] std::string& EditAssetName()
    {
        return m_assetName;
    }

    [[nodiscard]] const std::string& GetAssetId() const
    {
        return m_assetId;
    }

    [[nodiscard]] std::string& EditAssetId()
    {
        return m_assetId;
    }

    void SetAsset(std::string AssetName, std::string AssetId)
    {
        m_assetName = TrimCopy(AssetName);
        m_assetId = TrimCopy(AssetId);
    }

    void Clear()
    {
        m_assetName.clear();
        m_assetId.clear();
    }

    [[nodiscard]] bool IsNull() const
    {
        return m_assetName.empty() && m_assetId.empty() && DefaultAssetName().empty();
    }

    [[nodiscard]] std::string ResolvedAssetName() const
    {
        if (!m_assetName.empty())
        {
            return m_assetName;
        }
        return DefaultAssetName();
    }

    [[nodiscard]] std::string DisplayLabel() const
    {
        const std::string Name = ResolvedAssetName();
        const std::string Id = TrimCopy(m_assetId);
        if (!Name.empty() && !Id.empty())
        {
            return Name + " [" + ShortAssetId(Id) + "]";
        }
        if (!Name.empty())
        {
            return Name;
        }
        return Id;
    }

    [[nodiscard]] TLoadResult Load(::SnAPI::AssetPipeline::AssetManager& Manager, const std::any& Params = {}) const
    {
        return LoadInternal(Manager, Params);
    }

    [[nodiscard]] TLoadResult Load(const std::any& Params = {}) const
    {
        auto* Manager = ResolveDefaultAssetManager();
        if (!Manager)
        {
            return std::unexpected("No default AssetManager resolver is configured");
        }
        return Load(*Manager, Params);
    }

    [[nodiscard]] ::SnAPI::AssetPipeline::AsyncLoadHandle LoadAsync(
        ::SnAPI::AssetPipeline::AssetManager& Manager,
        ::SnAPI::AssetPipeline::ELoadPriority Priority = ::SnAPI::AssetPipeline::ELoadPriority::Normal,
        const std::any& Params = {},
        TAsyncCallback Callback = {},
        ::SnAPI::AssetPipeline::CancellationToken Token = {}) const
    {
        const std::optional<::SnAPI::AssetPipeline::AssetId> ParsedId = ParsedAssetId();
        const std::string Name = ResolvedAssetName();
        if (!ParsedId.has_value() && Name.empty())
        {
            if (Callback)
            {
                TAsyncResult Error{};
                Error.Error = "AssetRef is empty";
                Callback(std::move(Error));
            }
            return {};
        }

        if constexpr (std::is_base_of_v<BaseNode, TBase>)
        {
            auto WrappedCallback = [Callback = std::move(Callback)](::SnAPI::AssetPipeline::AsyncLoadResult<BaseNode> Raw) mutable {
                if (Raw.Asset && !IsNodeCompatible(Raw.Asset->TypeKey()))
                {
                    Raw.Asset.reset();
                    Raw.Error = BuildTypeMismatchMessage();
                }

                if (!Callback)
                {
                    return;
                }

                TAsyncResult Converted{};
                Converted.Asset = std::move(Raw.Asset);
                Converted.Error = std::move(Raw.Error);
                Converted.bCancelled = Raw.bCancelled;
                Callback(std::move(Converted));
            };

            if (ParsedId.has_value())
            {
                return Manager.LoadAsync<BaseNode>(
                    *ParsedId,
                    Priority,
                    Params,
                    std::move(WrappedCallback),
                    std::move(Token));
            }

            return Manager.LoadAsync<BaseNode>(
                Name,
                Priority,
                Params,
                std::move(WrappedCallback),
                std::move(Token));
        }
        else
        {
            auto WrappedCallback = [Callback = std::move(Callback)](::SnAPI::AssetPipeline::AsyncLoadResult<TBase> Raw) mutable {
                if (!Callback)
                {
                    return;
                }

                TAsyncResult Converted{};
                Converted.Asset = std::move(Raw.Asset);
                Converted.Error = std::move(Raw.Error);
                Converted.bCancelled = Raw.bCancelled;
                Callback(std::move(Converted));
            };

            if (ParsedId.has_value())
            {
                return Manager.LoadAsync<TBase>(
                    *ParsedId,
                    Priority,
                    Params,
                    std::move(WrappedCallback),
                    std::move(Token));
            }

            return Manager.LoadAsync<TBase>(
                Name,
                Priority,
                Params,
                std::move(WrappedCallback),
                std::move(Token));
        }
    }

    [[nodiscard]] ::SnAPI::AssetPipeline::AsyncLoadHandle LoadAsync(
        ::SnAPI::AssetPipeline::ELoadPriority Priority = ::SnAPI::AssetPipeline::ELoadPriority::Normal,
        const std::any& Params = {},
        TAsyncCallback Callback = {},
        ::SnAPI::AssetPipeline::CancellationToken Token = {}) const
    {
        auto* Manager = ResolveDefaultAssetManager();
        if (!Manager)
        {
            if (Callback)
            {
                TAsyncResult Error{};
                Error.Error = "No default AssetManager resolver is configured";
                Callback(std::move(Error));
            }
            return {};
        }

        return LoadAsync(*Manager, Priority, Params, std::move(Callback), std::move(Token));
    }

    template<typename U = TBase>
    [[nodiscard]] std::enable_if_t<std::is_base_of_v<BaseNode, U>, std::expected<NodeHandle, std::string>> Instantiate(
        ::SnAPI::AssetPipeline::AssetManager& Manager,
        IWorld& WorldRef,
        const NodeHandle& Parent = {},
        bool InstantiateAsCopy = true) const
    {
        NodeHandle Spawned{};
        NodeAssetLoadParams Params{};
        Params.TargetWorld = &WorldRef;
        Params.Parent = Parent;
        Params.InstantiateAsCopy = InstantiateAsCopy;
        Params.OutCreatedRoot = &Spawned;

        auto LoadResult = Load(Manager, Params);
        if (!LoadResult)
        {
            return std::unexpected(LoadResult.error());
        }

        if (Spawned.IsNull())
        {
            return std::unexpected("Asset load did not report an instantiated node handle");
        }

        BaseNode* SpawnedNode = Spawned.Borrowed();
        if (!SpawnedNode)
        {
            return std::unexpected("Instantiated node handle could not be resolved");
        }

        if (!IsNodeCompatible(SpawnedNode->TypeKey()))
        {
            (void)WorldRef.DestroyNode(Spawned);
            return std::unexpected(BuildTypeMismatchMessage());
        }

        return Spawned;
    }

    template<typename U = TBase>
    [[nodiscard]] std::enable_if_t<std::is_base_of_v<BaseNode, U>, std::expected<NodeHandle, std::string>> Instantiate(
        IWorld& WorldRef,
        const NodeHandle& Parent = {},
        bool InstantiateAsCopy = true) const
    {
        auto* Manager = ResolveDefaultAssetManager();
        if (!Manager)
        {
            return std::unexpected("No default AssetManager resolver is configured");
        }
        return Instantiate(*Manager, WorldRef, Parent, InstantiateAsCopy);
    }

    [[nodiscard]] static std::vector<TEntry> EnumerateCompatibleAssets(::SnAPI::AssetPipeline::AssetManager& Manager)
    {
        std::vector<TEntry> Entries{};

        for (const auto& CatalogEntry : Manager.ListAssetCatalog())
        {
            const auto& Info = CatalogEntry.Info;

            if constexpr (std::is_base_of_v<BaseNode, TBase>)
            {
                if (Info.AssetKind != AssetKindNode())
                {
                    continue;
                }

                auto Preview = Manager.Load<BaseNode>(Info.Id);
                if (!Preview)
                {
                    continue;
                }

                if (!IsNodeCompatible((*Preview)->TypeKey()))
                {
                    continue;
                }
            }
            else
            {
                auto Preview = Manager.Load<TBase>(Info.Id);
                if (!Preview)
                {
                    continue;
                }
            }

            const std::string AssetName = Info.Name.empty() ? Info.Id.ToString() : Info.Name;
            const std::string AssetId = Info.Id.ToString();
            TEntry Entry{};
            Entry.Name = AssetName;
            Entry.AssetId = AssetId;
            Entry.Label = AssetName + " [" + ShortAssetId(AssetId) + "]";
            Entries.push_back(std::move(Entry));
        }

        std::sort(Entries.begin(), Entries.end(), [](const TEntry& Left, const TEntry& Right) {
            if (Left.Name != Right.Name)
            {
                return Left.Name < Right.Name;
            }
            return Left.AssetId < Right.AssetId;
        });

        return Entries;
    }

    [[nodiscard]] static std::vector<TEntry> EnumerateCompatibleAssets()
    {
        auto* Manager = ResolveDefaultAssetManager();
        if (!Manager)
        {
            return {};
        }
        return EnumerateCompatibleAssets(*Manager);
    }

private:
    [[nodiscard]] static std::string TrimCopy(std::string_view Text)
    {
        size_t Begin = 0;
        while (Begin < Text.size() && std::isspace(static_cast<unsigned char>(Text[Begin])) != 0)
        {
            ++Begin;
        }

        size_t End = Text.size();
        while (End > Begin && std::isspace(static_cast<unsigned char>(Text[End - 1])) != 0)
        {
            --End;
        }

        return std::string(Text.substr(Begin, End - Begin));
    }

    [[nodiscard]] static std::string ShortAssetId(const std::string& AssetId)
    {
        if (AssetId.size() <= 8)
        {
            return AssetId;
        }
        return AssetId.substr(0, 8);
    }

    [[nodiscard]] static std::optional<::SnAPI::AssetPipeline::AssetId> ParseAssetId(std::string_view AssetIdText)
    {
        const std::string Trimmed = TrimCopy(AssetIdText);
        if (Trimmed.empty())
        {
            return std::nullopt;
        }

        const auto Parsed = ::SnAPI::AssetPipeline::AssetId::FromString(Trimmed);
        if (Parsed.IsNull() && Trimmed != "00000000-0000-0000-0000-000000000000")
        {
            return std::nullopt;
        }

        if (Parsed.IsNull())
        {
            return std::nullopt;
        }

        return Parsed;
    }

    [[nodiscard]] std::optional<::SnAPI::AssetPipeline::AssetId> ParsedAssetId() const
    {
        return ParseAssetId(m_assetId);
    }

    [[nodiscard]] static std::string DefaultAssetName()
    {
        if constexpr (THasAssetRefDefaultName<TNameTag>::value)
        {
            return std::string(TNameTag::Value);
        }
        return {};
    }

    [[nodiscard]] static std::string BuildTypeMismatchMessage()
    {
        const TypeId BaseTypeId = StaticTypeId<TBase>();
        const TypeInfo* BaseInfo = TypeRegistry::Instance().Find(BaseTypeId);
        const std::string BaseTypeName = BaseInfo ? BaseInfo->Name : std::string(TTypeNameV<TBase>);
        return "Loaded asset type is incompatible with required base type '" + BaseTypeName + "'";
    }

    [[nodiscard]] static bool IsNodeCompatible(const TypeId& RuntimeNodeType)
    {
        return TypeRegistry::Instance().IsA(RuntimeNodeType, StaticTypeId<TBase>());
    }

    [[nodiscard]] TLoadResult LoadInternal(::SnAPI::AssetPipeline::AssetManager& Manager, const std::any& Params) const
    {
        const std::optional<::SnAPI::AssetPipeline::AssetId> ParsedId = ParsedAssetId();
        const std::string Name = ResolvedAssetName();

        if constexpr (std::is_base_of_v<BaseNode, TBase>)
        {
            if (ParsedId.has_value())
            {
                auto ById = Manager.Load<BaseNode>(*ParsedId, Params);
                if (ById)
                {
                    if (!IsNodeCompatible((*ById)->TypeKey()))
                    {
                        return std::unexpected(BuildTypeMismatchMessage());
                    }
                    return ById;
                }
                if (Name.empty())
                {
                    return std::unexpected(ById.error());
                }
            }

            if (!Name.empty())
            {
                auto ByName = Manager.Load<BaseNode>(Name, Params);
                if (!ByName)
                {
                    return std::unexpected(ByName.error());
                }

                if (!IsNodeCompatible((*ByName)->TypeKey()))
                {
                    return std::unexpected(BuildTypeMismatchMessage());
                }

                return ByName;
            }
        }
        else
        {
            if (ParsedId.has_value())
            {
                auto ById = Manager.Load<TBase>(*ParsedId, Params);
                if (ById)
                {
                    return ById;
                }
                if (Name.empty())
                {
                    return std::unexpected(ById.error());
                }
            }

            if (!Name.empty())
            {
                auto ByName = Manager.Load<TBase>(Name, Params);
                if (!ByName)
                {
                    return std::unexpected(ByName.error());
                }
                return ByName;
            }
        }

        return std::unexpected("AssetRef is empty");
    }

    std::string m_assetName{};
    std::string m_assetId{};
};

} // namespace SnAPI::GameFramework
