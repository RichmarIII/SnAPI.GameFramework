#pragma once

#include <algorithm>
#include <any>
#include <cctype>
#include <expected>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "AuthoredAssetLoading.h"
#include "AuthoredAssetRegistry.h"
#include "AssetPipeline.h"
#include "AssetManager.h"
#include "AssetPipelineFactories.h"
#include "AssetPipelineIds.h"
#include "BaseNode.h"
#include "Export.h"
#include "IWorld.h"
#include "PathResolver.h"
#include "ReflectionAnnotations.h"
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
SnType(SnTemplate)
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
     * @brief Load the referenced authored asset document through an explicit asset manager parameter for API symmetry.
     * @param Manager Borrowed asset manager. Unused for authored asset refs because document loading bypasses the runtime manager.
     * @param Params Optional type-erased load parameters. Ignored for authored asset refs.
     * @return Owning authored asset document on success, or an error string.
     */
    template<typename U = TBase>
    [[nodiscard]] std::enable_if_t<std::is_base_of_v<IAsset, U>, std::expected<std::unique_ptr<U>, std::string>>
    Load(::SnAPI::AssetPipeline::AssetManager& Manager, const std::any& Params = {}) const
    {
        (void)Manager;
        (void)Params;
        return LoadAsset<U>();
    }

    /**
     * @brief Load the referenced runtime asset through an explicit asset manager.
     * @param Manager Borrowed asset manager.
     * @param Params Optional type-erased load parameters forwarded to the asset factory.
     * @return Owning detached runtime object on success, or an error string.
     *
     * Resolution order is asset id first, then resolved asset name.
     */
    template<typename U = TBase>
    [[nodiscard]] std::enable_if_t<!std::is_base_of_v<IAsset, U>, TLoadResult>
    Load(::SnAPI::AssetPipeline::AssetManager& Manager, const std::any& Params = {}) const
    {
        return LoadInternal(Manager, Params);
    }

    /**
     * @brief Load the referenced authored asset document directly from JSON source.
     * @param Params Optional type-erased load parameters. Ignored for authored asset refs.
     * @return Owning authored asset document on success, or an error string.
     */
    template<typename U = TBase>
    [[nodiscard]] std::enable_if_t<std::is_base_of_v<IAsset, U>, std::expected<std::unique_ptr<U>, std::string>>
    Load(const std::any& Params = {}) const
    {
        (void)Params;
        return LoadAsset<U>();
    }

    /**
     * @brief Load the referenced runtime asset through the default asset manager.
     * @param Params Optional type-erased load parameters forwarded to the asset factory.
     * @return Owning detached runtime object on success, or an error string.
     * @warning Fails when no default asset-manager resolver is configured.
     */
    template<typename U = TBase>
    [[nodiscard]] std::enable_if_t<!std::is_base_of_v<IAsset, U>, TLoadResult>
    Load(const std::any& Params = {}) const
    {
        auto* Manager = ResolveDefaultAssetManager();
        if (!Manager)
        {
            return std::unexpected("No default AssetManager resolver is configured");
        }
        return Load(*Manager, Params);
    }

    template<typename U = TBase>
    [[nodiscard]] std::enable_if_t<std::is_base_of_v<IAsset, U>, std::expected<std::unique_ptr<U>, std::string>>
    LoadAsset() const
    {
        const std::string Name = ResolvedAssetName();
        if (Name.empty())
        {
            return std::unexpected("Authored asset reference requires an asset name");
        }

        auto LoadResult = ::SnAPI::GameFramework::LoadAuthoredAssetByName<U>(Name);
        if (!LoadResult)
        {
            return std::unexpected(LoadResult.error().Message);
        }
        return std::move(*LoadResult);
    }

    template<typename RuntimeT, typename U = TBase>
    [[nodiscard]] std::enable_if_t<std::is_base_of_v<IAsset, U>, std::expected<std::unique_ptr<RuntimeT>, std::string>>
    LoadRuntime(::SnAPI::AssetPipeline::AssetManager& Manager, const std::any& Params = {}) const
    {
        const std::optional<::SnAPI::AssetPipeline::AssetId> ParsedId = ParsedAssetId();
        const std::string Name = ResolvedAssetName();

        if (ParsedId.has_value())
        {
            auto ExistingById = Manager.FindAsset(*ParsedId);
            if (ExistingById.has_value())
            {
                return Manager.Load<RuntimeT>(*ParsedId, Params);
            }
        }

        if (!Name.empty())
        {
            auto ExistingByName = Manager.FindAsset(Name);
            if (ExistingByName.has_value())
            {
                return Manager.Load<RuntimeT>(ExistingByName->Id, Params);
            }
        }

        auto AssetResult = LoadAsset<U>();
        if (!AssetResult)
        {
            return std::unexpected(AssetResult.error());
        }

        auto PayloadResult = BuildAuthoredAssetSourcePayload(**AssetResult, Manager.GetRegistry());
        if (!PayloadResult)
        {
            return std::unexpected(PayloadResult.error().Message);
        }

        ::SnAPI::AssetPipeline::SourcePayloadRequest Request{};
        if (ParsedId.has_value())
        {
            Request.Id = *ParsedId;
        }
        Request.LogicalName = !Name.empty() ? Name : Request.Id.ToString();
        Request.AssetKind = (*AssetResult)->CookedAssetKind();
        Request.Intermediate = std::move(*PayloadResult);

        if (!Name.empty())
        {
            if (auto SourcePath = ResolveAuthoredAssetPath(Name); SourcePath)
            {
                Request.Dependencies.emplace_back(SourcePath->string());
            }
        }

        return Manager.LoadFromSourcePayload<RuntimeT>(Request, Params);
    }

    template<typename RuntimeT, typename U = TBase>
    [[nodiscard]] std::enable_if_t<std::is_base_of_v<IAsset, U>, std::expected<std::unique_ptr<RuntimeT>, std::string>>
    LoadRuntime(const std::any& Params = {}) const
    {
        auto* Manager = ResolveDefaultAssetManager();
        if (!Manager)
        {
            return std::unexpected("No default AssetManager resolver is configured");
        }
        return LoadRuntime<RuntimeT, U>(*Manager, Params);
    }

    template<typename RuntimeT, typename U = TBase>
    [[nodiscard]] std::enable_if_t<std::is_base_of_v<IAsset, U>, std::expected<std::shared_ptr<RuntimeT>, std::string>>
    LoadRuntimeShared(::SnAPI::AssetPipeline::AssetManager& Manager, const std::any& Params = {}) const
    {
        const std::optional<::SnAPI::AssetPipeline::AssetId> ParsedId = ParsedAssetId();
        const std::string Name = ResolvedAssetName();

        if (ParsedId.has_value())
        {
            auto ExistingById = Manager.FindAsset(*ParsedId);
            if (ExistingById.has_value())
            {
                return Manager.LoadShared<RuntimeT>(*ParsedId, Params);
            }
        }

        if (!Name.empty())
        {
            auto ExistingByName = Manager.FindAsset(Name);
            if (ExistingByName.has_value())
            {
                return Manager.LoadShared<RuntimeT>(ExistingByName->Id, Params);
            }
        }

        auto AssetResult = LoadAsset<U>();
        if (!AssetResult)
        {
            return std::unexpected(AssetResult.error());
        }

        auto PayloadResult = BuildAuthoredAssetSourcePayload(**AssetResult, Manager.GetRegistry());
        if (!PayloadResult)
        {
            return std::unexpected(PayloadResult.error().Message);
        }

        ::SnAPI::AssetPipeline::SourcePayloadRequest Request{};
        if (ParsedId.has_value())
        {
            Request.Id = *ParsedId;
        }
        Request.LogicalName = !Name.empty() ? Name : Request.Id.ToString();
        Request.AssetKind = (*AssetResult)->CookedAssetKind();
        Request.Intermediate = std::move(*PayloadResult);

        if (!Name.empty())
        {
            if (auto SourcePath = ResolveAuthoredAssetPath(Name); SourcePath)
            {
                Request.Dependencies.emplace_back(SourcePath->string());
            }
        }

        return Manager.LoadSharedFromSourcePayload<RuntimeT>(Request, Params);
    }

    template<typename RuntimeT, typename U = TBase>
    [[nodiscard]] std::enable_if_t<std::is_base_of_v<IAsset, U>, std::expected<std::shared_ptr<RuntimeT>, std::string>>
    LoadRuntimeShared(const std::any& Params = {}) const
    {
        auto* Manager = ResolveDefaultAssetManager();
        if (!Manager)
        {
            return std::unexpected("No default AssetManager resolver is configured");
        }
        return LoadRuntimeShared<RuntimeT, U>(*Manager, Params);
    }

    template<typename RuntimeT, typename U = TBase>
    [[nodiscard]] std::enable_if_t<std::is_base_of_v<IAsset, U>, std::expected<::SnAPI::AssetPipeline::AssetHandle<RuntimeT>, std::string>>
    GetRuntime(::SnAPI::AssetPipeline::AssetManager& Manager, const std::any& Params = {}) const
    {
        const std::optional<::SnAPI::AssetPipeline::AssetId> ParsedId = ParsedAssetId();
        const std::string Name = ResolvedAssetName();

        if (ParsedId.has_value())
        {
            auto ExistingById = Manager.FindAsset(*ParsedId);
            if (ExistingById.has_value())
            {
                return Manager.GetById<RuntimeT>(*ParsedId, Params);
            }
        }

        if (!Name.empty())
        {
            auto ExistingByName = Manager.FindAsset(Name);
            if (ExistingByName.has_value())
            {
                return Manager.GetById<RuntimeT>(ExistingByName->Id, Params);
            }
        }

        auto AssetResult = LoadAsset<U>();
        if (!AssetResult)
        {
            return std::unexpected(AssetResult.error());
        }

        auto PayloadResult = BuildAuthoredAssetSourcePayload(**AssetResult, Manager.GetRegistry());
        if (!PayloadResult)
        {
            return std::unexpected(PayloadResult.error().Message);
        }

        ::SnAPI::AssetPipeline::SourcePayloadRequest Request{};
        if (ParsedId.has_value())
        {
            Request.Id = *ParsedId;
        }
        Request.LogicalName = !Name.empty() ? Name : Request.Id.ToString();
        Request.AssetKind = (*AssetResult)->CookedAssetKind();
        Request.Intermediate = std::move(*PayloadResult);

        if (!Name.empty())
        {
            if (auto SourcePath = ResolveAuthoredAssetPath(Name); SourcePath)
            {
                Request.Dependencies.emplace_back(SourcePath->string());
            }
        }

        return Manager.GetFromSourcePayload<RuntimeT>(Request, Params);
    }

    template<typename RuntimeT, typename U = TBase>
    [[nodiscard]] std::enable_if_t<std::is_base_of_v<IAsset, U>, std::expected<::SnAPI::AssetPipeline::AssetHandle<RuntimeT>, std::string>>
    GetRuntime(const std::any& Params = {}) const
    {
        auto* Manager = ResolveDefaultAssetManager();
        if (!Manager)
        {
            return std::unexpected("No default AssetManager resolver is configured");
        }
        return GetRuntime<RuntimeT, U>(*Manager, Params);
    }

    template<typename RuntimeT, typename U = TBase>
    [[nodiscard]] std::enable_if_t<std::is_base_of_v<IAsset, U>, std::expected<std::shared_ptr<RuntimeT>, std::string>>
    GetRuntimeShared(::SnAPI::AssetPipeline::AssetManager& Manager, const std::any& Params = {}) const
    {
        const std::optional<::SnAPI::AssetPipeline::AssetId> ParsedId = ParsedAssetId();
        const std::string Name = ResolvedAssetName();

        if (ParsedId.has_value())
        {
            auto ExistingById = Manager.FindAsset(*ParsedId);
            if (ExistingById.has_value())
            {
                return Manager.GetSharedById<RuntimeT>(*ParsedId, Params);
            }
        }

        if (!Name.empty())
        {
            auto ExistingByName = Manager.FindAsset(Name);
            if (ExistingByName.has_value())
            {
                return Manager.GetSharedById<RuntimeT>(ExistingByName->Id, Params);
            }
        }

        auto AssetResult = LoadAsset<U>();
        if (!AssetResult)
        {
            return std::unexpected(AssetResult.error());
        }

        auto PayloadResult = BuildAuthoredAssetSourcePayload(**AssetResult, Manager.GetRegistry());
        if (!PayloadResult)
        {
            return std::unexpected(PayloadResult.error().Message);
        }

        ::SnAPI::AssetPipeline::SourcePayloadRequest Request{};
        if (ParsedId.has_value())
        {
            Request.Id = *ParsedId;
        }
        Request.LogicalName = !Name.empty() ? Name : Request.Id.ToString();
        Request.AssetKind = (*AssetResult)->CookedAssetKind();
        Request.Intermediate = std::move(*PayloadResult);

        if (!Name.empty())
        {
            if (auto SourcePath = ResolveAuthoredAssetPath(Name); SourcePath)
            {
                Request.Dependencies.emplace_back(SourcePath->string());
            }
        }

        return Manager.GetSharedFromSourcePayload<RuntimeT>(Request, Params);
    }

    template<typename RuntimeT, typename U = TBase>
    [[nodiscard]] std::enable_if_t<std::is_base_of_v<IAsset, U>, std::expected<std::shared_ptr<RuntimeT>, std::string>>
    GetRuntimeShared(const std::any& Params = {}) const
    {
        auto* Manager = ResolveDefaultAssetManager();
        if (!Manager)
        {
            return std::unexpected("No default AssetManager resolver is configured");
        }
        return GetRuntimeShared<RuntimeT, U>(*Manager, Params);
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
        NodeHandle& InOutParent,
        bool InstantiateAsCopy = true) const
    {
        NodeHandle Spawned{};
        NodeAssetLoadParams Params{};
        Params.TargetWorld = &WorldRef;
        Params.Parent = InOutParent;
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

        BaseNode* SpawnedNode = WorldRef.BorrowedNode(Spawned);
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
        ::SnAPI::AssetPipeline::AssetManager& Manager,
        IWorld& WorldRef,
        bool InstantiateAsCopy = true) const
    {
        NodeHandle RootParent{};
        return Instantiate(Manager, WorldRef, RootParent, InstantiateAsCopy);
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
        NodeHandle& InOutParent,
        bool InstantiateAsCopy = true) const
    {
        auto* Manager = ResolveDefaultAssetManager();
        if (!Manager)
        {
            return std::unexpected("No default AssetManager resolver is configured");
        }
        return Instantiate(*Manager, WorldRef, InOutParent, InstantiateAsCopy);
    }

    template<typename U = TBase>
    [[nodiscard]] std::enable_if_t<std::is_base_of_v<BaseNode, U>, std::expected<NodeHandle, std::string>> Instantiate(
        IWorld& WorldRef,
        bool InstantiateAsCopy = true) const
    {
        auto* Manager = ResolveDefaultAssetManager();
        if (!Manager)
        {
            return std::unexpected("No default AssetManager resolver is configured");
        }
        NodeHandle RootParent{};
        return Instantiate(*Manager, WorldRef, RootParent, InstantiateAsCopy);
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
        std::unordered_set<std::string> SeenAssetIds{};

        const auto TryAppendEntry = [&Manager, &Entries, &SeenAssetIds](const std::string& AssetName,
                                                                        const std::optional<::SnAPI::AssetPipeline::AssetId>& AssetId,
                                                                        const bool LoadByName,
                                                                        const std::filesystem::path* SourcePath = nullptr) {
            if (AssetName.empty() && !AssetId.has_value())
            {
                return;
            }

            const ::SnAPI::AssetPipeline::AssetId EffectiveId = AssetId.value_or(SourceAssetIdFromLogicalName(AssetName));
            const std::string EffectiveIdText = EffectiveId.ToString();
            if (!SeenAssetIds.insert(EffectiveIdText).second)
            {
                return;
            }

            if constexpr (std::is_base_of_v<BaseNode, TBase>)
            {
                if (LoadByName && SourcePath != nullptr)
                {
                    if (SourcePath->extension() != ".prefab")
                    {
                        return;
                    }

                    std::ifstream File(*SourcePath, std::ios::binary | std::ios::ate);
                    if (!File.is_open())
                    {
                        return;
                    }

                    const std::streamsize Size = File.tellg();
                    std::string SourceJson{};
                    if (Size > 0)
                    {
                        SourceJson.resize(static_cast<std::size_t>(Size));
                        File.seekg(0, std::ios::beg);
                        File.read(SourceJson.data(), Size);
                    }

                    const nlohmann::json SourceDocument = nlohmann::json::parse(SourceJson, nullptr, false);
                    if (SourceDocument.is_discarded())
                    {
                        return;
                    }

                    const nlohmann::json* AssetRoot = &SourceDocument;
                    if (SourceDocument.is_object())
                    {
                        const auto AssetIt = SourceDocument.find("Asset");
                        if (AssetIt != SourceDocument.end() && AssetIt->is_object())
                        {
                            AssetRoot = &(*AssetIt);
                        }
                    }

                    const auto NodesIt = AssetRoot->find("Nodes");
                    if (NodesIt == AssetRoot->end() || !NodesIt->is_array() || NodesIt->empty())
                    {
                        return;
                    }

                    if (!(*NodesIt)[0].is_object())
                    {
                        return;
                    }

                    const auto TypeIt = (*NodesIt)[0].find("Type");
                    if (TypeIt == (*NodesIt)[0].end() || !TypeIt->is_string())
                    {
                        return;
                    }

                    TypeId SourceNodeType{};
                    const std::string TypeText = TypeIt->get<std::string>();
                    if (const auto* TypeInfoPtr = TypeRegistry::Instance().FindByName(TypeText))
                    {
                        SourceNodeType = TypeInfoPtr->Id;
                    }
                    else if (const auto Parsed = Uuid::from_string(TypeText); Parsed)
                    {
                        SourceNodeType = *Parsed;
                    }

                    if (SourceNodeType == TypeId{} || !IsNodeCompatible(SourceNodeType))
                    {
                        return;
                    }
                }
                else
                {
                    auto Preview = LoadByName
                        ? Manager.Load<BaseNode>(AssetName)
                        : Manager.Load<BaseNode>(EffectiveId);
                    if (!Preview)
                    {
                        return;
                    }
                    if (!IsNodeCompatible((*Preview)->TypeKey()))
                    {
                        return;
                    }
                }
            }
            else if constexpr (std::is_base_of_v<IAsset, TBase>)
            {
                AuthoredAssetRegistry::Instance().EnsureBuilt();
                const AuthoredAssetDescriptor* Descriptor = AuthoredAssetRegistry::Instance().FindByType(StaticTypeId<TBase>());
                if (!Descriptor)
                {
                    return;
                }

                const auto MatchesExtension = [&Descriptor](const std::filesystem::path& Path) {
                    std::string AssetExtension = Path.extension().string();
                    std::string ExpectedExtension = Descriptor->FileExtension;
                    std::transform(AssetExtension.begin(), AssetExtension.end(), AssetExtension.begin(), [](unsigned char Ch) {
                        return static_cast<char>(std::tolower(Ch));
                    });
                    std::transform(ExpectedExtension.begin(), ExpectedExtension.end(), ExpectedExtension.begin(), [](unsigned char Ch) {
                        return static_cast<char>(std::tolower(Ch));
                    });
                    return AssetExtension == ExpectedExtension;
                };

                if (SourcePath != nullptr)
                {
                    if (!MatchesExtension(*SourcePath))
                    {
                        return;
                    }
                }
                else if (!MatchesExtension(std::filesystem::path(AssetName)))
                {
                    return;
                }
            }
            else
            {
                auto Preview = LoadByName
                    ? Manager.Load<TBase>(AssetName)
                    : Manager.Load<TBase>(EffectiveId);
                if (!Preview)
                {
                    return;
                }
            }

            const std::string EntryName = !AssetName.empty() ? AssetName : EffectiveIdText;
            TEntry Entry{};
            Entry.Name = EntryName;
            Entry.AssetId = EffectiveIdText;
            Entry.Label = EntryName + " [" + ShortAssetId(EffectiveIdText) + "]";
            Entries.push_back(std::move(Entry));
        };

        for (const auto& CatalogEntry : Manager.ListAssetCatalog())
        {
            const auto& Info = CatalogEntry.Info;

            if constexpr (std::is_base_of_v<BaseNode, TBase>)
            {
                if (Info.AssetKind != AssetKindNode())
                {
                    continue;
                }
            }
            else
            {
                // Compatibility is validated in TryAppendEntry.
            }

            const std::string AssetName = Info.Name.empty() ? Info.Id.ToString() : Info.Name;
            TryAppendEntry(AssetName, Info.Id, false);
        }

        const std::filesystem::path AssetRoot = SPathResolver::Instance().AssetRoot();
        std::error_code Error{};
        if (!AssetRoot.empty() && std::filesystem::exists(AssetRoot, Error) && !Error)
        {
            std::filesystem::recursive_directory_iterator It(
                AssetRoot,
                std::filesystem::directory_options::skip_permission_denied,
                Error);
            const std::filesystem::recursive_directory_iterator End{};
            for (; !Error && It != End; It.increment(Error))
            {
                const std::filesystem::directory_entry& EntryRef = *It;
                if (!EntryRef.is_regular_file())
                {
                    continue;
                }

                std::filesystem::path SourcePath = EntryRef.path().lexically_normal();
                if (SourcePath.extension() == ".snpak")
                {
                    continue;
                }

                std::error_code RelativeError{};
                std::filesystem::path Relative = std::filesystem::relative(SourcePath, AssetRoot, RelativeError);
                if (RelativeError)
                {
                    Relative = SourcePath.filename();
                }

                std::string LogicalName = Relative.generic_string();
                std::replace(LogicalName.begin(), LogicalName.end(), '\\', '/');
                while (LogicalName.rfind("./", 0) == 0)
                {
                    LogicalName.erase(0, 2u);
                }
                while (!LogicalName.empty() && LogicalName.front() == '/')
                {
                    LogicalName.erase(LogicalName.begin());
                }
                if (LogicalName.empty())
                {
                    continue;
                }

                TryAppendEntry(LogicalName, std::nullopt, true, &SourcePath);
            }
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

    SnField(
        SnKey("AssetName"),
        SnGetter(EditAssetName),
        SnConstGetter(GetAssetName)
    )
    std::string m_assetName{};
    SnField(
        SnKey("AssetId"),
        SnGetter(EditAssetId),
        SnConstGetter(GetAssetId)
    )
    std::string m_assetId{};
};

template<typename TBase, typename TNameTag>
struct TEditorValueFamilyTraits<TAssetRef<TBase, TNameTag>>
{
    static constexpr EEditorValueFamily Family = EEditorValueFamily::AssetRef;

    static std::string Trimmed(std::string_view Text)
    {
        std::string Result(Text);
        auto NotSpace = [](unsigned char Character) {
            return std::isspace(Character) == 0;
        };

        Result.erase(Result.begin(), std::find_if(Result.begin(), Result.end(), NotSpace));
        Result.erase(std::find_if(Result.rbegin(), Result.rend(), NotSpace).base(), Result.end());
        return Result;
    }

    static TypeId TargetType()
    {
        if constexpr (CHasReflectedTypeName<TBase>)
        {
            return StaticTypeId<TBase>();
        }
        else
        {
            return {};
        }
    }

    static void PopulateOptions(std::vector<std::string>& OutOptions)
    {
        const auto Entries = TAssetRef<TBase, TNameTag>::EnumerateCompatibleAssets();
        OutOptions.reserve(OutOptions.size() + Entries.size() + 1);
        OutOptions.emplace_back("<None>");
        for (const auto& Entry : Entries)
        {
            OutOptions.push_back(Entry.Label);
        }
    }

    static bool ReadSelectionLabel(const void* Value, std::string& OutText)
    {
        const auto* AssetRefValue = static_cast<const TAssetRef<TBase, TNameTag>*>(Value);
        if (!AssetRefValue)
        {
            return false;
        }

        const std::string SelectedName = AssetRefValue->ResolvedAssetName();
        const std::string SelectedId = Trimmed(AssetRefValue->GetAssetId());
        if (SelectedName.empty() && SelectedId.empty())
        {
            OutText = "<None>";
            return true;
        }

        const auto Entries = TAssetRef<TBase, TNameTag>::EnumerateCompatibleAssets();
        const auto It = std::ranges::find_if(Entries, [&](const typename TAssetRef<TBase, TNameTag>::TEntry& Entry) {
            if (!SelectedId.empty())
            {
                return Entry.AssetId == SelectedId;
            }
            return Entry.Name == SelectedName;
        });

        OutText = (It != Entries.end()) ? It->Label : AssetRefValue->DisplayLabel();
        return true;
    }

    static bool WriteSelection(void* Value, std::string_view Selected)
    {
        auto* AssetRefValue = static_cast<TAssetRef<TBase, TNameTag>*>(Value);
        if (!AssetRefValue)
        {
            return false;
        }

        const std::string SelectedText = Trimmed(Selected);
        if (SelectedText.empty() || SelectedText == "<None>")
        {
            AssetRefValue->Clear();
            return true;
        }

        const auto Entries = TAssetRef<TBase, TNameTag>::EnumerateCompatibleAssets();
        const auto It = std::ranges::find_if(Entries, [&](const typename TAssetRef<TBase, TNameTag>::TEntry& Entry) {
            return Entry.Label == SelectedText || Entry.Name == SelectedText || Entry.AssetId == SelectedText;
        });

        if (It == Entries.end())
        {
            AssetRefValue->SetAsset(SelectedText, {});
            return true;
        }

        AssetRefValue->SetAsset(It->Name, It->AssetId);
        return true;
    }

    static const EditorValueAdapterOps& Adapter()
    {
        static const EditorValueAdapterOps Ops{
            .PopulateOptions = &PopulateOptions,
            .ReadSelectionLabel = &ReadSelectionLabel,
            .WriteSelection = &WriteSelection,
        };
        return Ops;
    }
};

} // namespace SnAPI::GameFramework
