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

/**
 * @ingroup SnAPI_GameFramework
 * @brief Install the process-wide fallback resolver used by `TAssetRef` overloads that omit an explicit asset manager.
 * @param Resolver Callback that returns the asset manager to use, or `nullptr`.
 *
 * The resolver is stored globally and is consulted synchronously on each default-manager call.
 * Replacing the resolver affects all subsequent `TAssetRef` operations that do not supply a manager.
 */
SNAPI_GAMEFRAMEWORK_API void SetDefaultAssetManagerResolver(TAssetManagerResolver Resolver);
/**
 * @ingroup SnAPI_GameFramework
 * @brief Clear the process-wide default asset-manager resolver.
 */
SNAPI_GAMEFRAMEWORK_API void ClearDefaultAssetManagerResolver();
/**
 * @ingroup SnAPI_GameFramework
 * @brief Resolve the current default asset manager, or `nullptr` if no resolver is configured.
 */
SNAPI_GAMEFRAMEWORK_API ::SnAPI::AssetPipeline::AssetManager* ResolveDefaultAssetManager();

/**
 * @ingroup SnAPI_GameFramework
 * @brief Trait that detects whether an asset-reference name tag exposes a static default asset name.
 *
 * A tag participates when it provides a `Value` member containing the default name text.
 */
template<typename TTag, typename = void>
struct THasAssetRefDefaultName : std::false_type
{
};

template<typename TTag>
struct THasAssetRefDefaultName<TTag, std::void_t<decltype(TTag::Value)>> : std::true_type
{
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Typed reference to an asset that can be resolved by asset id, asset name, or a tagged default name.
 *
 * `TAssetRef` is the engine-facing handle type used in reflected properties, runtime settings, and
 * serialized payloads to point at assets managed by the AssetPipeline.
 *
 * Core semantics:
 * - References may be populated by asset id, asset name, or both.
 * - Resolution prefers asset id first and falls back to the resolved asset name when id lookup fails.
 * - For `BaseNode`-derived types, the reference enforces runtime type compatibility using reflection.
 * - Node asset references can either load detached objects or instantiate directly into a world.
 * - Overloads without an explicit manager use the process-wide default asset-manager resolver.
 *
 * Ownership and lifetime:
 * - `Load()` returns an owning `std::unique_ptr` to a detached runtime object unless the supplied load
 *   params instruct the asset factory to instantiate into a world.
 * - `GetShared()` returns an AssetPipeline handle that shares ownership with the asset manager.
 * - `Instantiate()` returns a non-owning `NodeHandle` into the destination world.
 *
 * Threading model:
 * - `TAssetRef` value operations are thread-safe in isolation.
 * - Actual asset loading and default-manager resolution obey the thread-safety contract of the
 *   underlying `AssetManager`.
 *
 * Error semantics:
 * - Fails by returning `std::unexpected<std::string>` or an async result with the `Error` field populated.
 * - Empty references fail with `"AssetRef is empty"`.
 */
template<typename TBase, typename TNameTag = void>
class TAssetRef
{
public:
    using BaseType = TBase;
    using LoadedObjectType = std::conditional_t<std::is_base_of_v<BaseNode, TBase>, BaseNode, TBase>;
    using TLoadResult = std::expected<std::unique_ptr<LoadedObjectType>, std::string>;
    using TAsyncResult = ::SnAPI::AssetPipeline::AsyncLoadResult<LoadedObjectType>;
    using TAsyncCallback = std::function<void(TAsyncResult)>;

    /**
     * @brief One compatible-asset entry returned by `EnumerateCompatibleAssets()`.
     */
    struct TEntry
    {
        std::string Label{}; /**< @brief Preformatted display label combining the best user-facing name and a short id suffix. */
        std::string Name{}; /**< @brief Asset catalog name or stringified asset id when no name exists. */
        std::string AssetId{}; /**< @brief Canonical string form of the AssetPipeline asset id. */
    };

    /** @brief Construct an empty asset reference or a tag-default reference when `TNameTag` supplies one. */
    TAssetRef()
    {
        if (const std::string DefaultName = DefaultAssetName(); !DefaultName.empty())
        {
            m_assetName = DefaultName;
        }
    }

    /**
     * @brief Construct a reference from an asset name.
     * @param AssetName Asset catalog name. Leading and trailing ASCII whitespace is trimmed.
     */
    explicit TAssetRef(std::string AssetName)
        : TAssetRef()
    {
        m_assetName = TrimCopy(AssetName);
    }

    /**
     * @brief Construct a reference from both name and id text.
     * @param AssetName Asset catalog name.
     * @param AssetId Canonical asset-id string.
     *
     * Supplying both values allows id-first lookup with a name fallback.
     */
    TAssetRef(std::string AssetName, std::string AssetId)
        : TAssetRef()
    {
        m_assetName = TrimCopy(AssetName);
        m_assetId = TrimCopy(AssetId);
    }

    /** @brief Access the stored asset name text. */
    [[nodiscard]] const std::string& GetAssetName() const
    {
        return m_assetName;
    }

    /**
     * @brief Mutably access the stored asset name text.
     * @return Borrowed mutable string reference.
     * @warning The caller is responsible for maintaining trimmed/valid content if mutating in place.
     */
    [[nodiscard]] std::string& EditAssetName()
    {
        return m_assetName;
    }

    /** @brief Access the stored asset-id text. */
    [[nodiscard]] const std::string& GetAssetId() const
    {
        return m_assetId;
    }

    /**
     * @brief Mutably access the stored asset-id text.
     * @return Borrowed mutable string reference.
     * @warning The caller is responsible for maintaining canonical id text if mutating in place.
     */
    [[nodiscard]] std::string& EditAssetId()
    {
        return m_assetId;
    }

    /**
     * @brief Replace both stored reference fields.
     * @param AssetName Asset catalog name.
     * @param AssetId Asset-id string.
     *
     * Both inputs are trimmed before storage.
     */
    void SetAsset(std::string AssetName, std::string AssetId)
    {
        m_assetName = TrimCopy(AssetName);
        m_assetId = TrimCopy(AssetId);
    }

    /** @brief Clear the stored asset name and id text. */
    void Clear()
    {
        m_assetName.clear();
        m_assetId.clear();
    }

    /**
     * @brief Query whether this reference carries no resolvable asset identity.
     * @return `true` when no explicit name or id is stored and no tag-default name exists.
     */
    [[nodiscard]] bool IsNull() const
    {
        return m_assetName.empty() && m_assetId.empty() && DefaultAssetName().empty();
    }

    /** @brief Compare stored name and id text for exact equality. */
    [[nodiscard]] bool operator==(const TAssetRef& Other) const
    {
        return m_assetName == Other.m_assetName && m_assetId == Other.m_assetId;
    }

    /** @brief Negated equality comparison. */
    [[nodiscard]] bool operator!=(const TAssetRef& Other) const
    {
        return !(*this == Other);
    }

    /**
     * @brief Resolve the effective asset name.
     * @return Explicit asset name when present, otherwise the tag-default name, otherwise an empty string.
     */
    [[nodiscard]] std::string ResolvedAssetName() const
    {
        if (!m_assetName.empty())
        {
            return m_assetName;
        }
        return DefaultAssetName();
    }

    /**
     * @brief Build a user-facing display label for editors and diagnostics.
     * @return Name plus short id suffix when both are available, otherwise the best available identifier.
     */
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

    /**
     * @brief Load the referenced asset through an explicit asset manager.
     * @param Manager Borrowed asset manager.
     * @param Params Optional type-erased load parameters forwarded to the asset factory.
     * @return Owning detached runtime object on success, or an error string.
     *
     * Resolution order is asset id first, then resolved asset name.
     */
    [[nodiscard]] TLoadResult Load(::SnAPI::AssetPipeline::AssetManager& Manager, const std::any& Params = {}) const
    {
        return LoadInternal(Manager, Params);
    }

    /**
     * @brief Load the referenced asset through the default asset manager.
     * @param Params Optional type-erased load parameters forwarded to the asset factory.
     * @return Owning detached runtime object on success, or an error string.
     * @warning Fails when no default asset-manager resolver is configured.
     */
    [[nodiscard]] TLoadResult Load(const std::any& Params = {}) const
    {
        auto* Manager = ResolveDefaultAssetManager();
        if (!Manager)
        {
            return std::unexpected("No default AssetManager resolver is configured");
        }
        return Load(*Manager, Params);
    }

    /**
     * @brief Acquire a shared asset-manager handle for non-node asset types.
     * @tparam U Deduced base type. Enabled only when `TBase` is not node-derived.
     * @param Manager Borrowed asset manager.
     * @param Params Optional type-erased load parameters.
     * @return Shared asset-manager handle on success, or an error string.
     *
     * This does not clone the asset. Lifetime is tied to the manager's shared asset storage.
     */
    template<typename U = TBase>
    [[nodiscard]] std::enable_if_t<!std::is_base_of_v<BaseNode, U>, std::expected<::SnAPI::AssetPipeline::AssetHandle<U>, std::string>>
    GetShared(::SnAPI::AssetPipeline::AssetManager& Manager, const std::any& Params = {}) const
    {
        const std::optional<::SnAPI::AssetPipeline::AssetId> ParsedId = ParsedAssetId();
        const std::string Name = ResolvedAssetName();

        if (ParsedId.has_value())
        {
            auto ById = Manager.GetById<U>(*ParsedId, Params);
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
            auto ByName = Manager.Get<U>(Name, Params);
            if (!ByName)
            {
                return std::unexpected(ByName.error());
            }
            return ByName;
        }

        return std::unexpected("AssetRef is empty");
    }

    /**
     * @brief Acquire a shared asset-manager handle for non-node asset types using the default manager.
     * @tparam U Deduced base type. Enabled only when `TBase` is not node-derived.
     * @param Params Optional type-erased load parameters.
     * @return Shared asset-manager handle on success, or an error string.
     */
    template<typename U = TBase>
    [[nodiscard]] std::enable_if_t<!std::is_base_of_v<BaseNode, U>, std::expected<::SnAPI::AssetPipeline::AssetHandle<U>, std::string>>
    GetShared(const std::any& Params = {}) const
    {
        auto* Manager = ResolveDefaultAssetManager();
        if (!Manager)
        {
            return std::unexpected("No default AssetManager resolver is configured");
        }
        return GetShared<U>(*Manager, Params);
    }

    /**
     * @brief Begin asynchronous asset loading through an explicit asset manager.
     * @param Manager Borrowed asset manager.
     * @param Priority Asset-pipeline load priority.
     * @param Params Optional type-erased load parameters.
     * @param Callback Optional completion callback.
     * @param Token Optional cancellation token.
     * @return Async load handle that can be used to track or cancel the request.
     *
     * For node-derived asset references, the completion path validates the loaded runtime type before
     * invoking the callback.
     */
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

    /**
     * @brief Begin asynchronous asset loading through the default asset manager.
     * @param Priority Asset-pipeline load priority.
     * @param Params Optional type-erased load parameters.
     * @param Callback Optional completion callback.
     * @param Token Optional cancellation token.
     * @return Async load handle, or an empty handle when no default manager is configured.
     *
     * When no default manager exists, the callback is invoked immediately with an error result.
     */
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

    /**
     * @brief Instantiate a node-derived asset directly into a world through an explicit asset manager.
     * @tparam U Deduced base type. Enabled only when `TBase` is node-derived.
     * @param Manager Borrowed asset manager.
     * @param WorldRef Borrowed destination world.
     * @param Parent Optional parent node. A null handle means the world root.
     * @param InstantiateAsCopy When `true`, regenerate object ids during deserialization.
     * @return Handle to the created root node on success, or an error string.
     *
     * The created node is validated against `TBase`. On mismatch, the newly created node is destroyed
     * before the error is returned.
     */
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

    /**
     * @brief Instantiate a node-derived asset directly into a world through the default asset manager.
     * @tparam U Deduced base type. Enabled only when `TBase` is node-derived.
     * @param WorldRef Borrowed destination world.
     * @param Parent Optional parent node.
     * @param InstantiateAsCopy When `true`, regenerate object ids during deserialization.
     * @return Handle to the created root node on success, or an error string.
     */
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

    /**
     * @brief Enumerate catalog assets that can be resolved as `TBase`.
     * @param Manager Borrowed asset manager.
     * @return Sorted list of compatible entries.
     *
     * For node-derived bases, this performs preview loads and reflection-based type compatibility checks.
     * For non-node assets, it performs preview loads of the requested asset type.
     *
     * @warning This may synchronously load many assets and is not suitable for hot paths.
     */
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

    /**
     * @brief Enumerate compatible assets using the default asset manager.
     * @return Sorted list of compatible entries, or an empty list when no default manager is configured.
     */
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
