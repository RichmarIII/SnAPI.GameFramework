#include "Editor/EditorAssetService.h"

#include "BaseNode.h"
#include "AssetPackReader.h"
#include "AssetPackWriter.h"
#include "AssetPipelineFactories.h"
#include "AssetPipelineIds.h"
#include "ComponentStorage.h"
#include "GameRuntime.h"
#include "Level.h"
#include "NodeGraph.h"
#include "Serialization.h"
#include "TransformComponent.h"
#include "TypeRegistry.h"
#include "World.h"
#if defined(SNAPI_GF_ENABLE_RENDERER)
#include "StaticMeshComponent.h"
#endif
#if defined(SNAPI_GF_ENABLE_PHYSICS)
#include "ColliderComponent.h"
#include "CollisionFilters.h"
#include "RigidBodyComponent.h"
#endif

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <unordered_set>

namespace SnAPI::GameFramework::Editor
{
namespace
{
[[nodiscard]] std::string ToLowerCopy(std::string_view Text)
{
    std::string Output(Text);
    std::transform(Output.begin(), Output.end(), Output.begin(), [](const unsigned char Character) {
        return static_cast<char>(std::tolower(Character));
    });
    return Output;
}

[[nodiscard]] std::filesystem::path ResolveAppDataRootPath()
{
#if defined(_WIN32)
    if (const char* LocalAppData = std::getenv("LOCALAPPDATA"))
    {
        return std::filesystem::path(LocalAppData);
    }
    if (const char* RoamingAppData = std::getenv("APPDATA"))
    {
        return std::filesystem::path(RoamingAppData);
    }
    return {};
#elif defined(__APPLE__)
    if (const char* Home = std::getenv("HOME"))
    {
        return std::filesystem::path(Home) / "Library" / "Application Support";
    }
    return {};
#else
    if (const char* XdgDataHome = std::getenv("XDG_DATA_HOME"))
    {
        return std::filesystem::path(XdgDataHome);
    }
    if (const char* Home = std::getenv("HOME"))
    {
        return std::filesystem::path(Home) / ".local" / "share";
    }
    return {};
#endif
}

[[nodiscard]] std::filesystem::path EditorDefaultShapeAssetDirectory()
{
    const std::filesystem::path AppDataRoot = ResolveAppDataRootPath();
    if (AppDataRoot.empty())
    {
        return {};
    }
    return AppDataRoot / "SnAPI" / "GameFramework" / "Editor" / "Assets";
}

struct DefaultShapePackSpec
{
    const char* PackFileName = "";
    const char* AssetName = "";
    const char* PrimitiveMeshPathToken = "primitive://box";
#if defined(SNAPI_GF_ENABLE_PHYSICS)
    SnAPI::Physics::EShapeType ColliderShape = SnAPI::Physics::EShapeType::Box;
    Vec3 ColliderHalfExtent{0.5f, 0.5f, 0.5f};
    float ColliderRadius = 0.5f;
    float ColliderHalfHeight = 0.5f;
    float ColliderFriction = 0.8f;
    float ColliderRestitution = 0.05f;
#endif
};

[[nodiscard]] std::array<DefaultShapePackSpec, 4> DefaultShapePackSpecs()
{
    std::array<DefaultShapePackSpec, 4> Specs{};

    Specs[0].PackFileName = "BoxShape.snpak";
    Specs[0].AssetName = "BoxShape";
    Specs[0].PrimitiveMeshPathToken = "primitive://box";
#if defined(SNAPI_GF_ENABLE_PHYSICS)
    Specs[0].ColliderShape = SnAPI::Physics::EShapeType::Box;
    Specs[0].ColliderHalfExtent = Vec3(0.5f, 0.5f, 0.5f);
#endif

    Specs[1].PackFileName = "SphereShape.snpak";
    Specs[1].AssetName = "SphereShape";
    Specs[1].PrimitiveMeshPathToken = "primitive://sphere";
#if defined(SNAPI_GF_ENABLE_PHYSICS)
    Specs[1].ColliderShape = SnAPI::Physics::EShapeType::Sphere;
    Specs[1].ColliderRadius = 0.5f;
#endif

    Specs[2].PackFileName = "ConeShape.snpak";
    Specs[2].AssetName = "ConeShape";
    Specs[2].PrimitiveMeshPathToken = "primitive://cone";
#if defined(SNAPI_GF_ENABLE_PHYSICS)
    Specs[2].ColliderShape = SnAPI::Physics::EShapeType::Capsule;
    Specs[2].ColliderRadius = 0.4f;
    Specs[2].ColliderHalfHeight = 0.5f;
#endif

    Specs[3].PackFileName = "PyramidShape.snpak";
    Specs[3].AssetName = "PyramidShape";
    Specs[3].PrimitiveMeshPathToken = "primitive://pyramid";
#if defined(SNAPI_GF_ENABLE_PHYSICS)
    Specs[3].ColliderShape = SnAPI::Physics::EShapeType::Box;
    Specs[3].ColliderHalfExtent = Vec3(0.5f, 0.5f, 0.5f);
#endif

    return Specs;
}

[[nodiscard]] std::expected<::SnAPI::AssetPipeline::AssetPackEntry, std::string> BuildDefaultShapePackEntry(
    const DefaultShapePackSpec& Spec)
{
    NodeGraph Graph(std::string(Spec.AssetName) + ".Graph");
    auto NodeResult = Graph.CreateNode(std::string(Spec.AssetName));
    if (!NodeResult)
    {
        return std::unexpected(NodeResult.error().Message);
    }

    auto NodeHandle = NodeResult.value();
    auto* Node = NodeHandle.Borrowed();
    if (!Node)
    {
        return std::unexpected("Failed to resolve created node for default shape asset");
    }

    auto TransformResult = Node->Add<TransformComponent>();
    if (!TransformResult)
    {
        return std::unexpected(TransformResult.error().Message);
    }
    TransformResult->Position = Vec3(0.0f, 0.0f, 0.0f);
    TransformResult->Rotation = Quat::Identity();
    TransformResult->Scale = Vec3(1.0f, 1.0f, 1.0f);

#if defined(SNAPI_GF_ENABLE_RENDERER)
    auto MeshResult = Node->Add<StaticMeshComponent>();
    if (!MeshResult)
    {
        return std::unexpected(MeshResult.error().Message);
    }

    auto& MeshSettings = MeshResult->EditSettings();
    MeshSettings.MeshPath = Spec.PrimitiveMeshPathToken;
    MeshSettings.Visible = true;
    MeshSettings.CastShadows = true;
    MeshSettings.SyncFromTransform = true;
    MeshSettings.RegisterWithRenderer = true;
#endif

#if defined(SNAPI_GF_ENABLE_PHYSICS)
    auto ColliderResult = Node->Add<ColliderComponent>();
    if (!ColliderResult)
    {
        return std::unexpected(ColliderResult.error().Message);
    }

    auto& ColliderSettings = ColliderResult->EditSettings();
    ColliderSettings.Shape = Spec.ColliderShape;
    ColliderSettings.HalfExtent = Spec.ColliderHalfExtent;
    ColliderSettings.Radius = Spec.ColliderRadius;
    ColliderSettings.HalfHeight = Spec.ColliderHalfHeight;
    ColliderSettings.Friction = Spec.ColliderFriction;
    ColliderSettings.Restitution = Spec.ColliderRestitution;
    ColliderSettings.Density = 1.0f;
    ColliderSettings.Layer = CollisionLayerFlags(ECollisionFilterBits::WorldStatic);
    ColliderSettings.Mask = kCollisionMaskAll;
    ColliderSettings.IsTrigger = false;

    RigidBodyComponent::Settings BodySettings{};
    BodySettings.BodyType = SnAPI::Physics::EBodyType::Static;
    BodySettings.Mass = 1.0f;
    BodySettings.EnableCcd = false;
    BodySettings.SyncFromPhysics = false;
    BodySettings.SyncToPhysics = true;
    BodySettings.StartActive = true;
    BodySettings.EnableRenderInterpolation = false;
    BodySettings.AutoDeactivateWhenSleeping = true;

    auto RigidBodyResult = Node->Add<RigidBodyComponent>(BodySettings);
    if (!RigidBodyResult)
    {
        return std::unexpected(RigidBodyResult.error().Message);
    }
#endif

    auto GraphPayloadResult = NodeGraphSerializer::Serialize(Graph);
    if (!GraphPayloadResult)
    {
        return std::unexpected(GraphPayloadResult.error().Message);
    }

    std::vector<uint8_t> CookedBytes{};
    auto SerializeResult = SerializeNodeGraphPayload(*GraphPayloadResult, CookedBytes);
    if (!SerializeResult)
    {
        return std::unexpected(SerializeResult.error().Message);
    }

    ::SnAPI::AssetPipeline::AssetPackEntry Entry{};
    Entry.Id = AssetPipelineAssetIdFromName(std::string("SnAPI.Editor.DefaultShape.") + Spec.AssetName);
    Entry.AssetKind = AssetKindNodeGraph();
    Entry.Name = Spec.AssetName;
    Entry.VariantKey = "default";
    Entry.Cooked = ::SnAPI::AssetPipeline::TypedPayload(PayloadNodeGraph(),
                                                         NodeGraphSerializer::kSchemaVersion,
                                                         std::move(CookedBytes));
    return Entry;
}

[[nodiscard]] std::expected<std::size_t, std::string> EnsureDefaultShapePacks(const std::filesystem::path& PackDirectory)
{
    if (PackDirectory.empty())
    {
        return std::unexpected("Unable to resolve appdata path for editor default assets");
    }

    std::error_code Error{};
    std::filesystem::create_directories(PackDirectory, Error);
    if (Error)
    {
        return std::unexpected("Failed to create default asset directory: " + Error.message());
    }

    std::size_t CreatedCount = 0;
    for (const auto& Spec : DefaultShapePackSpecs())
    {
        const std::filesystem::path PackPath = PackDirectory / Spec.PackFileName;

        Error.clear();
        if (std::filesystem::exists(PackPath, Error) && !Error)
        {
            continue;
        }

        auto EntryResult = BuildDefaultShapePackEntry(Spec);
        if (!EntryResult)
        {
            return std::unexpected("Failed to build default shape '" + std::string(Spec.AssetName) + "': " + EntryResult.error());
        }

        ::SnAPI::AssetPipeline::AssetPackWriter Writer{};
        Writer.AddAsset(std::move(*EntryResult));
        auto WriteResult = Writer.Write(PackPath.string());
        if (!WriteResult)
        {
            return std::unexpected("Failed to write pack '" + PackPath.string() + "': " + WriteResult.error());
        }

        ++CreatedCount;
    }

    return CreatedCount;
}

[[nodiscard]] std::size_t CountNodes(const NodeGraph& Graph)
{
    std::size_t NodeCount = 0;
    Graph.NodePool().ForEach([&NodeCount](const NodeHandle&, BaseNode&) { ++NodeCount; });
    return NodeCount;
}

[[nodiscard]] std::size_t CountComponents(const NodeGraph& Graph)
{
    std::size_t ComponentCount = 0;
    Graph.NodePool().ForEach([&ComponentCount](const NodeHandle&, BaseNode& Node) {
        ComponentCount += Node.ComponentTypes().size();
    });
    return ComponentCount;
}

void AppendUniquePath(std::vector<std::string>& Paths,
                      std::unordered_set<std::string>& SeenPaths,
                      const std::filesystem::path& InputPath)
{
    std::error_code Error{};
    std::filesystem::path PathToUse = InputPath;
    if (PathToUse.empty())
    {
        return;
    }

    const std::filesystem::path Canonical = std::filesystem::weakly_canonical(PathToUse, Error);
    if (!Error)
    {
        PathToUse = Canonical;
    }
    else
    {
        Error.clear();
        const auto Absolute = std::filesystem::absolute(PathToUse, Error);
        if (!Error)
        {
            PathToUse = Absolute;
        }
    }

    Error.clear();
    if (!std::filesystem::exists(PathToUse, Error) || Error)
    {
        return;
    }

    Error.clear();
    if (!std::filesystem::is_directory(PathToUse, Error) || Error)
    {
        return;
    }

    const std::string Key = ToLowerCopy(PathToUse.generic_string());
    if (!SeenPaths.insert(Key).second)
    {
        return;
    }

    Paths.push_back(PathToUse.string());
}

[[nodiscard]] std::string NormalizeAssetLogicalName(std::string_view RawName)
{
    std::string Name(RawName);
    std::replace(Name.begin(), Name.end(), '\\', '/');

    const auto IsWhitespace = [](const unsigned char Character) {
        return std::isspace(Character) != 0;
    };

    while (!Name.empty() && IsWhitespace(static_cast<unsigned char>(Name.front())))
    {
        Name.erase(Name.begin());
    }
    while (!Name.empty() && IsWhitespace(static_cast<unsigned char>(Name.back())))
    {
        Name.pop_back();
    }

    while (Name.find("//") != std::string::npos)
    {
        Name.replace(Name.find("//"), 2u, "/");
    }

    while (Name.rfind("./", 0) == 0)
    {
        Name.erase(0, 2);
    }

    while (!Name.empty() && Name.front() == '/')
    {
        Name.erase(Name.begin());
    }

    if (Name == ".")
    {
        Name.clear();
    }

    return Name;
}

[[nodiscard]] std::string ShortTypeName(std::string_view QualifiedTypeName)
{
    const std::size_t Delimiter = QualifiedTypeName.rfind("::");
    if (Delimiter == std::string_view::npos)
    {
        return std::string(QualifiedTypeName);
    }
    return std::string(QualifiedTypeName.substr(Delimiter + 2));
}

[[nodiscard]] std::string MakeUniqueLogicalName(::SnAPI::AssetPipeline::AssetManager& AssetManagerRef,
                                                const std::string& Prefix,
                                                std::string BaseName)
{
    BaseName = NormalizeAssetLogicalName(BaseName);
    if (BaseName.empty())
    {
        BaseName = "Asset";
    }

    std::string CandidateBase = NormalizeAssetLogicalName(Prefix + "/" + BaseName);
    if (CandidateBase.empty())
    {
        CandidateBase = BaseName;
    }

    std::string Candidate = CandidateBase;
    std::size_t SuffixIndex = 1;
    while (AssetManagerRef.FindAsset(Candidate).has_value())
    {
        Candidate = CandidateBase + "_" + std::to_string(SuffixIndex++);
    }

    return Candidate;
}

TExpected<void> CloneNodeComponents(const BaseNode& SourceNode, NodeHandle DestinationHandle, NodeGraph& DestinationGraph)
{
    NodeGraph* SourceGraph = SourceNode.OwnerGraph();
    if (!SourceGraph)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Source node is not owned by a graph"));
    }

    NodeHandle SourceHandle = SourceNode.Handle();
    if (SourceHandle.IsNull())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Source node handle is null"));
    }
    if (SourceGraph->NodePool().Borrowed(SourceHandle) == nullptr)
    {
        if (const auto RefreshedHandle = SourceGraph->NodeHandleByIdSlow(SourceNode.Id()); RefreshedHandle)
        {
            SourceHandle = *RefreshedHandle;
        }
    }

    const auto& ComponentTypes = SourceNode.ComponentTypes();
    const auto& ComponentStorages = SourceNode.ComponentStorages();
    for (std::size_t ComponentIndex = 0; ComponentIndex < ComponentTypes.size(); ++ComponentIndex)
    {
        const TypeId& ComponentType = ComponentTypes[ComponentIndex];
        IComponentStorage* Storage =
            (ComponentIndex < ComponentStorages.size()) ? ComponentStorages[ComponentIndex] : nullptr;

        if ((!Storage || Storage->TypeKey() != ComponentType) && !ComponentStorages.empty())
        {
            auto MatchIt = std::find_if(ComponentStorages.begin(),
                                        ComponentStorages.end(),
                                        [&](IComponentStorage* Candidate) {
                                            return Candidate && Candidate->TypeKey() == ComponentType;
                                        });
            Storage = (MatchIt != ComponentStorages.end()) ? *MatchIt : nullptr;
        }

        if (!Storage)
        {
            const TypeInfo* ComponentInfo = TypeRegistry::Instance().Find(ComponentType);
            const std::string ComponentLabel = ComponentInfo ? ShortTypeName(ComponentInfo->Name) : std::string("UnknownComponent");
            return std::unexpected(
                MakeError(EErrorCode::NotFound, "Missing component storage for '" + ComponentLabel + "' while cloning prefab"));
        }

        const void* SourceComponent = Storage->Borrowed(SourceHandle);
        if (!SourceComponent && !SourceHandle.Id.is_nil())
        {
            if (const auto RefreshedHandle = SourceGraph->NodeHandleByIdSlow(SourceHandle.Id); RefreshedHandle)
            {
                SourceHandle = *RefreshedHandle;
                SourceComponent = Storage->Borrowed(SourceHandle);
            }
        }
        if (!SourceComponent && !SourceHandle.Id.is_nil())
        {
            SourceComponent = Storage->Borrowed(NodeHandle{SourceHandle.Id});
        }
        if (!SourceComponent)
        {
            const TypeInfo* ComponentInfo = TypeRegistry::Instance().Find(ComponentType);
            const std::string ComponentLabel = ComponentInfo ? ShortTypeName(ComponentInfo->Name) : std::string("UnknownComponent");
            return std::unexpected(
                MakeError(EErrorCode::NotFound, "Failed to resolve source component '" + ComponentLabel + "' while cloning prefab"));
        }

        auto CreateResult = ComponentSerializationRegistry::Instance().Create(DestinationGraph, DestinationHandle, ComponentType);
        if (!CreateResult)
        {
            return std::unexpected(CreateResult.error());
        }
        void* DestinationComponent = CreateResult.value();
        if (!DestinationComponent)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to allocate destination component"));
        }

        std::vector<uint8_t> SerializedBytes{};
        const TSerializationContext SerializeContext{.Graph = SourceGraph};
        auto SerializeResult = ComponentSerializationRegistry::Instance().Serialize(
            ComponentType,
            SourceComponent,
            SerializedBytes,
            SerializeContext);
        if (!SerializeResult)
        {
            return std::unexpected(SerializeResult.error());
        }

        const TSerializationContext DeserializeContext{.Graph = &DestinationGraph};
        auto DeserializeResult = ComponentSerializationRegistry::Instance().Deserialize(
            ComponentType,
            DestinationComponent,
            SerializedBytes.data(),
            SerializedBytes.size(),
            DeserializeContext);
        if (!DeserializeResult)
        {
            return std::unexpected(DeserializeResult.error());
        }
    }

    return Ok();
}

[[nodiscard]] BaseNode* ResolveNodeForClone(NodeGraph* Graph, const NodeHandle Handle)
{
    if (Graph)
    {
        if (BaseNode* Node = Graph->NodePool().Borrowed(Handle))
        {
            return Node;
        }
        if (!Handle.Id.is_nil())
        {
            if (const auto RefreshedHandle = Graph->NodeHandleByIdSlow(Handle.Id); RefreshedHandle)
            {
                if (BaseNode* Node = Graph->NodePool().Borrowed(*RefreshedHandle))
                {
                    return Node;
                }
            }
        }
    }

    if (BaseNode* Node = Handle.Borrowed())
    {
        return Node;
    }
    return Handle.BorrowedSlowByUuid();
}

TExpected<NodeHandle> CloneNodeSubtree(const BaseNode& SourceNode, NodeGraph& DestinationGraph, const NodeHandle DestinationParent)
{
    std::string NodeName = SourceNode.Name();
    if (NodeName.empty())
    {
        if (const TypeInfo* Info = TypeRegistry::Instance().Find(SourceNode.TypeKey()))
        {
            NodeName = ShortTypeName(Info->Name);
        }
        else
        {
            NodeName = "Node";
        }
    }

    auto CreateResult = DestinationGraph.CreateNode(SourceNode.TypeKey(), NodeName);
    if (!CreateResult)
    {
        return std::unexpected(CreateResult.error());
    }

    NodeHandle CreatedHandle = CreateResult.value();
    BaseNode* CreatedNode = CreatedHandle.Borrowed();
    if (!CreatedNode)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to resolve cloned node"));
    }

    auto CopyComponentsResult = CloneNodeComponents(SourceNode, CreatedHandle, DestinationGraph);
    if (!CopyComponentsResult)
    {
        return std::unexpected(CopyComponentsResult.error());
    }

    if (!DestinationParent.IsNull())
    {
        auto AttachResult = DestinationGraph.AttachChild(DestinationParent, CreatedHandle);
        if (!AttachResult)
        {
            return std::unexpected(AttachResult.error());
        }
    }

    NodeGraph* SourceGraph = SourceNode.OwnerGraph();
    for (const NodeHandle ChildHandle : SourceNode.Children())
    {
        BaseNode* ChildNode = ResolveNodeForClone(SourceGraph, ChildHandle);
        if (!ChildNode)
        {
            continue;
        }

        auto ChildCloneResult = CloneNodeSubtree(*ChildNode, DestinationGraph, CreatedHandle);
        if (!ChildCloneResult)
        {
            return std::unexpected(ChildCloneResult.error());
        }
    }

    return CreatedHandle;
}

TExpected<void> CloneNodeChildrenAsRoots(const BaseNode& SourceNode, NodeGraph& DestinationGraph)
{
    NodeGraph* SourceGraph = SourceNode.OwnerGraph();
    for (const NodeHandle ChildHandle : SourceNode.Children())
    {
        BaseNode* ChildNode = ResolveNodeForClone(SourceGraph, ChildHandle);
        if (!ChildNode)
        {
            continue;
        }

        auto CloneResult = CloneNodeSubtree(*ChildNode, DestinationGraph, NodeHandle{});
        if (!CloneResult)
        {
            return std::unexpected(CloneResult.error());
        }
    }
    return Ok();
}
} // namespace

std::string_view EditorAssetService::Name() const
{
    return "EditorAssetService";
}

Result EditorAssetService::Initialize(EditorServiceContext& Context)
{
    (void)Context;
    m_assets.clear();
    m_assetIndexByKey.clear();
    m_assetRenameOverrides.clear();
    m_selectedAssetKey.clear();
    m_placementAssetKey.clear();
    m_previewSummary.clear();
    m_statusMessage.clear();

    std::size_t BootstrappedPackCount = 0;
    std::string BootstrapError{};
    const std::filesystem::path DefaultPackDirectory = EditorDefaultShapeAssetDirectory();
    if (!DefaultPackDirectory.empty())
    {
        auto BootstrapResult = EnsureDefaultShapePacks(DefaultPackDirectory);
        if (BootstrapResult)
        {
            BootstrappedPackCount = *BootstrapResult;
        }
        else
        {
            BootstrapError = BootstrapResult.error();
        }
    }

    ::SnAPI::AssetPipeline::AssetManagerConfig Config{};
    Config.PackSearchPaths = BuildPackSearchPaths();
    m_assetManager = std::make_unique<::SnAPI::AssetPipeline::AssetManager>(Config);

    RegisterAssetPipelinePayloads(m_assetManager->GetRegistry());
    RegisterAssetPipelineFactories(*m_assetManager);

    Result DiscoveryResult = RefreshDiscovery();
    if (!DiscoveryResult)
    {
        return DiscoveryResult;
    }

    if (!BootstrapError.empty())
    {
        m_statusMessage += " Default shape bootstrap failed: " + BootstrapError;
    }
    else if (BootstrappedPackCount > 0)
    {
        m_statusMessage += " Created " + std::to_string(BootstrappedPackCount) + " default shape pack(s).";
    }

    return Ok();
}

void EditorAssetService::Shutdown(EditorServiceContext& Context)
{
    (void)Context;
    m_assetManager.reset();
    m_assets.clear();
    m_assetIndexByKey.clear();
    m_assetRenameOverrides.clear();
    m_selectedAssetKey.clear();
    m_placementAssetKey.clear();
    m_previewSummary.clear();
    m_statusMessage.clear();
}

const EditorAssetService::DiscoveredAsset* EditorAssetService::SelectedAsset() const
{
    if (m_selectedAssetKey.empty())
    {
        return nullptr;
    }
    return FindAssetByKey(m_selectedAssetKey);
}

bool EditorAssetService::SelectAssetByKey(const std::string_view Key)
{
    const auto* Asset = FindAssetByKey(Key);
    if (!Asset)
    {
        return false;
    }

    m_selectedAssetKey = Asset->Key;
    if (!m_previewSummary.empty())
    {
        m_statusMessage = m_previewSummary;
    }
    else
    {
        m_statusMessage = "Selected asset: " + Asset->Name;
    }

    return true;
}

Result EditorAssetService::ArmPlacementByKey(const std::string_view Key)
{
    const auto* Asset = FindAssetByKey(Key);
    if (!Asset)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Asset was not found for placement"));
    }

    m_selectedAssetKey = Asset->Key;
    m_placementAssetKey = Asset->Key;
    m_statusMessage = "Placement armed: " + Asset->Name + ". Click inside the viewport to instantiate.";
    return Ok();
}

void EditorAssetService::ClearPlacement()
{
    m_placementAssetKey.clear();
}

Result EditorAssetService::RefreshDiscovery()
{
    if (!m_assetManager)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Asset manager is not initialized"));
    }

    const std::vector<::SnAPI::AssetPipeline::AssetCatalogEntry> RawAssets = m_assetManager->ListAssetCatalog();

    std::vector<DiscoveredAsset> NextAssets{};
    NextAssets.reserve(RawAssets.size());

    std::unordered_set<std::string> SeenKeys{};
    SeenKeys.reserve(RawAssets.size());
    std::unordered_set<::SnAPI::AssetPipeline::AssetId, ::SnAPI::AssetPipeline::UuidHash> SeenAssetIds{};
    SeenAssetIds.reserve(RawAssets.size());

    std::size_t RuntimeAssetCount = 0;
    std::size_t DirtyAssetCount = 0;

    for (const auto& CatalogEntry : RawAssets)
    {
        const auto& Info = CatalogEntry.Info;
        const std::string Key = Info.Id.ToString();
        if (!SeenKeys.insert(Key).second || !SeenAssetIds.insert(Info.Id).second)
        {
            continue;
        }

        DiscoveredAsset Entry{};
        Entry.Key = Key;
        Entry.Name = Info.Name.empty() ? Key : NormalizeAssetLogicalName(Info.Name);
        if (Entry.Name.empty())
        {
            Entry.Name = Key;
        }

        bool HasRenameOverride = false;
        if (const auto RenameOverrideIt = m_assetRenameOverrides.find(Info.Id); RenameOverrideIt != m_assetRenameOverrides.end())
        {
            const std::string OverriddenName = NormalizeAssetLogicalName(RenameOverrideIt->second);
            if (!OverriddenName.empty())
            {
                Entry.Name = OverriddenName;
                HasRenameOverride = (OverriddenName != NormalizeAssetLogicalName(Info.Name));
            }
        }

        Entry.TypeLabel = AssetKindToLabel(Info.AssetKind);
        Entry.Variant = Info.VariantKey;
        Entry.AssetId = Info.Id;
        Entry.AssetKind = Info.AssetKind;
        Entry.CookedPayloadType = Info.CookedPayloadType;
        Entry.SchemaVersion = Info.SchemaVersion;
        Entry.IsRuntime = (CatalogEntry.Origin == ::SnAPI::AssetPipeline::EAssetOrigin::RuntimeMemory);
        Entry.IsDirty = CatalogEntry.Dirty || HasRenameOverride;
        Entry.CanSave = CatalogEntry.CanSave;
        Entry.OwningPackPath = CatalogEntry.OwningPackPath;
        if (Entry.IsRuntime)
        {
            ++RuntimeAssetCount;
        }
        if (Entry.IsDirty)
        {
            ++DirtyAssetCount;
        }

        NextAssets.emplace_back(std::move(Entry));
    }

    for (auto It = m_assetRenameOverrides.begin(); It != m_assetRenameOverrides.end();)
    {
        if (SeenAssetIds.contains(It->first))
        {
            ++It;
        }
        else
        {
            It = m_assetRenameOverrides.erase(It);
        }
    }

    std::sort(NextAssets.begin(), NextAssets.end(), [](const DiscoveredAsset& Left, const DiscoveredAsset& Right) {
        if (Left.IsDirty != Right.IsDirty)
        {
            return Left.IsDirty && !Right.IsDirty;
        }
        if (Left.IsRuntime != Right.IsRuntime)
        {
            return Left.IsRuntime && !Right.IsRuntime;
        }
        if (Left.Name != Right.Name)
        {
            return Left.Name < Right.Name;
        }
        if (Left.TypeLabel != Right.TypeLabel)
        {
            return Left.TypeLabel < Right.TypeLabel;
        }
        if (Left.Variant != Right.Variant)
        {
            return Left.Variant < Right.Variant;
        }
        return Left.Key < Right.Key;
    });

    m_assets = std::move(NextAssets);
    m_assetIndexByKey.clear();
    m_assetIndexByKey.reserve(m_assets.size());
    for (std::size_t Index = 0; Index < m_assets.size(); ++Index)
    {
        m_assetIndexByKey[m_assets[Index].Key] = Index;
    }

    if (!m_selectedAssetKey.empty() && !m_assetIndexByKey.contains(m_selectedAssetKey))
    {
        m_selectedAssetKey.clear();
    }
    if (!m_placementAssetKey.empty() && !m_assetIndexByKey.contains(m_placementAssetKey))
    {
        m_placementAssetKey.clear();
    }

    std::ostringstream Message;
    Message << "Discovered " << m_assets.size() << " assets across "
            << m_assetManager->GetMountedPacks().size() << " mounted pack(s), "
            << RuntimeAssetCount << " runtime asset(s), "
            << DirtyAssetCount << " unsaved.";
    m_statusMessage = Message.str();
    return Ok();
}

Result EditorAssetService::OpenSelectedAssetPreview()
{
    const DiscoveredAsset* Asset = SelectedAsset();
    if (!Asset)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No selected asset to preview"));
    }

    if (!m_assetManager)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Asset manager is not initialized"));
    }

    std::ostringstream Summary;
    Summary << Asset->TypeLabel << " '" << Asset->Name << "'";

    if (Asset->AssetKind == AssetKindNodeGraph())
    {
        auto GraphResult = m_assetManager->Load<NodeGraph>(Asset->AssetId);
        if (!GraphResult)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, GraphResult.error()));
        }

        const std::size_t NodeCount = CountNodes(*GraphResult.value());
        const std::size_t ComponentCount = CountComponents(*GraphResult.value());
        Summary << " preview loaded (" << NodeCount << " nodes, " << ComponentCount << " components).";
    }
    else if (Asset->AssetKind == AssetKindLevel())
    {
        auto LevelResult = m_assetManager->Load<Level>(Asset->AssetId);
        if (!LevelResult)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, LevelResult.error()));
        }

        const std::size_t NodeCount = CountNodes(*LevelResult.value());
        const std::size_t ComponentCount = CountComponents(*LevelResult.value());
        Summary << " preview loaded (" << NodeCount << " nodes, " << ComponentCount << " components).";
    }
    else if (Asset->AssetKind == AssetKindWorld())
    {
        auto WorldResult = m_assetManager->Load<World>(Asset->AssetId);
        if (!WorldResult)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, WorldResult.error()));
        }

        const std::size_t NodeCount = CountNodes(*WorldResult.value());
        const std::size_t ComponentCount = CountComponents(*WorldResult.value());
        Summary << " preview loaded (" << NodeCount << " nodes, " << ComponentCount << " components).";
    }
    else
    {
        Summary << " preview is not implemented for this asset kind.";
    }

    m_previewSummary = Summary.str();
    m_statusMessage = m_previewSummary;
    return Ok();
}

Result EditorAssetService::SaveSelectedAssetUpdate()
{
    if (m_selectedAssetKey.empty())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No selected asset to save"));
    }

    return SaveAssetByKey(m_selectedAssetKey);
}

Result EditorAssetService::SaveAssetByKey(const std::string_view Key)
{
    const DiscoveredAsset* Asset = FindAssetByKey(Key);
    if (!Asset)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Asset was not found"));
    }

    if (!m_assetManager)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Asset manager is not initialized"));
    }

    if (Asset->IsRuntime)
    {
        auto SavePathResult = ResolveRuntimeSavePath(*Asset);
        if (!SavePathResult)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, SavePathResult.error()));
        }

        auto SaveResult = m_assetManager->SaveRuntimeAsset(Asset->AssetId, SavePathResult.value());
        if (!SaveResult)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, SaveResult.error()));
        }

        m_assetRenameOverrides.erase(Asset->AssetId);
        auto RefreshResult = RefreshDiscovery();
        if (!RefreshResult)
        {
            return RefreshResult;
        }

        m_statusMessage = "Saved runtime asset to pack: " + SavePathResult.value();
        return Ok();
    }

    auto PackPathResult = ResolveOwningPackPath(*Asset);
    if (!PackPathResult)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, PackPathResult.error()));
    }

    auto CookedPayloadResult = BuildCookedPayloadForAsset(*Asset);
    if (!CookedPayloadResult)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, CookedPayloadResult.error()));
    }

    ::SnAPI::AssetPipeline::AssetPackEntry Entry{};
    Entry.Id = Asset->AssetId;
    Entry.AssetKind = Asset->AssetKind;
    Entry.Name = Asset->Name;
    Entry.VariantKey = Asset->Variant;
    Entry.Cooked = std::move(CookedPayloadResult.value());

    ::SnAPI::AssetPipeline::AssetPackWriter Writer{};
    Writer.AddAsset(std::move(Entry));
    auto WriteResult = Writer.AppendUpdate(PackPathResult.value());
    if (!WriteResult)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, WriteResult.error()));
    }

    m_assetRenameOverrides.erase(Asset->AssetId);
    auto RefreshResult = RefreshDiscovery();
    if (!RefreshResult)
    {
        return RefreshResult;
    }

    m_statusMessage = "Saved asset update into pack: " + PackPathResult.value();
    return Ok();
}

Result EditorAssetService::DeleteAssetByKey(const std::string_view Key)
{
    const DiscoveredAsset* Asset = FindAssetByKey(Key);
    if (!Asset)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Asset was not found"));
    }

    if (!m_assetManager)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Asset manager is not initialized"));
    }

    const DiscoveredAsset AssetSnapshot = *Asset;
    const std::string DeletedAssetKey = AssetSnapshot.Key;
    const std::string DeletedAssetName = AssetSnapshot.Name.empty() ? DeletedAssetKey : AssetSnapshot.Name;

    if (AssetSnapshot.IsRuntime)
    {
        auto DeleteResult = m_assetManager->DeleteRuntimeAsset(AssetSnapshot.AssetId);
        if (!DeleteResult)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, DeleteResult.error()));
        }

        m_assetManager->ClearCache();
        m_assetRenameOverrides.erase(AssetSnapshot.AssetId);
        if (m_selectedAssetKey == DeletedAssetKey)
        {
            m_selectedAssetKey.clear();
            m_previewSummary.clear();
        }
        if (m_placementAssetKey == DeletedAssetKey)
        {
            m_placementAssetKey.clear();
        }

        auto RefreshResult = RefreshDiscovery();
        if (!RefreshResult)
        {
            return RefreshResult;
        }

        m_statusMessage = "Deleted runtime asset: " + DeletedAssetName;
        return Ok();
    }

    auto PackPathResult = ResolveOwningPackPath(AssetSnapshot);
    if (!PackPathResult)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, PackPathResult.error()));
    }

    const std::string PackPath = PackPathResult.value();
    ::SnAPI::AssetPipeline::AssetPackReader Reader{};
    auto OpenResult = Reader.Open(PackPath);
    if (!OpenResult)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, OpenResult.error()));
    }

    ::SnAPI::AssetPipeline::AssetPackWriter Writer{};
    const uint32_t AssetCount = Reader.GetAssetCount();
    bool Removed = false;
    uint32_t RemainingAssets = 0;
    for (uint32_t Index = 0; Index < AssetCount; ++Index)
    {
        auto InfoResult = Reader.GetAssetInfo(Index);
        if (!InfoResult)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, InfoResult.error()));
        }

        const ::SnAPI::AssetPipeline::AssetInfo& Info = *InfoResult;
        if (Info.Id == AssetSnapshot.AssetId)
        {
            Removed = true;
            continue;
        }

        auto CookedResult = Reader.LoadCookedPayload(Info.Id);
        if (!CookedResult)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, CookedResult.error()));
        }

        ::SnAPI::AssetPipeline::AssetPackEntry Entry{};
        Entry.Id = Info.Id;
        Entry.AssetKind = Info.AssetKind;
        Entry.Name = Info.Name;
        Entry.VariantKey = Info.VariantKey;
        Entry.Cooked = std::move(*CookedResult);

        Entry.Bulk.reserve(Info.BulkChunkCount);
        for (uint32_t BulkIndex = 0; BulkIndex < Info.BulkChunkCount; ++BulkIndex)
        {
            auto BulkResult = Reader.LoadBulkChunk(Info.Id, BulkIndex);
            if (!BulkResult)
            {
                return std::unexpected(MakeError(EErrorCode::InternalError, BulkResult.error()));
            }

            auto BulkInfoResult = Reader.GetBulkChunkInfo(Info.Id, BulkIndex);
            if (!BulkInfoResult)
            {
                return std::unexpected(MakeError(EErrorCode::InternalError, BulkInfoResult.error()));
            }

            ::SnAPI::AssetPipeline::BulkChunk Chunk(BulkInfoResult->Semantic, BulkInfoResult->SubIndex, true);
            Chunk.Bytes = std::move(*BulkResult);
            Entry.Bulk.push_back(std::move(Chunk));
        }

        Writer.AddAsset(std::move(Entry));
        ++RemainingAssets;
    }

    if (!Removed)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Asset was not found in its owning pack"));
    }

    m_assetManager->UnmountPack(PackPath);
    if (RemainingAssets == 0)
    {
        std::error_code Error{};
        std::filesystem::remove(PackPath, Error);
        if (Error)
        {
            (void)m_assetManager->MountPack(PackPath);
            return std::unexpected(MakeError(EErrorCode::InternalError,
                                             "Failed to delete empty pack '" + PackPath + "': " + Error.message()));
        }
    }
    else
    {
        auto WriteResult = Writer.Write(PackPath);
        if (!WriteResult)
        {
            (void)m_assetManager->MountPack(PackPath);
            return std::unexpected(MakeError(EErrorCode::InternalError, WriteResult.error()));
        }

        auto MountResult = m_assetManager->MountPack(PackPath);
        if (!MountResult)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, MountResult.error()));
        }
    }

    m_assetManager->ClearCache();
    m_assetRenameOverrides.erase(AssetSnapshot.AssetId);
    if (m_selectedAssetKey == DeletedAssetKey)
    {
        m_selectedAssetKey.clear();
        m_previewSummary.clear();
    }
    if (m_placementAssetKey == DeletedAssetKey)
    {
        m_placementAssetKey.clear();
    }

    auto RefreshResult = RefreshDiscovery();
    if (!RefreshResult)
    {
        return RefreshResult;
    }

    if (RemainingAssets == 0)
    {
        m_statusMessage = "Deleted asset and removed empty pack: " + DeletedAssetName;
    }
    else
    {
        m_statusMessage = "Deleted asset: " + DeletedAssetName;
    }
    return Ok();
}

Result EditorAssetService::DeleteSelectedAsset()
{
    if (m_selectedAssetKey.empty())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No selected asset to delete"));
    }
    return DeleteAssetByKey(m_selectedAssetKey);
}

Result EditorAssetService::RenameAssetByKey(const std::string_view Key, const std::string_view NewName)
{
    const DiscoveredAsset* Asset = FindAssetByKey(Key);
    if (!Asset)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Asset was not found"));
    }

    if (!m_assetManager)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Asset manager is not initialized"));
    }

    const std::string NormalizedName = NormalizeAssetLogicalName(NewName);
    if (NormalizedName.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Asset name cannot be empty"));
    }

    if (Asset->IsRuntime)
    {
        auto RenameResult = m_assetManager->RenameRuntimeAsset(Asset->AssetId, NormalizedName);
        if (!RenameResult)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, RenameResult.error()));
        }
        auto RefreshResult = RefreshDiscovery();
        if (!RefreshResult)
        {
            return RefreshResult;
        }
        m_statusMessage = "Renamed runtime asset to: " + NormalizedName;
        return Ok();
    }

    if (NormalizedName == Asset->Name)
    {
        m_assetRenameOverrides.erase(Asset->AssetId);
    }
    else
    {
        m_assetRenameOverrides[Asset->AssetId] = NormalizedName;
    }

    auto RefreshResult = RefreshDiscovery();
    if (!RefreshResult)
    {
        return RefreshResult;
    }

    m_statusMessage = "Renamed asset in editor: " + NormalizedName + " (save to persist)";
    return Ok();
}

Result EditorAssetService::RenameSelectedAsset(const std::string_view NewName)
{
    if (m_selectedAssetKey.empty())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No selected asset to rename"));
    }
    return RenameAssetByKey(m_selectedAssetKey, NewName);
}

Result EditorAssetService::CreateRuntimePrefabFromNode(EditorServiceContext& Context, const NodeHandle SourceHandle)
{
    if (!m_assetManager)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Asset manager is not initialized"));
    }

    BaseNode* SourceNode = SourceHandle.Borrowed();
    if (!SourceNode)
    {
        SourceNode = SourceHandle.BorrowedSlowByUuid();
    }
    if (!SourceNode)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Source node not found for prefab creation"));
    }

    if (TypeRegistry::Instance().IsA(SourceNode->TypeKey(), StaticTypeId<World>()))
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "World cannot be converted into a prefab asset"));
    }

    ::SnAPI::AssetPipeline::RuntimeAssetUpsert RuntimeAsset{};
    RuntimeAsset.Id = ::SnAPI::AssetPipeline::AssetId::Generate();
    RuntimeAsset.Bulk.clear();
    RuntimeAsset.Dirty = true;

    if (TypeRegistry::Instance().IsA(SourceNode->TypeKey(), StaticTypeId<Level>()))
    {
        auto* LevelNode = dynamic_cast<Level*>(SourceNode);
        if (!LevelNode)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, "Selected level node could not be resolved"));
        }

        auto PayloadResult = LevelSerializer::Serialize(*LevelNode);
        if (!PayloadResult)
        {
            return std::unexpected(PayloadResult.error());
        }
        if (PayloadResult->Graph.Nodes.empty()
            && (!SourceNode->Children().empty() || !SourceNode->ComponentTypes().empty()))
        {
            Level FallbackLevel(SourceNode->Name().empty() ? std::string("Level") : SourceNode->Name());
            auto CloneResult = CloneNodeChildrenAsRoots(*SourceNode, FallbackLevel);
            if (!CloneResult)
            {
                return std::unexpected(CloneResult.error());
            }
            auto FallbackPayloadResult = LevelSerializer::Serialize(FallbackLevel);
            if (!FallbackPayloadResult)
            {
                return std::unexpected(FallbackPayloadResult.error());
            }
            PayloadResult = std::move(FallbackPayloadResult);
        }

        std::vector<uint8_t> Bytes{};
        auto SerializeResult = SerializeLevelPayload(*PayloadResult, Bytes);
        if (!SerializeResult)
        {
            return std::unexpected(SerializeResult.error());
        }

        RuntimeAsset.Name = MakeUniqueLogicalName(*m_assetManager, "Levels", SourceNode->Name());
        RuntimeAsset.AssetKind = AssetKindLevel();
        RuntimeAsset.Cooked = ::SnAPI::AssetPipeline::TypedPayload(
            PayloadLevel(),
            LevelSerializer::kSchemaVersion,
            std::move(Bytes));
    }
    else
    {
        NodeGraph PrefabGraph{};
        std::string BaseName = SourceNode->Name();
        if (BaseName.empty())
        {
            if (const TypeInfo* Type = TypeRegistry::Instance().Find(SourceNode->TypeKey()))
            {
                BaseName = ShortTypeName(Type->Name);
            }
            else
            {
                BaseName = "Node";
            }
        }
        PrefabGraph.Name(BaseName + ".Graph");

        if (TypeRegistry::Instance().IsA(SourceNode->TypeKey(), StaticTypeId<NodeGraph>()))
        {
            auto* SourceGraph = dynamic_cast<NodeGraph*>(SourceNode);
            if (!SourceGraph)
            {
                return std::unexpected(MakeError(EErrorCode::InternalError, "Selected graph node could not be resolved"));
            }

            auto PayloadResult = NodeGraphSerializer::Serialize(*SourceGraph);
            if (!PayloadResult)
            {
                return std::unexpected(PayloadResult.error());
            }
            if (PayloadResult->Nodes.empty()
                && (!SourceNode->Children().empty() || !SourceNode->ComponentTypes().empty()))
            {
                auto CloneResult = CloneNodeSubtree(*SourceNode, PrefabGraph, NodeHandle{});
                if (!CloneResult)
                {
                    return std::unexpected(CloneResult.error());
                }
                auto FallbackPayloadResult = NodeGraphSerializer::Serialize(PrefabGraph);
                if (!FallbackPayloadResult)
                {
                    return std::unexpected(FallbackPayloadResult.error());
                }
                PayloadResult = std::move(FallbackPayloadResult);
            }

            std::vector<uint8_t> Bytes{};
            auto SerializeResult = SerializeNodeGraphPayload(*PayloadResult, Bytes);
            if (!SerializeResult)
            {
                return std::unexpected(SerializeResult.error());
            }

            RuntimeAsset.Name = MakeUniqueLogicalName(*m_assetManager, "Graphs", BaseName);
            RuntimeAsset.AssetKind = AssetKindNodeGraph();
            RuntimeAsset.Cooked = ::SnAPI::AssetPipeline::TypedPayload(
                PayloadNodeGraph(),
                NodeGraphSerializer::kSchemaVersion,
                std::move(Bytes));
        }
        else
        {
            auto CloneResult = CloneNodeSubtree(*SourceNode, PrefabGraph, NodeHandle{});
            if (!CloneResult)
            {
                return std::unexpected(CloneResult.error());
            }

            auto PayloadResult = NodeGraphSerializer::Serialize(PrefabGraph);
            if (!PayloadResult)
            {
                return std::unexpected(PayloadResult.error());
            }
            if (PayloadResult->Nodes.empty())
            {
                return std::unexpected(MakeError(EErrorCode::InternalError, "Prefab clone produced an empty graph payload"));
            }

            std::vector<uint8_t> Bytes{};
            auto SerializeResult = SerializeNodeGraphPayload(*PayloadResult, Bytes);
            if (!SerializeResult)
            {
                return std::unexpected(SerializeResult.error());
            }

            RuntimeAsset.Name = MakeUniqueLogicalName(*m_assetManager, "Prefabs", BaseName);
            RuntimeAsset.AssetKind = AssetKindNodeGraph();
            RuntimeAsset.Cooked = ::SnAPI::AssetPipeline::TypedPayload(
                PayloadNodeGraph(),
                NodeGraphSerializer::kSchemaVersion,
                std::move(Bytes));
        }
    }

    auto UpsertResult = m_assetManager->UpsertRuntimeAsset(std::move(RuntimeAsset));
    if (!UpsertResult)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, UpsertResult.error()));
    }

    auto RefreshResult = RefreshDiscovery();
    if (!RefreshResult)
    {
        return RefreshResult;
    }

    const std::string AssetKey = UpsertResult->ToString();
    (void)SelectAssetByKey(AssetKey);
    m_statusMessage = "Created runtime prefab asset: " + SourceNode->Name();
    (void)Context;
    return Ok();
}

Result EditorAssetService::InstantiateArmedAsset(EditorServiceContext& Context)
{
    if (m_placementAssetKey.empty())
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "No placement-armed asset"));
    }

    const std::string PlacementKey = m_placementAssetKey;
    auto InstantiateResult = InstantiateAssetByKey(Context, PlacementKey);
    if (!InstantiateResult)
    {
        return InstantiateResult;
    }

    m_placementAssetKey.clear();
    return Ok();
}

Result EditorAssetService::InstantiateAssetByKey(EditorServiceContext& Context, const std::string_view Key)
{
    const DiscoveredAsset* Asset = FindAssetByKey(Key);
    if (!Asset)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Asset was not found for instantiation"));
    }

    if (!m_assetManager)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Asset manager is not initialized"));
    }

    if (Asset->AssetKind == AssetKindNodeGraph())
    {
        return InstantiateNodeGraphAsset(Context, *Asset);
    }
    if (Asset->AssetKind == AssetKindLevel())
    {
        return InstantiateLevelAsset(Context, *Asset);
    }
    if (Asset->AssetKind == AssetKindWorld())
    {
        return InstantiateWorldAsset(Context, *Asset);
    }

    return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Unsupported asset kind for instantiation"));
}

std::vector<std::string> EditorAssetService::BuildPackSearchPaths()
{
    std::vector<std::string> Paths{};
    std::unordered_set<std::string> SeenPaths{};

    const std::filesystem::path DefaultPackDirectory = EditorDefaultShapeAssetDirectory();
    if (!DefaultPackDirectory.empty())
    {
        AppendUniquePath(Paths, SeenPaths, DefaultPackDirectory);
    }

    std::error_code Error{};
    const std::filesystem::path CurrentPath = std::filesystem::current_path(Error);
    if (!Error)
    {
        AppendUniquePath(Paths, SeenPaths, CurrentPath);
        AppendUniquePath(Paths, SeenPaths, CurrentPath / "Content");
        AppendUniquePath(Paths, SeenPaths, CurrentPath / "Assets");
        AppendUniquePath(Paths, SeenPaths, CurrentPath / "Packs");
        AppendUniquePath(Paths, SeenPaths, CurrentPath / "build");
    }

    if (const char* EnvRaw = std::getenv("SNAPI_EDITOR_ASSET_PATHS"))
    {
        const auto ExtraPaths = ParsePackSearchPathEnv(std::string_view(EnvRaw));
        for (const std::string& Path : ExtraPaths)
        {
            AppendUniquePath(Paths, SeenPaths, std::filesystem::path(Path));
        }
    }

    return Paths;
}

std::vector<std::string> EditorAssetService::ParsePackSearchPathEnv(const std::string_view Raw)
{
    std::vector<std::string> Paths{};
    std::string Token{};
    Token.reserve(Raw.size());

    for (const char Character : Raw)
    {
        if (Character == ';' || Character == ':')
        {
            if (!Token.empty())
            {
                Paths.push_back(Token);
                Token.clear();
            }
            continue;
        }
        Token.push_back(Character);
    }

    if (!Token.empty())
    {
        Paths.push_back(Token);
    }

    return Paths;
}

std::string EditorAssetService::AssetKindToLabel(const ::SnAPI::AssetPipeline::TypeId& AssetKind)
{
    if (AssetKind == AssetKindWorld())
    {
        return "World";
    }
    if (AssetKind == AssetKindLevel())
    {
        return "Level";
    }
    if (AssetKind == AssetKindNodeGraph())
    {
        return "NodeGraph";
    }
    return "Asset";
}

const EditorAssetService::DiscoveredAsset* EditorAssetService::FindAssetByKey(const std::string_view Key) const
{
    const auto It = m_assetIndexByKey.find(std::string(Key));
    if (It == m_assetIndexByKey.end())
    {
        return nullptr;
    }

    const std::size_t Index = It->second;
    if (Index >= m_assets.size())
    {
        return nullptr;
    }
    return &m_assets[Index];
}

std::expected<std::string, std::string> EditorAssetService::ResolveOwningPackPath(const DiscoveredAsset& Asset) const
{
    if (!m_assetManager)
    {
        return std::unexpected("Asset manager is not initialized");
    }

    if (!Asset.OwningPackPath.empty())
    {
        return Asset.OwningPackPath;
    }

    if (Asset.IsRuntime)
    {
        return std::unexpected("Runtime memory assets do not have an owning pack");
    }

    const auto MountedPacks = m_assetManager->GetMountedPacks();
    for (const std::string& PackPath : MountedPacks)
    {
        ::SnAPI::AssetPipeline::AssetPackReader Reader{};
        auto OpenResult = Reader.Open(PackPath);
        if (!OpenResult)
        {
            continue;
        }

        auto AssetInfoResult = Reader.FindAsset(Asset.AssetId);
        if (AssetInfoResult)
        {
            return PackPath;
        }
    }

    return std::unexpected("Unable to resolve owning pack for selected asset");
}

std::expected<std::string, std::string> EditorAssetService::ResolveRuntimeSavePath(const DiscoveredAsset& Asset) const
{
    if (!Asset.IsRuntime)
    {
        return std::unexpected("Selected asset is not a runtime memory asset");
    }

    std::error_code Error{};
    const std::filesystem::path CurrentPath = std::filesystem::current_path(Error);
    if (Error)
    {
        return std::unexpected("Failed to resolve current directory: " + Error.message());
    }

    std::filesystem::path AssetsRoot = CurrentPath / "Assets";
    std::filesystem::create_directories(AssetsRoot, Error);
    if (Error)
    {
        return std::unexpected("Failed to create Assets directory: " + Error.message());
    }

    const std::string RelativeName = NormalizeAssetLogicalName(Asset.Name);
    if (RelativeName.empty())
    {
        return std::unexpected("Runtime asset has an empty logical name");
    }

    std::filesystem::path OutputPath = AssetsRoot / std::filesystem::path(RelativeName);
    const std::string ExtensionLower = ToLowerCopy(OutputPath.extension().string());
    if (ExtensionLower != ".snpak")
    {
        OutputPath += ".snpak";
    }

    return OutputPath.lexically_normal().string();
}

std::expected<::SnAPI::AssetPipeline::TypedPayload, std::string> EditorAssetService::BuildCookedPayloadForAsset(
    const DiscoveredAsset& Asset)
{
    if (!m_assetManager)
    {
        return std::unexpected("Asset manager is not initialized");
    }

    if (Asset.AssetKind == AssetKindNodeGraph())
    {
        auto GraphResult = m_assetManager->Load<NodeGraph>(Asset.AssetId);
        if (!GraphResult)
        {
            return std::unexpected(GraphResult.error());
        }

        auto PayloadResult = NodeGraphSerializer::Serialize(*GraphResult.value());
        if (!PayloadResult)
        {
            return std::unexpected(PayloadResult.error().Message);
        }

        std::vector<uint8_t> Bytes{};
        auto BytesResult = SerializeNodeGraphPayload(*PayloadResult, Bytes);
        if (!BytesResult)
        {
            return std::unexpected(BytesResult.error().Message);
        }

        return ::SnAPI::AssetPipeline::TypedPayload(PayloadNodeGraph(), NodeGraphSerializer::kSchemaVersion, std::move(Bytes));
    }

    if (Asset.AssetKind == AssetKindLevel())
    {
        auto LevelResult = m_assetManager->Load<Level>(Asset.AssetId);
        if (!LevelResult)
        {
            return std::unexpected(LevelResult.error());
        }

        auto PayloadResult = LevelSerializer::Serialize(*LevelResult.value());
        if (!PayloadResult)
        {
            return std::unexpected(PayloadResult.error().Message);
        }

        std::vector<uint8_t> Bytes{};
        auto BytesResult = SerializeLevelPayload(*PayloadResult, Bytes);
        if (!BytesResult)
        {
            return std::unexpected(BytesResult.error().Message);
        }

        return ::SnAPI::AssetPipeline::TypedPayload(PayloadLevel(), LevelSerializer::kSchemaVersion, std::move(Bytes));
    }

    if (Asset.AssetKind == AssetKindWorld())
    {
        auto WorldResult = m_assetManager->Load<World>(Asset.AssetId);
        if (!WorldResult)
        {
            return std::unexpected(WorldResult.error());
        }

        auto PayloadResult = WorldSerializer::Serialize(*WorldResult.value());
        if (!PayloadResult)
        {
            return std::unexpected(PayloadResult.error().Message);
        }

        std::vector<uint8_t> Bytes{};
        auto BytesResult = SerializeWorldPayload(*PayloadResult, Bytes);
        if (!BytesResult)
        {
            return std::unexpected(BytesResult.error().Message);
        }

        return ::SnAPI::AssetPipeline::TypedPayload(PayloadWorld(), WorldSerializer::kSchemaVersion, std::move(Bytes));
    }

    return std::unexpected("Unsupported asset kind for payload serialization");
}

Result EditorAssetService::InstantiateNodeGraphAsset(EditorServiceContext& Context, const DiscoveredAsset& Asset)
{
    auto* WorldPtr = Context.Runtime().WorldPtr();
    if (!WorldPtr)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Runtime world is not available"));
    }

    auto GraphResult = m_assetManager->Load<NodeGraph>(Asset.AssetId);
    if (!GraphResult)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, GraphResult.error()));
    }

    auto PayloadResult = NodeGraphSerializer::Serialize(*GraphResult.value());
    if (!PayloadResult)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, PayloadResult.error().Message));
    }

    auto CreateResult = WorldPtr->CreateNode<NodeGraph>(Asset.Name.empty() ? std::string("NodeGraphAsset") : Asset.Name);
    if (!CreateResult)
    {
        return std::unexpected(CreateResult.error());
    }

    auto* CreatedNodeGraph = dynamic_cast<NodeGraph*>(CreateResult->Borrowed());
    if (!CreatedNodeGraph)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to create destination NodeGraph"));
    }

    auto DeserializeResult = NodeGraphSerializer::Deserialize(*PayloadResult, *CreatedNodeGraph);
    if (!DeserializeResult)
    {
        return std::unexpected(DeserializeResult.error());
    }

    m_statusMessage = "Instantiated NodeGraph asset: " + Asset.Name;
    return Ok();
}

Result EditorAssetService::InstantiateLevelAsset(EditorServiceContext& Context, const DiscoveredAsset& Asset)
{
    auto* WorldPtr = Context.Runtime().WorldPtr();
    if (!WorldPtr)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Runtime world is not available"));
    }

    auto LevelResult = m_assetManager->Load<Level>(Asset.AssetId);
    if (!LevelResult)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, LevelResult.error()));
    }

    auto PayloadResult = LevelSerializer::Serialize(*LevelResult.value());
    if (!PayloadResult)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, PayloadResult.error().Message));
    }

    auto CreateResult = WorldPtr->CreateLevel(Asset.Name.empty() ? std::string("LevelAsset") : Asset.Name);
    if (!CreateResult)
    {
        return std::unexpected(CreateResult.error());
    }

    auto* CreatedLevel = dynamic_cast<Level*>(CreateResult->Borrowed());
    if (!CreatedLevel)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to create destination Level"));
    }

    auto DeserializeResult = LevelSerializer::Deserialize(*PayloadResult, *CreatedLevel);
    if (!DeserializeResult)
    {
        return std::unexpected(DeserializeResult.error());
    }

    m_statusMessage = "Instantiated Level asset: " + Asset.Name;
    return Ok();
}

Result EditorAssetService::InstantiateWorldAsset(EditorServiceContext& Context, const DiscoveredAsset& Asset)
{
    auto* WorldPtr = Context.Runtime().WorldPtr();
    if (!WorldPtr)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Runtime world is not available"));
    }

    auto SourceWorldResult = m_assetManager->Load<World>(Asset.AssetId);
    if (!SourceWorldResult)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, SourceWorldResult.error()));
    }

    auto PayloadResult = WorldSerializer::Serialize(*SourceWorldResult.value());
    if (!PayloadResult)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, PayloadResult.error().Message));
    }

    auto CreateResult = WorldPtr->CreateNode<NodeGraph>(Asset.Name.empty() ? std::string("WorldAsset") : Asset.Name);
    if (!CreateResult)
    {
        return std::unexpected(CreateResult.error());
    }

    auto* CreatedNodeGraph = dynamic_cast<NodeGraph*>(CreateResult->Borrowed());
    if (!CreatedNodeGraph)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to create destination graph for world asset"));
    }

    auto DeserializeResult = NodeGraphSerializer::Deserialize(PayloadResult->Graph, *CreatedNodeGraph);
    if (!DeserializeResult)
    {
        return std::unexpected(DeserializeResult.error());
    }

    m_statusMessage = "Instantiated World asset graph: " + Asset.Name;
    return Ok();
}

} // namespace SnAPI::GameFramework::Editor
