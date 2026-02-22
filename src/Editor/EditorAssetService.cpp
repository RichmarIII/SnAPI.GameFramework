#include "Editor/EditorAssetService.h"

#include "AssetPackReader.h"
#include "AssetPackWriter.h"
#include "AssetPipelineFactories.h"
#include "AssetPipelineIds.h"
#include "GameRuntime.h"
#include "Level.h"
#include "NodeGraph.h"
#include "Serialization.h"
#include "TransformComponent.h"
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

    const std::vector<::SnAPI::AssetPipeline::AssetInfo> RawAssets = m_assetManager->ListAssets();

    std::vector<DiscoveredAsset> NextAssets{};
    NextAssets.reserve(RawAssets.size());

    std::unordered_set<std::string> SeenKeys{};
    SeenKeys.reserve(RawAssets.size());

    for (const auto& Info : RawAssets)
    {
        const std::string Key = Info.Id.ToString();
        if (!SeenKeys.insert(Key).second)
        {
            continue;
        }

        DiscoveredAsset Entry{};
        Entry.Key = Key;
        Entry.Name = Info.Name.empty() ? Key : Info.Name;
        Entry.TypeLabel = AssetKindToLabel(Info.AssetKind);
        Entry.Variant = Info.VariantKey;
        Entry.AssetId = Info.Id;
        Entry.AssetKind = Info.AssetKind;
        Entry.CookedPayloadType = Info.CookedPayloadType;
        Entry.SchemaVersion = Info.SchemaVersion;
        NextAssets.emplace_back(std::move(Entry));
    }

    std::sort(NextAssets.begin(), NextAssets.end(), [](const DiscoveredAsset& Left, const DiscoveredAsset& Right) {
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
            << m_assetManager->GetMountedPacks().size() << " mounted pack(s).";
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
    const DiscoveredAsset* Asset = SelectedAsset();
    if (!Asset)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No selected asset to save"));
    }

    if (!m_assetManager)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Asset manager is not initialized"));
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

    m_statusMessage = "Saved asset update into pack: " + PackPathResult.value();
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
