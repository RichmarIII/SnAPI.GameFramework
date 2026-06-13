#include "AuthoredAssetRegistry.h"
#include "AuthoredAssetJson.h"
#include "NodeAsset.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <ranges>
#include <string>
#include <vector>

#include "IAssetCooker.h"
#include "IAssetImporter.h"
#include "IPipelineContext.h"
#include "StaticTypeId.h"
#include "TypeRegistry.h"

namespace SnAPI::GameFramework
{
namespace
{
using Json = nlohmann::ordered_json;

[[nodiscard]] TExpected<::SnAPI::AssetPipeline::TypedPayload> BuildSourceTypedPayload(
    const void* Object,
    const ::SnAPI::AssetPipeline::IPipelineContext& Context,
    const ::SnAPI::AssetPipeline::TypeId PayloadType)
{
    const auto* Serializer = Context.FindSerializer(PayloadType);
    if (!Serializer)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound,
                                         "Payload serializer is not registered for authored source asset type"));
    }

    std::vector<std::uint8_t> Bytes{};
    Serializer->SerializeToBytes(Object, Bytes);
    return ::SnAPI::AssetPipeline::TypedPayload(PayloadType, Serializer->GetSchemaVersion(), std::move(Bytes));
}

[[nodiscard]] std::string NormalizeExtension(std::string Extension)
{
    if (Extension.empty())
    {
        return Extension;
    }
    if (Extension.front() != '.')
    {
        Extension.insert(Extension.begin(), '.');
    }
    for (char& Ch : Extension)
    {
        if (Ch >= 'A' && Ch <= 'Z')
        {
            Ch = static_cast<char>(Ch - 'A' + 'a');
        }
    }
    return Extension;
}

[[nodiscard]] std::string NormalizeLogicalDependencyName(std::string Value)
{
    std::replace(Value.begin(), Value.end(), '\\', '/');
    Value = std::filesystem::path(Value).lexically_normal().generic_string();
    if (Value == ".")
    {
        return {};
    }
    if (!Value.empty() && Value.starts_with("./"))
    {
        Value.erase(0u, 2u);
    }
    return Value;
}

void AppendAssetDependency(std::vector<::SnAPI::AssetPipeline::AssetDependencyRef>& Dependencies,
                           ::SnAPI::AssetPipeline::AssetDependencyRef Dependency)
{
    Dependency.LogicalName = NormalizeLogicalDependencyName(std::move(Dependency.LogicalName));
    if (Dependency.Id.IsNull() && Dependency.LogicalName.empty())
    {
        return;
    }

    const auto Existing = std::ranges::find_if(
        Dependencies,
        [&](const ::SnAPI::AssetPipeline::AssetDependencyRef& Entry)
        {
            return Entry.Id == Dependency.Id &&
                   Entry.LogicalName == Dependency.LogicalName &&
                   Entry.Kind == Dependency.Kind;
        });
    if (Existing == Dependencies.end())
    {
        Dependencies.push_back(std::move(Dependency));
    }
}

void CollectJsonAssetDependencies(const Json& Value,
                                  const std::size_t Depth,
                                  const ::SnAPI::AssetPipeline::AssetId SelfId,
                                  const std::string_view SelfLogicalName,
                                  std::vector<::SnAPI::AssetPipeline::AssetDependencyRef>& OutDependencies)
{
    if (Value.is_object())
    {
        if (Depth > 0u)
        {
            std::string LogicalName{};
            if (const auto AssetNameIt = Value.find("AssetName");
                AssetNameIt != Value.end() && AssetNameIt->is_string())
            {
                LogicalName = AssetNameIt->get<std::string>();
            }
            else if (const auto AssetNameIt = Value.find("assetName");
                     AssetNameIt != Value.end() && AssetNameIt->is_string())
            {
                LogicalName = AssetNameIt->get<std::string>();
            }

            std::string AssetIdText{};
            if (const auto AssetIdIt = Value.find("AssetId");
                AssetIdIt != Value.end() && AssetIdIt->is_string())
            {
                AssetIdText = AssetIdIt->get<std::string>();
            }
            else if (const auto AssetIdIt = Value.find("assetId");
                     AssetIdIt != Value.end() && AssetIdIt->is_string())
            {
                AssetIdText = AssetIdIt->get<std::string>();
            }

            const std::string NormalizedLogicalName = NormalizeLogicalDependencyName(LogicalName);
            const auto ParsedId = ::SnAPI::AssetPipeline::AssetId::FromString(AssetIdText);
            const bool bIsSelf =
                (!SelfId.IsNull() && ParsedId == SelfId) ||
                (!SelfLogicalName.empty() && NormalizedLogicalName == SelfLogicalName);
            if (!bIsSelf && (!ParsedId.IsNull() || !NormalizedLogicalName.empty()))
            {
                AppendAssetDependency(
                    OutDependencies,
                    ::SnAPI::AssetPipeline::AssetDependencyRef{
                        .Id = ParsedId,
                        .LogicalName = NormalizedLogicalName,
                        .Kind = ::SnAPI::AssetPipeline::EAssetDependencyKind::Required,
                    });
            }
        }

        for (const auto& [_, Child] : Value.items())
        {
            CollectJsonAssetDependencies(Child, Depth + 1u, SelfId, SelfLogicalName, OutDependencies);
        }
        return;
    }

    if (Value.is_array())
    {
        for (const Json& Child : Value)
        {
            CollectJsonAssetDependencies(Child, Depth + 1u, SelfId, SelfLogicalName, OutDependencies);
        }
    }
}

class AuthoredAssetJsonImporter final : public ::SnAPI::AssetPipeline::IAssetImporter
{
public:
    const char* GetName() const override
    {
        return "SnAPI.GameFramework.AuthoredAssetJsonImporter";
    }

    bool CanImport(const ::SnAPI::AssetPipeline::SourceRef& Source) const override
    {
        AuthoredAssetRegistry::Instance().EnsureBuilt();
        return AuthoredAssetRegistry::Instance().FindByExtension(
            NormalizeExtension(std::filesystem::path(Source.Uri).extension().string())) != nullptr;
    }

    bool Import(const ::SnAPI::AssetPipeline::SourceRef& Source,
                std::vector<::SnAPI::AssetPipeline::ImportedItem>& OutItems,
                ::SnAPI::AssetPipeline::IPipelineContext& Ctx) override
    {
        AuthoredAssetRegistry::Instance().EnsureBuilt();
        const std::string Extension = NormalizeExtension(std::filesystem::path(Source.Uri).extension().string());
        const AuthoredAssetDescriptor* Descriptor = AuthoredAssetRegistry::Instance().FindByExtension(Extension);
        if (!Descriptor || !Descriptor->Type || !Descriptor->Type->RuntimeOps || !Descriptor->Type->RuntimeOps->DefaultConstruct ||
            !Descriptor->Type->RuntimeOps->Destroy)
        {
            return false;
        }

        std::vector<std::uint8_t> SourceBytes{};
        if (!Ctx.ReadAllBytes(Source.Uri, SourceBytes))
        {
            Ctx.LogError("AuthoredAsset importer failed to read source: %s", Source.Uri.c_str());
            return false;
        }

        const std::string SourceText(SourceBytes.begin(), SourceBytes.end());
        void* Storage = ::operator new(Descriptor->Type->Size, std::align_val_t(Descriptor->Type->Align));
        Descriptor->Type->RuntimeOps->DefaultConstruct(Storage);
        AuthoredAssetImportDiagnostics Diagnostics{};
        const Result LoadResult = DeserializeAuthoredAssetFromJson(Descriptor->Type->Id, SourceText, Storage, &Diagnostics);
        if (!LoadResult)
        {
            Descriptor->Type->RuntimeOps->Destroy(Storage);
            ::operator delete(Storage, std::align_val_t(Descriptor->Type->Align));
            Ctx.LogError("AuthoredAsset importer JSON parse error in %s: %s",
                         Source.Uri.c_str(),
                         LoadResult.error().Message.c_str());
            return false;
        }

        auto* AuthoredAsset = static_cast<IAsset*>(
            TypeRegistry::Instance().Cast(Descriptor->Type->Id, StaticTypeId<IAsset>(), Storage));
        const std::string LogicalName = Source.Uri;
        const ::SnAPI::AssetPipeline::AssetId AssetId = (AuthoredAsset && !AuthoredAsset->AssetId.IsNull())
            ? AuthoredAsset->AssetId
            : Ctx.MakeDeterministicAssetId(LogicalName, {});
        if (AuthoredAsset)
        {
            AuthoredAsset->SetPersistentIdentity(AssetId, LogicalName);
        }

        std::vector<::SnAPI::AssetPipeline::AssetDependencyRef> AssetDependencies{};
        Json SourceJson = Json::parse(SourceText, nullptr, false);
        if (!SourceJson.is_discarded())
        {
            CollectJsonAssetDependencies(SourceJson, 0u, AssetId, LogicalName, AssetDependencies);
        }

        auto PayloadResult = BuildSourceTypedPayload(Storage, Ctx, Descriptor->SourcePayloadType);
        Descriptor->Type->RuntimeOps->Destroy(Storage);
        ::operator delete(Storage, std::align_val_t(Descriptor->Type->Align));
        if (!PayloadResult)
        {
            Ctx.LogError("AuthoredAsset importer failed to build source payload for %s: %s",
                         Source.Uri.c_str(),
                         PayloadResult.error().Message.c_str());
            return false;
        }

        for (const std::string& Diagnostic : Diagnostics)
        {
            Ctx.LogWarn("AuthoredAsset importer warning in %s: %s", Source.Uri.c_str(), Diagnostic.c_str());
        }

        ::SnAPI::AssetPipeline::ImportedItem Item{};
        // AssetPipelineEngine::ProcessSource injects the resolved source logical name before cook.
        // Keep this as a unique fallback only for direct importer use outside that path.
        Item.LogicalName = LogicalName;
        Item.AssetKind = Descriptor->CookedAssetKind;
        Item.Intermediate = std::move(PayloadResult.value());
        Item.Dependencies.push_back(Source);
        Item.AssetDependencies = std::move(AssetDependencies);
        Item.Id = AssetId;
        OutItems.push_back(std::move(Item));
        return true;
    }
};

class AuthoredAssetPassThroughCooker final : public ::SnAPI::AssetPipeline::IAssetCooker
{
public:
    const char* GetName() const override
    {
        return "SnAPI.GameFramework.AuthoredAssetPassThroughCooker";
    }

    bool CanCook(const ::SnAPI::AssetPipeline::TypeId AssetKind,
                 const ::SnAPI::AssetPipeline::TypeId IntermediatePayloadType) const override
    {
        AuthoredAssetRegistry::Instance().EnsureBuilt();
        const AuthoredAssetDescriptor* Descriptor = AuthoredAssetRegistry::Instance().FindByCookedAssetKind(AssetKind);
        return Descriptor &&
               Descriptor->SourcePayloadType == IntermediatePayloadType &&
               Descriptor->CookedPayloadType == Descriptor->SourcePayloadType;
    }

    bool Cook(const ::SnAPI::AssetPipeline::CookRequest& Req,
              ::SnAPI::AssetPipeline::CookResult& Out,
              ::SnAPI::AssetPipeline::IPipelineContext&) override
    {
        Out.Cooked = Req.Intermediate;
        Out.Dependencies = Req.Dependencies;
        Out.AssetDependencies = Req.AssetDependencies;
        return true;
    }
};

class NodeSourceCooker final : public ::SnAPI::AssetPipeline::IAssetCooker
{
public:
    const char* GetName() const override
    {
        return "SnAPI.GameFramework.NodeSourceCooker";
    }

    bool CanCook(const ::SnAPI::AssetPipeline::TypeId AssetKind,
                 const ::SnAPI::AssetPipeline::TypeId IntermediatePayloadType) const override
    {
        return AssetKind == AssetKindNode() && IntermediatePayloadType == PayloadNodeSource();
    }

    bool Cook(const ::SnAPI::AssetPipeline::CookRequest& Req,
              ::SnAPI::AssetPipeline::CookResult& Out,
              ::SnAPI::AssetPipeline::IPipelineContext& Context) override
    {
        const auto* Serializer = Context.FindSerializer(PayloadNodeSource());
        if (!Serializer)
        {
            return false;
        }

        NodeAsset Asset{};
        if (!Serializer->DeserializeFromBytes(&Asset, Req.Intermediate.Bytes.data(), Req.Intermediate.Bytes.size()))
        {
            return false;
        }

        auto PayloadResult = CookNodeAsset(Asset);
        if (!PayloadResult)
        {
            Context.LogError("Failed to cook authored prefab asset: %s", PayloadResult.error().Message.c_str());
            return false;
        }

        std::vector<std::uint8_t> Bytes{};
        auto SerializeResult = SerializeNodePayload(*PayloadResult, Bytes);
        if (!SerializeResult)
        {
            Context.LogError("Failed to serialize cooked prefab payload: %s", SerializeResult.error().Message.c_str());
            return false;
        }

        Out.Cooked = ::SnAPI::AssetPipeline::TypedPayload(PayloadNode(), NodeSerializer::kSchemaVersion, std::move(Bytes));
        Out.Dependencies = Req.Dependencies;
        Out.AssetDependencies = Req.AssetDependencies;
        return true;
    }
};

class LevelSourceCooker final : public ::SnAPI::AssetPipeline::IAssetCooker
{
public:
    const char* GetName() const override
    {
        return "SnAPI.GameFramework.LevelSourceCooker";
    }

    bool CanCook(const ::SnAPI::AssetPipeline::TypeId AssetKind,
                 const ::SnAPI::AssetPipeline::TypeId IntermediatePayloadType) const override
    {
        return AssetKind == AssetKindLevel() && IntermediatePayloadType == PayloadLevelSource();
    }

    bool Cook(const ::SnAPI::AssetPipeline::CookRequest& Req,
              ::SnAPI::AssetPipeline::CookResult& Out,
              ::SnAPI::AssetPipeline::IPipelineContext& Context) override
    {
        const auto* Serializer = Context.FindSerializer(PayloadLevelSource());
        if (!Serializer)
        {
            return false;
        }

        LevelAsset Asset{};
        if (!Serializer->DeserializeFromBytes(&Asset, Req.Intermediate.Bytes.data(), Req.Intermediate.Bytes.size()))
        {
            return false;
        }

        auto PayloadResult = CookLevelAsset(Asset);
        if (!PayloadResult)
        {
            Context.LogError("Failed to cook authored level asset: %s", PayloadResult.error().Message.c_str());
            return false;
        }

        std::vector<std::uint8_t> Bytes{};
        auto SerializeResult = SerializeLevelPayload(*PayloadResult, Bytes);
        if (!SerializeResult)
        {
            Context.LogError("Failed to serialize cooked level payload: %s", SerializeResult.error().Message.c_str());
            return false;
        }

        Out.Cooked = ::SnAPI::AssetPipeline::TypedPayload(PayloadLevel(), LevelSerializer::kSchemaVersion, std::move(Bytes));
        Out.Dependencies = Req.Dependencies;
        Out.AssetDependencies = Req.AssetDependencies;
        return true;
    }
};

class WorldSourceCooker final : public ::SnAPI::AssetPipeline::IAssetCooker
{
public:
    const char* GetName() const override
    {
        return "SnAPI.GameFramework.WorldSourceCooker";
    }

    bool CanCook(const ::SnAPI::AssetPipeline::TypeId AssetKind,
                 const ::SnAPI::AssetPipeline::TypeId IntermediatePayloadType) const override
    {
        return AssetKind == AssetKindWorld() && IntermediatePayloadType == PayloadWorldSource();
    }

    bool Cook(const ::SnAPI::AssetPipeline::CookRequest& Req,
              ::SnAPI::AssetPipeline::CookResult& Out,
              ::SnAPI::AssetPipeline::IPipelineContext& Context) override
    {
        const auto* Serializer = Context.FindSerializer(PayloadWorldSource());
        if (!Serializer)
        {
            return false;
        }

        WorldAsset Asset{};
        if (!Serializer->DeserializeFromBytes(&Asset, Req.Intermediate.Bytes.data(), Req.Intermediate.Bytes.size()))
        {
            return false;
        }

        auto PayloadResult = CookWorldAsset(Asset);
        if (!PayloadResult)
        {
            Context.LogError("Failed to cook authored world asset: %s", PayloadResult.error().Message.c_str());
            return false;
        }

        std::vector<std::uint8_t> Bytes{};
        auto SerializeResult = SerializeWorldPayload(*PayloadResult, Bytes);
        if (!SerializeResult)
        {
            Context.LogError("Failed to serialize cooked world payload: %s", SerializeResult.error().Message.c_str());
            return false;
        }

        Out.Cooked = ::SnAPI::AssetPipeline::TypedPayload(PayloadWorld(), WorldSerializer::kSchemaVersion, std::move(Bytes));
        Out.Dependencies = Req.Dependencies;
        Out.AssetDependencies = Req.AssetDependencies;
        return true;
    }
};

} // namespace

std::unique_ptr<::SnAPI::AssetPipeline::IAssetImporter> CreateAuthoredAssetJsonImporter()
{
    return std::make_unique<AuthoredAssetJsonImporter>();
}

std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateAuthoredAssetPassThroughCooker()
{
    return std::make_unique<AuthoredAssetPassThroughCooker>();
}

std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateNodeSourceCooker()
{
    return std::make_unique<NodeSourceCooker>();
}

std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateLevelSourceCooker()
{
    return std::make_unique<LevelSourceCooker>();
}

std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateWorldSourceCooker()
{
    return std::make_unique<WorldSourceCooker>();
}

} // namespace SnAPI::GameFramework
