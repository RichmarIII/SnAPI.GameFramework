#include "AuthoredAssetRegistry.h"
#include "AuthoredAssetJson.h"
#include "NodeAsset.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "IAssetCooker.h"
#include "IAssetImporter.h"
#include "IPipelineContext.h"

namespace SnAPI::GameFramework
{
namespace
{
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
        Item.LogicalName = Source.Uri;
        Item.AssetKind = Descriptor->CookedAssetKind;
        Item.Intermediate = std::move(PayloadResult.value());
        Item.Dependencies.push_back(Source);
        Item.Id = Ctx.MakeDeterministicAssetId(Item.LogicalName, {});
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
