#include "AuthoredAssetRegistry.h"

#include <new>

#include "IAsset.h"
#include "TypeAutoRegistry.h"

namespace SnAPI::GameFramework
{
namespace
{
[[nodiscard]] std::string NormalizeExtension(std::string_view Extension)
{
    std::string Value(Extension);
    if (Value.empty())
    {
        return Value;
    }
    if (Value.front() != '.')
    {
        Value.insert(Value.begin(), '.');
    }
    for (char& Ch : Value)
    {
        if (Ch >= 'A' && Ch <= 'Z')
        {
            Ch = static_cast<char>(Ch - 'A' + 'a');
        }
    }
    return Value;
}
} // namespace

AuthoredAssetRegistry& AuthoredAssetRegistry::Instance()
{
    static AuthoredAssetRegistry Registry{};
    return Registry;
}

void AuthoredAssetRegistry::AddDiagnostic(std::string Message)
{
    m_diagnostics.push_back(std::move(Message));
}

void AuthoredAssetRegistry::EnsureBuilt()
{
    if (!m_built)
    {
        Build();
    }
}

const AuthoredAssetDescriptor* AuthoredAssetRegistry::FindByType(const TypeId& Type) const
{
    const auto It = m_indexByType.find(Type);
    if (It != m_indexByType.end())
    {
        return &m_assets[It->second];
    }

    const auto Match = std::find_if(m_assets.begin(), m_assets.end(), [&Type](const AuthoredAssetDescriptor& Descriptor) {
        return Descriptor.Type && Descriptor.Type->Id == Type;
    });
    return Match != m_assets.end() ? &*Match : nullptr;
}

const AuthoredAssetDescriptor* AuthoredAssetRegistry::FindByExtension(const std::string_view Extension) const
{
    const auto It = m_indexByExtension.find(NormalizeExtension(Extension));
    return It != m_indexByExtension.end() ? &m_assets[It->second] : nullptr;
}

const AuthoredAssetDescriptor* AuthoredAssetRegistry::FindBySourceAssetKind(const ::SnAPI::AssetPipeline::TypeId& AssetKind) const
{
    const auto It = m_indexBySourceAssetKind.find(AssetKind);
    return It != m_indexBySourceAssetKind.end() ? &m_assets[It->second] : nullptr;
}

const AuthoredAssetDescriptor* AuthoredAssetRegistry::FindBySourcePayloadType(const ::SnAPI::AssetPipeline::TypeId& PayloadType) const
{
    const auto It = m_indexBySourcePayloadType.find(PayloadType);
    return It != m_indexBySourcePayloadType.end() ? &m_assets[It->second] : nullptr;
}

const AuthoredAssetDescriptor* AuthoredAssetRegistry::FindByCookedAssetKind(const ::SnAPI::AssetPipeline::TypeId& AssetKind) const
{
    const auto It = m_indexByCookedAssetKind.find(AssetKind);
    return It != m_indexByCookedAssetKind.end() ? &m_assets[It->second] : nullptr;
}

const AuthoredAssetDescriptor* AuthoredAssetRegistry::FindByCookedPayloadType(const ::SnAPI::AssetPipeline::TypeId& PayloadType) const
{
    const auto It = m_indexByCookedPayloadType.find(PayloadType);
    return It != m_indexByCookedPayloadType.end() ? &m_assets[It->second] : nullptr;
}

void AuthoredAssetRegistry::Build()
{
    if (m_built)
    {
        return;
    }

    (void)TypeAutoRegistry::Instance().EnsureAll();

    m_assets.clear();
    m_diagnostics.clear();
    m_indexByType.clear();
    m_indexByExtension.clear();
    m_indexBySourceAssetKind.clear();
    m_indexBySourcePayloadType.clear();
    m_indexByCookedAssetKind.clear();
    m_indexByCookedPayloadType.clear();

    const TypeId AssetBaseType = StaticTypeId<IAsset>();
    std::vector<const TypeInfo*> Types = TypeRegistry::Instance().Derived(AssetBaseType);
    const TypeInfo* AssetBaseInfo = TypeRegistry::Instance().Find(AssetBaseType);
    if (!AssetBaseInfo)
    {
        AddDiagnostic("Authored asset base type is not registered: " + std::string(TTypeNameV<IAsset>));
        m_built = true;
        return;
    }

    if (AssetBaseInfo)
    {
        std::erase_if(Types, [AssetBaseInfo](const TypeInfo* Type) { return !Type || Type->Id == AssetBaseInfo->Id; });
    }

    for (const TypeInfo* Type : Types)
    {
        if (!Type)
        {
            AddDiagnostic("Authored asset registry encountered a null reflected type entry");
            continue;
        }
        if (Type->IsAbstract)
        {
            continue;
        }
        if (!TypeRegistry::Instance().IsA(Type->Id, AssetBaseType))
        {
            AddDiagnostic("Reflected authored asset type is not derived from IAsset: " + Type->Name);
            continue;
        }
        if (!Type->RuntimeOps)
        {
            AddDiagnostic("Authored asset type has no runtime ops: " + Type->Name);
            continue;
        }
        if (!Type->RuntimeOps->DefaultConstruct)
        {
            AddDiagnostic("Authored asset type is not default-constructible: " + Type->Name);
            continue;
        }
        if (!Type->RuntimeOps->Destroy)
        {
            AddDiagnostic("Authored asset type has no destroy op: " + Type->Name);
            continue;
        }

        void* Storage = ::operator new(Type->Size, std::align_val_t(Type->Align));
        Type->RuntimeOps->DefaultConstruct(Storage);
        const auto* Asset = static_cast<const IAsset*>(TypeRegistry::Instance().Cast(Type->Id, StaticTypeId<IAsset>(), Storage));
        if (!Asset)
        {
            AddDiagnostic("Authored asset type could not be cast to IAsset: " + Type->Name);
            Type->RuntimeOps->Destroy(Storage);
            ::operator delete(Storage, std::align_val_t(Type->Align));
            continue;
        }

        AuthoredAssetDescriptor Descriptor{};
        Descriptor.AssetType = Type->Id;
        Descriptor.Type = Type;
        Descriptor.DisplayName = std::string(Asset->DisplayName());
        Descriptor.Category = std::string(Asset->Category());
        Descriptor.FileExtension = NormalizeExtension(Asset->FileExtension());
        Descriptor.EditorMode = Asset->EditorMode();
        Descriptor.CanCreate = Asset->CanCreate();
        Descriptor.CanSave = Asset->CanSave();
        Descriptor.CanDelete = Asset->CanDelete();
        Descriptor.CanRename = Asset->CanRename();
        Descriptor.SourceAssetKind = Asset->SourceAssetKind();
        Descriptor.SourcePayloadType = Asset->SourcePayloadType();
        Descriptor.CookedAssetKind = Asset->CookedAssetKind();
        Descriptor.CookedPayloadType = Asset->CookedPayloadType();

        Type->RuntimeOps->Destroy(Storage);
        ::operator delete(Storage, std::align_val_t(Type->Align));

        if (Descriptor.DisplayName.empty())
        {
            AddDiagnostic("Authored asset type has an empty display name: " + Type->Name);
            continue;
        }
        if (Descriptor.FileExtension.empty())
        {
            AddDiagnostic("Authored asset type has an empty file extension: " + Type->Name);
            continue;
        }
        if (Descriptor.SourceAssetKind == ::SnAPI::AssetPipeline::TypeId{})
        {
            AddDiagnostic("Authored asset type has an empty source asset kind: " + Type->Name);
            continue;
        }
        if (Descriptor.SourcePayloadType == ::SnAPI::AssetPipeline::TypeId{})
        {
            AddDiagnostic("Authored asset type has an empty source payload type: " + Type->Name);
            continue;
        }
        if (Descriptor.CookedAssetKind == ::SnAPI::AssetPipeline::TypeId{})
        {
            AddDiagnostic("Authored asset type has an empty cooked asset kind: " + Type->Name);
            continue;
        }
        if (Descriptor.CookedPayloadType == ::SnAPI::AssetPipeline::TypeId{})
        {
            AddDiagnostic("Authored asset type has an empty cooked payload type: " + Type->Name);
            continue;
        }
        if (m_indexByExtension.contains(Descriptor.FileExtension))
        {
            AddDiagnostic("Duplicate authored asset file extension: " + Descriptor.FileExtension);
            continue;
        }
        if (m_indexByType.contains(Type->Id))
        {
            AddDiagnostic("Duplicate authored asset reflected type id: " + Type->Name);
            continue;
        }
        if (m_indexBySourceAssetKind.contains(Descriptor.SourceAssetKind))
        {
            AddDiagnostic("Duplicate authored source asset kind: " + Type->Name);
            continue;
        }
        if (m_indexBySourcePayloadType.contains(Descriptor.SourcePayloadType))
        {
            AddDiagnostic("Duplicate authored source payload type: " + Type->Name);
            continue;
        }
        if (m_indexByCookedAssetKind.contains(Descriptor.CookedAssetKind))
        {
            AddDiagnostic("Duplicate authored cooked asset kind: " + Type->Name);
            continue;
        }
        if (m_indexByCookedPayloadType.contains(Descriptor.CookedPayloadType))
        {
            AddDiagnostic("Duplicate authored cooked payload type: " + Type->Name);
            continue;
        }

        const std::size_t Index = m_assets.size();
        m_indexByType.emplace(Type->Id, Index);
        m_indexByExtension.emplace(Descriptor.FileExtension, Index);
        m_indexBySourceAssetKind.emplace(Descriptor.SourceAssetKind, Index);
        m_indexBySourcePayloadType.emplace(Descriptor.SourcePayloadType, Index);
        m_indexByCookedAssetKind.emplace(Descriptor.CookedAssetKind, Index);
        m_indexByCookedPayloadType.emplace(Descriptor.CookedPayloadType, Index);
        m_assets.push_back(std::move(Descriptor));
    }

    std::sort(m_assets.begin(), m_assets.end(), [](const AuthoredAssetDescriptor& Left, const AuthoredAssetDescriptor& Right) {
        if (Left.Category != Right.Category)
        {
            return Left.Category < Right.Category;
        }
        return Left.DisplayName < Right.DisplayName;
    });

    m_indexByType.clear();
    m_indexByExtension.clear();
    m_indexBySourceAssetKind.clear();
    m_indexBySourcePayloadType.clear();
    m_indexByCookedAssetKind.clear();
    m_indexByCookedPayloadType.clear();
    for (std::size_t Index = 0; Index < m_assets.size(); ++Index)
    {
        if (m_assets[Index].Type)
        {
            m_assets[Index].AssetType = m_assets[Index].Type->Id;
            m_indexByType.emplace(m_assets[Index].Type->Id, Index);
        }
        m_indexByExtension.emplace(m_assets[Index].FileExtension, Index);
        m_indexBySourceAssetKind.emplace(m_assets[Index].SourceAssetKind, Index);
        m_indexBySourcePayloadType.emplace(m_assets[Index].SourcePayloadType, Index);
        m_indexByCookedAssetKind.emplace(m_assets[Index].CookedAssetKind, Index);
        m_indexByCookedPayloadType.emplace(m_assets[Index].CookedPayloadType, Index);
    }

    m_built = true;
}

} // namespace SnAPI::GameFramework
