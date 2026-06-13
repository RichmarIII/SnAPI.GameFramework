#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <typeindex>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "AuthoredAssetJson.h"
#include "Conduit/Editor/Service.h"
#include "Editor/EditorAssetIconService.h"
#include "Editor/EditorAssetService.h"
#include "Editor/EditorImportSettings.h"
#include "Editor/EditorPieService.h"
#include "Editor/IEditorService.h"
#include "GameFramework.hpp"
#include "PathResolver.h"
#include "TypeAutoRegistry.h"
#include "UIAccordion.h"
#include "UICheckbox.h"
#include "UIComboBox.h"
#include "UIContext.h"
#include "UINumberField.h"
#include "UIPropertyPanel.h"
#include "UIText.h"

using namespace SnAPI::GameFramework;
using namespace SnAPI::GameFramework::Editor;

namespace
{

struct SourceAssetEditorNodeHost : BaseNode, NodeCRTP<SourceAssetEditorNodeHost>
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::SourceAssetEditorNodeHost";
};

struct SourceAssetEditorDefaultNode : BaseNode, NodeCRTP<SourceAssetEditorDefaultNode>
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::SourceAssetEditorDefaultNode";

    void OnCreate()
    {
        if (!Has<TransformComponent>())
        {
            (void)Add<TransformComponent>();
        }
    }
};

struct SourceAssetEditorNestedSettingsComponent : BaseComponent, ComponentCRTP<SourceAssetEditorNestedSettingsComponent>
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::SourceAssetEditorNestedSettingsComponent";

    struct Settings
    {
        static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::SourceAssetEditorNestedSettingsComponent::Settings";

        float Scalar = 3.0f;
    };

    Settings& EditSettings() { return m_settings; }
    const Settings& GetSettings() const { return m_settings; }

private:
    Settings m_settings{};
};

struct SourceAssetEditorNestedSettingsNode : BaseNode, NodeCRTP<SourceAssetEditorNestedSettingsNode>
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::SourceAssetEditorNestedSettingsNode";

    void OnCreate()
    {
        if (!Has<SourceAssetEditorNestedSettingsComponent>())
        {
            (void)Add<SourceAssetEditorNestedSettingsComponent>();
        }
    }
};

struct SourceAssetEditorCameraNode : BaseNode, NodeCRTP<SourceAssetEditorCameraNode>
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::SourceAssetEditorCameraNode";

    void OnCreate()
    {
        if (!Has<CameraComponent>())
        {
            (void)Add<CameraComponent>();
        }
    }
};

struct SourceAssetEditorFlagsComponent : BaseComponent, ComponentCRTP<SourceAssetEditorFlagsComponent>
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::SourceAssetEditorFlagsComponent";

    MethodFlags Flags = MethodFlags(EMethodFlagBits::RpcReliable) | EMethodFlagBits::RpcNetServer;
};

struct SourceAssetEditorTypeIdComponent : BaseComponent, ComponentCRTP<SourceAssetEditorTypeIdComponent>
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::SourceAssetEditorTypeIdComponent";

    TypeId SelectedType = StaticTypeId<BaseNode>();
};

void EnsureSourceAssetEditorNodeHostRegistered()
{
    RegisterBuiltinTypes();

    if (TypeRegistry::Instance().Find(StaticTypeId<SourceAssetEditorNodeHost>()))
    {
        return;
    }

    auto RegisterResult = TTypeBuilder<SourceAssetEditorNodeHost>(SourceAssetEditorNodeHost::kTypeName)
        .Base<BaseNode>()
        .Constructor<>()
        .Register();
    REQUIRE(RegisterResult);
}

void EnsureSourceAssetEditorDefaultNodeRegistered()
{
    RegisterBuiltinTypes();

    if (TypeRegistry::Instance().Find(StaticTypeId<SourceAssetEditorDefaultNode>()))
    {
        return;
    }

    auto RegisterResult = TTypeBuilder<SourceAssetEditorDefaultNode>(SourceAssetEditorDefaultNode::kTypeName)
        .Base<BaseNode>()
        .Constructor<>()
        .Register();
    REQUIRE(RegisterResult);
}

void EnsureSourceAssetEditorNestedSettingsNodeRegistered()
{
    RegisterBuiltinTypes();

    if (!TypeRegistry::Instance().Find(StaticTypeId<SourceAssetEditorNestedSettingsComponent::Settings>()))
    {
        auto SettingsRegisterResult = TTypeBuilder<SourceAssetEditorNestedSettingsComponent::Settings>(
            SourceAssetEditorNestedSettingsComponent::Settings::kTypeName)
            .Field("Scalar", &SourceAssetEditorNestedSettingsComponent::Settings::Scalar)
            .Constructor<>()
            .Register();
        REQUIRE(SettingsRegisterResult);
    }

    if (!TypeRegistry::Instance().Find(StaticTypeId<SourceAssetEditorNestedSettingsComponent>()))
    {
        auto ComponentRegisterResult = TTypeBuilder<SourceAssetEditorNestedSettingsComponent>(
            SourceAssetEditorNestedSettingsComponent::kTypeName)
            .Field("Settings",
                   &SourceAssetEditorNestedSettingsComponent::EditSettings,
                   &SourceAssetEditorNestedSettingsComponent::GetSettings)
            .Constructor<>()
            .Register();
        REQUIRE(ComponentRegisterResult);
    }

    if (TypeRegistry::Instance().Find(StaticTypeId<SourceAssetEditorNestedSettingsNode>()))
    {
        return;
    }

    auto RegisterResult = TTypeBuilder<SourceAssetEditorNestedSettingsNode>(SourceAssetEditorNestedSettingsNode::kTypeName)
        .Base<BaseNode>()
        .Constructor<>()
        .Register();
    REQUIRE(RegisterResult);
}

void EnsureSourceAssetEditorCameraNodeRegistered()
{
    RegisterBuiltinTypes();

    if (TypeRegistry::Instance().Find(StaticTypeId<SourceAssetEditorCameraNode>()))
    {
        return;
    }

    auto RegisterResult = TTypeBuilder<SourceAssetEditorCameraNode>(SourceAssetEditorCameraNode::kTypeName)
        .Base<BaseNode>()
        .Constructor<>()
        .Register();
    REQUIRE(RegisterResult);
}

void EnsureSourceAssetEditorFlagsComponentRegistered()
{
    RegisterBuiltinTypes();

    if (TypeRegistry::Instance().Find(StaticTypeId<SourceAssetEditorFlagsComponent>()))
    {
        return;
    }

    auto RegisterResult = TTypeBuilder<SourceAssetEditorFlagsComponent>(SourceAssetEditorFlagsComponent::kTypeName)
        .Field("Flags", &SourceAssetEditorFlagsComponent::Flags)
        .Constructor<>()
        .Register();
    REQUIRE(RegisterResult);
}

void EnsureSourceAssetEditorTypeIdComponentRegistered()
{
    RegisterBuiltinTypes();
    EnsureSourceAssetEditorNodeHostRegistered();

    if (TypeRegistry::Instance().Find(StaticTypeId<SourceAssetEditorTypeIdComponent>()))
    {
        return;
    }

    auto RegisterResult = TTypeBuilder<SourceAssetEditorTypeIdComponent>(SourceAssetEditorTypeIdComponent::kTypeName)
        .Field("SelectedType", &SourceAssetEditorTypeIdComponent::SelectedType)
        .Constructor<>()
        .Register();
    REQUIRE(RegisterResult);
}

struct TempDir
{
    std::filesystem::path Path{};

    TempDir()
    {
        const auto Stamp = std::to_string(static_cast<unsigned long long>(
            std::chrono::steady_clock::now().time_since_epoch().count()));
        Path = std::filesystem::temp_directory_path() / ("snapi_gf_source_editor_test_" + Stamp);
        std::filesystem::create_directories(Path);
    }

    ~TempDir()
    {
        std::error_code Ec{};
        std::filesystem::remove_all(Path, Ec);
    }
};

std::string ReadTextFile(const std::filesystem::path& Path)
{
    std::ifstream In(Path, std::ios::binary);
    REQUIRE(In.is_open());
    std::ostringstream Buffer{};
    Buffer << In.rdbuf();
    return Buffer.str();
}

void WriteTextFile(const std::filesystem::path& Path, const std::string& Text)
{
    std::error_code Ec{};
    std::filesystem::create_directories(Path.parent_path(), Ec);
    REQUIRE_FALSE(Ec);

    std::ofstream Out(Path, std::ios::binary | std::ios::trunc);
    REQUIRE(Out.is_open());
    Out.write(Text.data(), static_cast<std::streamsize>(Text.size()));
    REQUIRE(Out.good());
}

void WriteBinaryFile(const std::filesystem::path& Path, const std::vector<std::uint8_t>& Bytes)
{
    std::error_code Ec{};
    std::filesystem::create_directories(Path.parent_path(), Ec);
    REQUIRE_FALSE(Ec);

    std::ofstream Out(Path, std::ios::binary | std::ios::trunc);
    REQUIRE(Out.is_open());
    if (!Bytes.empty())
    {
        Out.write(reinterpret_cast<const char*>(Bytes.data()), static_cast<std::streamsize>(Bytes.size()));
    }
    REQUIRE(Out.good());
}

std::filesystem::path WriteTinyPngFixture(const std::filesystem::path& RootDir, std::string_view LeafName = "tiny.png")
{
    static constexpr std::array<std::uint8_t, 90> kPng{
        0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A,
        0x00,0x00,0x00,0x0D,0x49,0x48,0x44,0x52,
        0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,
        0x08,0x02,0x00,0x00,0x00,0x90,0x77,0x53,
        0xDE,0x00,0x00,0x00,0x09,0x70,0x48,0x59,
        0x73,0x00,0x00,0x0B,0x13,0x00,0x00,0x0B,
        0x13,0x01,0x00,0x9A,0x9C,0x18,0x00,0x00,
        0x00,0x0C,0x49,0x44,0x41,0x54,0x08,0x99,
        0x63,0xF8,0xCF,0xC0,0x00,0x00,0x03,0x01,
        0x01,0x00,0x9C,0xE3,0xBF,0x59,0x00,0x00,
        0x00,0x00,0x49,0x45,0x4E,0x44,0xAE,0x42,
        0x60,0x82
    };

    const std::filesystem::path Path = RootDir / std::string(LeafName);
    WriteBinaryFile(Path, std::vector<std::uint8_t>(kPng.begin(), kPng.end()));
    return Path;
}

void AppendFloat(std::vector<std::uint8_t>& Buffer, const float Value)
{
    const auto* Bytes = reinterpret_cast<const std::uint8_t*>(&Value);
    Buffer.insert(Buffer.end(), Bytes, Bytes + sizeof(float));
}

void AppendU16(std::vector<std::uint8_t>& Buffer, const std::uint16_t Value)
{
    const auto* Bytes = reinterpret_cast<const std::uint8_t*>(&Value);
    Buffer.insert(Buffer.end(), Bytes, Bytes + sizeof(std::uint16_t));
}

void AppendU32(std::vector<std::uint8_t>& Buffer, const std::uint32_t Value)
{
    const auto* Bytes = reinterpret_cast<const std::uint8_t*>(&Value);
    Buffer.insert(Buffer.end(), Bytes, Bytes + sizeof(std::uint32_t));
}

std::filesystem::path WriteEmbeddedTextureGltfFixture(const std::filesystem::path& RootDir)
{
    std::filesystem::create_directories(RootDir);

    std::vector<std::uint8_t> Buffer{};
    Buffer.reserve(256);

    AppendFloat(Buffer, 0.0f);
    AppendFloat(Buffer, 0.0f);
    AppendFloat(Buffer, 0.0f);
    AppendFloat(Buffer, 1.0f);
    AppendFloat(Buffer, 0.0f);
    AppendFloat(Buffer, 0.0f);
    AppendFloat(Buffer, 0.0f);
    AppendFloat(Buffer, 1.0f);
    AppendFloat(Buffer, 0.0f);
    constexpr std::uint32_t PositionByteLength = 9u * sizeof(float);

    AppendFloat(Buffer, 0.0f);
    AppendFloat(Buffer, 0.0f);
    AppendFloat(Buffer, 1.0f);
    AppendFloat(Buffer, 0.0f);
    AppendFloat(Buffer, 0.0f);
    AppendFloat(Buffer, 1.0f);
    constexpr std::uint32_t TexcoordByteOffset = PositionByteLength;
    constexpr std::uint32_t TexcoordByteLength = 6u * sizeof(float);

    AppendU16(Buffer, 0u);
    AppendU16(Buffer, 1u);
    AppendU16(Buffer, 2u);
    constexpr std::uint32_t IndexByteOffset = TexcoordByteOffset + TexcoordByteLength;
    constexpr std::uint32_t IndexByteLength = 3u * sizeof(std::uint16_t);

    while ((Buffer.size() % 4u) != 0u)
    {
        Buffer.push_back(0u);
    }

    static constexpr std::array<std::uint8_t, 90> kPng{
        0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A,
        0x00,0x00,0x00,0x0D,0x49,0x48,0x44,0x52,
        0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,
        0x08,0x02,0x00,0x00,0x00,0x90,0x77,0x53,
        0xDE,0x00,0x00,0x00,0x09,0x70,0x48,0x59,
        0x73,0x00,0x00,0x0B,0x13,0x00,0x00,0x0B,
        0x13,0x01,0x00,0x9A,0x9C,0x18,0x00,0x00,
        0x00,0x0C,0x49,0x44,0x41,0x54,0x08,0x99,
        0x63,0xF8,0xCF,0xC0,0x00,0x00,0x03,0x01,
        0x01,0x00,0x9C,0xE3,0xBF,0x59,0x00,0x00,
        0x00,0x00,0x49,0x45,0x4E,0x44,0xAE,0x42,
        0x60,0x82
    };

    const std::uint32_t ImageByteOffset = static_cast<std::uint32_t>(Buffer.size());
    Buffer.insert(Buffer.end(), kPng.begin(), kPng.end());
    const std::uint32_t ImageByteLength = static_cast<std::uint32_t>(kPng.size());
    const std::uint32_t BufferByteLength = static_cast<std::uint32_t>(Buffer.size());

    const std::filesystem::path BinPath = RootDir / "mesh.bin";
    WriteBinaryFile(BinPath, Buffer);

    std::ostringstream Json{};
    Json
        << "{\n"
        << "  \"asset\": {\"version\": \"2.0\"},\n"
        << "  \"buffers\": [{\"uri\": \"mesh.bin\", \"byteLength\": " << BufferByteLength << "}],\n"
        << "  \"bufferViews\": [\n"
        << "    {\"buffer\": 0, \"byteOffset\": 0, \"byteLength\": " << PositionByteLength << ", \"target\": 34962},\n"
        << "    {\"buffer\": 0, \"byteOffset\": " << TexcoordByteOffset << ", \"byteLength\": " << TexcoordByteLength << ", \"target\": 34962},\n"
        << "    {\"buffer\": 0, \"byteOffset\": " << IndexByteOffset << ", \"byteLength\": " << IndexByteLength << ", \"target\": 34963},\n"
        << "    {\"buffer\": 0, \"byteOffset\": " << ImageByteOffset << ", \"byteLength\": " << ImageByteLength << "}\n"
        << "  ],\n"
        << "  \"accessors\": [\n"
        << "    {\"bufferView\": 0, \"componentType\": 5126, \"count\": 3, \"type\": \"VEC3\", \"min\": [0, 0, 0], \"max\": [1, 1, 0]},\n"
        << "    {\"bufferView\": 1, \"componentType\": 5126, \"count\": 3, \"type\": \"VEC2\"},\n"
        << "    {\"bufferView\": 2, \"componentType\": 5123, \"count\": 3, \"type\": \"SCALAR\"}\n"
        << "  ],\n"
        << "  \"images\": [{\"bufferView\": 3, \"mimeType\": \"image/png\", \"name\": \"EmbeddedTexture\"}],\n"
        << "  \"samplers\": [{\"magFilter\": 9729, \"minFilter\": 9729, \"wrapS\": 10497, \"wrapT\": 10497}],\n"
        << "  \"textures\": [{\"source\": 0, \"sampler\": 0}],\n"
        << "  \"materials\": [{\"name\": \"Mat0\", \"pbrMetallicRoughness\": {\"baseColorTexture\": {\"index\": 0}, \"metallicFactor\": 0.0, \"roughnessFactor\": 1.0}}],\n"
        << "  \"meshes\": [{\"primitives\": [{\"attributes\": {\"POSITION\": 0, \"TEXCOORD_0\": 1}, \"indices\": 2, \"material\": 0}]}],\n"
        << "  \"nodes\": [{\"mesh\": 0}],\n"
        << "  \"scenes\": [{\"nodes\": [0]}],\n"
        << "  \"scene\": 0\n"
        << "}\n";

    const std::filesystem::path GltfPath = RootDir / "embedded_textures.gltf";
    WriteTextFile(GltfPath, Json.str());
    return GltfPath;
}

std::filesystem::path WriteEmbeddedTextureGlbFixture(const std::filesystem::path& RootDir)
{
    std::filesystem::create_directories(RootDir);

    std::vector<std::uint8_t> Buffer{};
    Buffer.reserve(256);

    AppendFloat(Buffer, 0.0f);
    AppendFloat(Buffer, 0.0f);
    AppendFloat(Buffer, 0.0f);
    AppendFloat(Buffer, 1.0f);
    AppendFloat(Buffer, 0.0f);
    AppendFloat(Buffer, 0.0f);
    AppendFloat(Buffer, 0.0f);
    AppendFloat(Buffer, 1.0f);
    AppendFloat(Buffer, 0.0f);
    constexpr std::uint32_t PositionByteLength = 9u * sizeof(float);

    AppendFloat(Buffer, 0.0f);
    AppendFloat(Buffer, 0.0f);
    AppendFloat(Buffer, 1.0f);
    AppendFloat(Buffer, 0.0f);
    AppendFloat(Buffer, 0.0f);
    AppendFloat(Buffer, 1.0f);
    constexpr std::uint32_t TexcoordByteOffset = PositionByteLength;
    constexpr std::uint32_t TexcoordByteLength = 6u * sizeof(float);

    AppendU16(Buffer, 0u);
    AppendU16(Buffer, 1u);
    AppendU16(Buffer, 2u);
    constexpr std::uint32_t IndexByteOffset = TexcoordByteOffset + TexcoordByteLength;
    constexpr std::uint32_t IndexByteLength = 3u * sizeof(std::uint16_t);

    while ((Buffer.size() % 4u) != 0u)
    {
        Buffer.push_back(0u);
    }

    static constexpr std::array<std::uint8_t, 90> kPng{
        0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A,
        0x00,0x00,0x00,0x0D,0x49,0x48,0x44,0x52,
        0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,
        0x08,0x02,0x00,0x00,0x00,0x90,0x77,0x53,
        0xDE,0x00,0x00,0x00,0x09,0x70,0x48,0x59,
        0x73,0x00,0x00,0x0B,0x13,0x00,0x00,0x0B,
        0x13,0x01,0x00,0x9A,0x9C,0x18,0x00,0x00,
        0x00,0x0C,0x49,0x44,0x41,0x54,0x08,0x99,
        0x63,0xF8,0xCF,0xC0,0x00,0x00,0x03,0x01,
        0x01,0x00,0x9C,0xE3,0xBF,0x59,0x00,0x00,
        0x00,0x00,0x49,0x45,0x4E,0x44,0xAE,0x42,
        0x60,0x82
    };

    const std::uint32_t ImageByteOffset = static_cast<std::uint32_t>(Buffer.size());
    Buffer.insert(Buffer.end(), kPng.begin(), kPng.end());
    const std::uint32_t ImageByteLength = static_cast<std::uint32_t>(kPng.size());
    const std::uint32_t BufferByteLength = static_cast<std::uint32_t>(Buffer.size());

    std::ostringstream Json{};
    Json
        << "{\n"
        << "  \"asset\": {\"version\": \"2.0\"},\n"
        << "  \"buffers\": [{\"byteLength\": " << BufferByteLength << "}],\n"
        << "  \"bufferViews\": [\n"
        << "    {\"buffer\": 0, \"byteOffset\": 0, \"byteLength\": " << PositionByteLength << ", \"target\": 34962},\n"
        << "    {\"buffer\": 0, \"byteOffset\": " << TexcoordByteOffset << ", \"byteLength\": " << TexcoordByteLength << ", \"target\": 34962},\n"
        << "    {\"buffer\": 0, \"byteOffset\": " << IndexByteOffset << ", \"byteLength\": " << IndexByteLength << ", \"target\": 34963},\n"
        << "    {\"buffer\": 0, \"byteOffset\": " << ImageByteOffset << ", \"byteLength\": " << ImageByteLength << "}\n"
        << "  ],\n"
        << "  \"accessors\": [\n"
        << "    {\"bufferView\": 0, \"componentType\": 5126, \"count\": 3, \"type\": \"VEC3\", \"min\": [0, 0, 0], \"max\": [1, 1, 0]},\n"
        << "    {\"bufferView\": 1, \"componentType\": 5126, \"count\": 3, \"type\": \"VEC2\"},\n"
        << "    {\"bufferView\": 2, \"componentType\": 5123, \"count\": 3, \"type\": \"SCALAR\"}\n"
        << "  ],\n"
        << "  \"images\": [{\"bufferView\": 3, \"mimeType\": \"image/png\", \"name\": \"EmbeddedTexture\"}],\n"
        << "  \"samplers\": [{\"magFilter\": 9729, \"minFilter\": 9729, \"wrapS\": 10497, \"wrapT\": 10497}],\n"
        << "  \"textures\": [{\"source\": 0, \"sampler\": 0}],\n"
        << "  \"materials\": [{\"name\": \"Mat0\", \"pbrMetallicRoughness\": {\"baseColorTexture\": {\"index\": 0}, \"metallicFactor\": 0.0, \"roughnessFactor\": 1.0}}],\n"
        << "  \"meshes\": [{\"primitives\": [{\"attributes\": {\"POSITION\": 0, \"TEXCOORD_0\": 1}, \"indices\": 2, \"material\": 0}]}],\n"
        << "  \"nodes\": [{\"mesh\": 0}],\n"
        << "  \"scenes\": [{\"nodes\": [0]}],\n"
        << "  \"scene\": 0\n"
        << "}\n";

    const std::string JsonText = Json.str();
    std::vector<std::uint8_t> JsonBytes(JsonText.begin(), JsonText.end());
    while ((JsonBytes.size() % 4u) != 0u)
    {
        JsonBytes.push_back(static_cast<std::uint8_t>(' '));
    }

    std::vector<std::uint8_t> BinBytes = Buffer;
    while ((BinBytes.size() % 4u) != 0u)
    {
        BinBytes.push_back(0u);
    }

    std::vector<std::uint8_t> Glb{};
    Glb.reserve(12u + 8u + JsonBytes.size() + 8u + BinBytes.size());
    AppendU32(Glb, 0x46546C67u);
    AppendU32(Glb, 2u);
    AppendU32(Glb, static_cast<std::uint32_t>(12u + 8u + JsonBytes.size() + 8u + BinBytes.size()));
    AppendU32(Glb, static_cast<std::uint32_t>(JsonBytes.size()));
    AppendU32(Glb, 0x4E4F534Au);
    Glb.insert(Glb.end(), JsonBytes.begin(), JsonBytes.end());
    AppendU32(Glb, static_cast<std::uint32_t>(BinBytes.size()));
    AppendU32(Glb, 0x004E4942u);
    Glb.insert(Glb.end(), BinBytes.begin(), BinBytes.end());

    const std::filesystem::path GlbPath = RootDir / "embedded_textures.glb";
    WriteBinaryFile(GlbPath, Glb);
    return GlbPath;
}

struct ScopedAssetRoot
{
    std::filesystem::path Previous{};

    explicit ScopedAssetRoot(const std::filesystem::path& Path)
        : Previous(SPathResolver::Instance().AssetRoot())
    {
        REQUIRE(SPathResolver::Instance().SetAssetRoot(Path));
    }

    ~ScopedAssetRoot()
    {
        (void)SPathResolver::Instance().SetAssetRoot(Previous);
    }
};

struct TestEditorHost final : IEditorServiceHost
{
    GameRuntime Runtime{};
    EditorAssetService AssetService{};
    Conduit::Editor::ConduitEditorService ConduitService{};

    TestEditorHost()
    {
        REQUIRE(Runtime.Init({}));
        EditorServiceContext Context(*this);
        REQUIRE(AssetService.Initialize(Context));
        REQUIRE(ConduitService.Initialize(Context));
    }

    ~TestEditorHost() override
    {
        EditorServiceContext Context(*this);
        ConduitService.Shutdown(Context);
        AssetService.Shutdown(Context);
        Runtime.Shutdown();
    }

    [[nodiscard]] GameRuntime& RuntimeForServices() override
    {
        return Runtime;
    }

    [[nodiscard]] const GameRuntime& RuntimeForServices() const override
    {
        return Runtime;
    }

    [[nodiscard]] IEditorService* ResolveServiceForContext(const std::type_index& Type) override
    {
        if (Type == std::type_index(typeid(EditorAssetService)))
        {
            return &AssetService;
        }
        if (Type == std::type_index(typeid(Conduit::Editor::ConduitEditorService)))
        {
            return &ConduitService;
        }
        return nullptr;
    }

    [[nodiscard]] const IEditorService* ResolveServiceForContext(const std::type_index& Type) const override
    {
        if (Type == std::type_index(typeid(EditorAssetService)))
        {
            return &AssetService;
        }
        if (Type == std::type_index(typeid(Conduit::Editor::ConduitEditorService)))
        {
            return &ConduitService;
        }
        return nullptr;
    }
};

template <typename TAsset>
std::vector<const EditorAssetService::DiscoveredAsset*> FindDiscoveredAssetsByType(
    const EditorAssetService& Service,
    const std::string_view Prefix = {})
{
    std::vector<const EditorAssetService::DiscoveredAsset*> Matches{};
    for (const auto& Asset : Service.Assets())
    {
        if (Asset.AssetType != StaticTypeId<TAsset>())
        {
            continue;
        }
        if (!Prefix.empty() && Asset.Key.rfind(Prefix, 0u) != 0u)
        {
            continue;
        }
        Matches.push_back(&Asset);
    }
    return Matches;
}

std::size_t CountNodesOfType(World& WorldRef, const TypeId& Type, const bool RootsOnly = false)
{
    std::size_t Count = 0;
    WorldRef.ForEachNode([&Count, Type, RootsOnly](const NodeHandle&, BaseNode& Node) {
        if (RootsOnly && !Node.Parent().IsNull())
        {
            return;
        }

        if (TypeRegistry::Instance().IsA(Node.TypeKey(), Type))
        {
            ++Count;
        }
    });
    return Count;
}

#if defined(SNAPI_GF_ENABLE_UI)

void CollectElementAndDescendants(SnAPI::UI::UIContext& Context,
                                  const SnAPI::UI::ElementId Root,
                                  std::vector<SnAPI::UI::ElementId>& Out)
{
    if (Root.Value == 0)
    {
        return;
    }

    Out.push_back(Root);
    auto& Element = Context.GetElement(Root);
    for (std::uint32_t Index = 0; Index < Element.ChildCount(); ++Index)
    {
        CollectElementAndDescendants(Context, Element.ChildAt(Index).GetId(), Out);
    }
}

std::optional<SnAPI::UI::ElementId> FindTextElementByText(SnAPI::UI::UIContext& Context,
                                                          const SnAPI::UI::ElementId Root,
                                                          std::string_view Text)
{
    std::vector<SnAPI::UI::ElementId> Elements{};
    CollectElementAndDescendants(Context, Root, Elements);
    for (const SnAPI::UI::ElementId Id : Elements)
    {
        auto* Label = dynamic_cast<SnAPI::UI::UIText*>(&Context.GetElement(Id));
        if (!Label)
        {
            continue;
        }

        if (Label->Properties().GetPropertyOr(SnAPI::UI::UIText::TextKey, std::string{}) == Text)
        {
            return Id;
        }
    }

    return std::nullopt;
}

std::vector<SnAPI::UI::UINumberField*> FindNumberFieldsUnder(SnAPI::UI::UIContext& Context,
                                                             const SnAPI::UI::ElementId Root)
{
    std::vector<SnAPI::UI::ElementId> Elements{};
    CollectElementAndDescendants(Context, Root, Elements);

    std::vector<SnAPI::UI::UINumberField*> Result{};
    for (const SnAPI::UI::ElementId Id : Elements)
    {
        if (auto* NumberField = dynamic_cast<SnAPI::UI::UINumberField*>(&Context.GetElement(Id)))
        {
            Result.push_back(NumberField);
        }
    }

    return Result;
}

std::vector<SnAPI::UI::UIComboBox*> FindComboBoxesUnder(SnAPI::UI::UIContext& Context,
                                                        const SnAPI::UI::ElementId Root)
{
    std::vector<SnAPI::UI::ElementId> Elements{};
    CollectElementAndDescendants(Context, Root, Elements);

    std::vector<SnAPI::UI::UIComboBox*> Result{};
    for (const SnAPI::UI::ElementId Id : Elements)
    {
        if (auto* ComboBox = dynamic_cast<SnAPI::UI::UIComboBox*>(&Context.GetElement(Id)))
        {
            Result.push_back(ComboBox);
        }
    }

    return Result;
}

std::vector<SnAPI::UI::UICheckbox*> FindCheckboxesUnder(SnAPI::UI::UIContext& Context,
                                                        const SnAPI::UI::ElementId Root)
{
    std::vector<SnAPI::UI::ElementId> Elements{};
    CollectElementAndDescendants(Context, Root, Elements);

    std::vector<SnAPI::UI::UICheckbox*> Result{};
    for (const SnAPI::UI::ElementId Id : Elements)
    {
        if (auto* Checkbox = dynamic_cast<SnAPI::UI::UICheckbox*>(&Context.GetElement(Id)))
        {
            Result.push_back(Checkbox);
        }
    }

    return Result;
}

std::vector<SnAPI::UI::UIAccordion*> FindAccordionsUnder(SnAPI::UI::UIContext& Context,
                                                         const SnAPI::UI::ElementId Root)
{
    std::vector<SnAPI::UI::ElementId> Elements{};
    CollectElementAndDescendants(Context, Root, Elements);

    std::vector<SnAPI::UI::UIAccordion*> Result{};
    for (const SnAPI::UI::ElementId Id : Elements)
    {
        if (auto* Accordion = dynamic_cast<SnAPI::UI::UIAccordion*>(&Context.GetElement(Id)))
        {
            Result.push_back(Accordion);
        }
    }

    return Result;
}

#endif

} // namespace

TEST_CASE("Editor asset discovery shows source files and skips cooked packs", "[Assets][Editor][Source]")
{
    RegisterBuiltinTypes();

    TestEditorHost Host{};
    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);
    EditorServiceContext Context(Host);

    MaterialAsset Material{};
    Material.ShaderModule = "DiscoveryShader";
    auto MaterialJson = SerializeAuthoredAssetToJson(Material);
    REQUIRE(MaterialJson);

    Conduit::GraphAsset Graph{};
    Graph.Name = "DiscoveryGraph";
    auto GraphJson = SerializeAuthoredAssetToJson(Graph);
    REQUIRE(GraphJson);

    {
        std::error_code Ec{};
        std::filesystem::create_directories(Root.Path / "Levels", Ec);
        REQUIRE_FALSE(Ec);
        std::ofstream Pack(Root.Path / "Levels" / "Ignored.snpak", std::ios::binary | std::ios::trunc);
        REQUIRE(Pack.is_open());
        Pack << "not a real pack";
    }
    {
        std::error_code Ec{};
        std::filesystem::create_directories(Root.Path / "Rendering", Ec);
        REQUIRE_FALSE(Ec);
        std::ofstream Out(Root.Path / "Rendering" / "Visible.material", std::ios::binary | std::ios::trunc);
        REQUIRE(Out.is_open());
        Out.write(MaterialJson->data(), static_cast<std::streamsize>(MaterialJson->size()));
    }
    {
        std::error_code Ec{};
        std::filesystem::create_directories(Root.Path / "Conduit", Ec);
        REQUIRE_FALSE(Ec);
        std::ofstream Out(Root.Path / "Conduit" / "Visible.conduitgraph", std::ios::binary | std::ios::trunc);
        REQUIRE(Out.is_open());
        Out.write(GraphJson->data(), static_cast<std::streamsize>(GraphJson->size()));
    }

    REQUIRE(Host.AssetService.RefreshDiscovery());

    const auto& Assets = Host.AssetService.Assets();
    CHECK_FALSE(Assets.empty());
    CHECK(std::none_of(Assets.begin(), Assets.end(), [](const EditorAssetService::DiscoveredAsset& Asset) {
        return Asset.Key.ends_with(".snpak");
    }));
    CHECK(std::any_of(Assets.begin(), Assets.end(), [](const EditorAssetService::DiscoveredAsset& Asset) {
        return Asset.Key == "Rendering/Visible.material" && Asset.AssetType == StaticTypeId<MaterialAsset>();
    }));
    CHECK(std::any_of(Assets.begin(), Assets.end(), [](const EditorAssetService::DiscoveredAsset& Asset) {
        return Asset.Key == "Conduit/Visible.conduitgraph" &&
               Asset.AssetType == StaticTypeId<Conduit::GraphAsset>();
    }));

    (void)Context;
}

TEST_CASE("Editor asset service can create and persist generic authored source assets", "[Assets][Editor][Source]")
{
    RegisterBuiltinTypes();

    TestEditorHost Host{};
    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);
    EditorServiceContext Context(Host);

    REQUIRE(Host.AssetService.RefreshDiscovery());
    REQUIRE(Host.AssetService.CreateSourceAssetByType(Context, StaticTypeId<MaterialAsset>(), "UnitTestMaterial", "Rendering"));

    const auto* Created = Host.AssetService.SelectedAsset();
    REQUIRE(Created != nullptr);
    const std::string CreatedKey = Created->Key;
    const std::string CreatedSourcePath = Created->SourceFilePath;
    REQUIRE(CreatedKey == "Rendering/UnitTestMaterial.material");
    REQUIRE(std::filesystem::path(CreatedSourcePath).lexically_normal() ==
            (Root.Path / "Rendering" / "UnitTestMaterial.material").lexically_normal());

    REQUIRE(Host.AssetService.OpenAssetEditorByKey(CreatedKey));
    auto Session = Host.AssetService.AssetEditorSession();
    REQUIRE(Session.IsOpen);
    REQUIRE(Session.TargetType == StaticTypeId<MaterialAsset>());
    auto* Material = static_cast<MaterialAsset*>(Session.TargetObject);
    REQUIRE(Material != nullptr);
    Material->ShaderModule = "SavedUnitTestShader";
    Material->FeatureAlphaBlend = true;
    Host.AssetService.NotifyActiveAssetEditorRuntimeMutated(Session.TargetType, Session.TargetObject);

    CHECK(Host.AssetService.AssetEditorSession().RuntimeDirty);

    REQUIRE(Host.AssetService.SaveAssetByKey(CreatedKey));

    MaterialAsset SavedMaterial{};
    REQUIRE(DeserializeAuthoredAssetFromJson(
        ReadTextFile(Root.Path / "Rendering" / "UnitTestMaterial.material"),
        SavedMaterial));
    CHECK(SavedMaterial.ShaderModule == "SavedUnitTestShader");
    CHECK(SavedMaterial.FeatureAlphaBlend);

    REQUIRE(Host.AssetService.RenameAssetByKey("Rendering/UnitTestMaterial.material", "RenamedMaterial"));
    const auto* Renamed = Host.AssetService.SelectedAsset();
    REQUIRE(Renamed != nullptr);
    CHECK(Renamed->Key == "Rendering/RenamedMaterial.material");
    CHECK(std::filesystem::exists(Root.Path / "Rendering" / "RenamedMaterial.material"));
    CHECK_FALSE(std::filesystem::exists(Root.Path / "Rendering" / "UnitTestMaterial.material"));

    REQUIRE(Host.AssetService.DeleteAssetByKey("Rendering/RenamedMaterial.material"));
    CHECK_FALSE(std::filesystem::exists(Root.Path / "Rendering" / "RenamedMaterial.material"));
}

TEST_CASE("Editor asset service creates typed prefabs that open in the hierarchy editor", "[Assets][Editor][Source]")
{
    EnsureSourceAssetEditorNodeHostRegistered();

    TestEditorHost Host{};
    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);
    EditorServiceContext Context(Host);

    REQUIRE(Host.AssetService.RefreshDiscovery());
    REQUIRE(Host.AssetService.CreatePrefabSourceAssetByNodeType(
        Context,
        StaticTypeId<SourceAssetEditorNodeHost>(),
        "TypedEnemy",
        "Gameplay"));

    const auto* Created = Host.AssetService.SelectedAsset();
    REQUIRE(Created != nullptr);
    const std::string CreatedKey = Created->Key;
    REQUIRE(CreatedKey == "Gameplay/TypedEnemy.prefab");
    REQUIRE(std::filesystem::exists(Root.Path / "Gameplay" / "TypedEnemy.prefab"));

    REQUIRE(Host.AssetService.OpenAssetEditorByKey(CreatedKey));
    auto Session = Host.AssetService.AssetEditorSession();
    REQUIRE(Session.IsOpen);
    REQUIRE(Session.CanEditHierarchy);
    REQUIRE(Session.TargetType == StaticTypeId<SourceAssetEditorNodeHost>());
    REQUIRE(Session.SelectedNode.IsValidSlowByUuid());
    REQUIRE(Session.Nodes.size() == 1);

    REQUIRE(Host.AssetService.AddAssetEditorComponent(Session.SelectedNode, StaticTypeId<TransformComponent>()));
    REQUIRE(Host.AssetService.AddAssetEditorNode(Session.SelectedNode, StaticTypeId<BaseNode>()));
    Host.AssetService.TickAssetEditorSession(0.25f);
    CHECK(Host.AssetService.AssetEditorSession().RuntimeDirty);

    const auto SaveResult = Host.AssetService.SaveActiveAssetEditor();
    INFO("save error: " << (SaveResult ? std::string("ok") : SaveResult.error().Message));
    REQUIRE(SaveResult);

    NodeAsset SavedPrefab{};
    REQUIRE(DeserializeAuthoredAssetFromJson(
        ReadTextFile(Root.Path / "Gameplay" / "TypedEnemy.prefab"),
        SavedPrefab));
    REQUIRE(SavedPrefab.Nodes.size() == 1);
    CHECK(SavedPrefab.Nodes.front().Type == StaticTypeId<SourceAssetEditorNodeHost>());
    REQUIRE(SavedPrefab.Nodes.front().Components.size() == 1);
    CHECK(SavedPrefab.Nodes.front().Components.front().Type == StaticTypeId<TransformComponent>());
    REQUIRE(SavedPrefab.Nodes.front().Children.size() == 1);
    CHECK(SavedPrefab.Nodes.front().Children.front().Type == StaticTypeId<BaseNode>());
}

TEST_CASE("Typed asset refs enumerate source prefabs before they are opened in the asset editor", "[Assets][Editor][Source]")
{
    RegisterBuiltinTypes();
    REQUIRE(TypeAutoRegistry::Instance().Ensure(StaticTypeId<PawnBase>()));
#if defined(SNAPI_GF_ENABLE_RENDERER)
    REQUIRE(TypeAutoRegistry::Instance().Ensure(StaticTypeId<WorldRenderSettings>()));
#endif

    TestEditorHost Host{};
    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);
    EditorServiceContext Context(Host);

    World PawnWorld("TypedAssetRefEnumerationPawnWorld");
    auto PawnHandleResult = PawnWorld.CreateNode(StaticTypeId<PawnBase>(), "UnitPawn");
    REQUIRE(PawnHandleResult);
    auto PawnAssetResult = CaptureNodeAsset(*PawnHandleResult->Borrowed());
    REQUIRE(PawnAssetResult);
    auto PawnJson = SerializeAuthoredAssetToJson(*PawnAssetResult);
    REQUIRE(PawnJson);
    WriteTextFile(Root.Path / "Gameplay" / "UnitPawn.prefab", *PawnJson);

#if defined(SNAPI_GF_ENABLE_RENDERER)
    World RenderWorld("TypedAssetRefEnumerationRenderWorld");
    auto RenderHandleResult = RenderWorld.CreateNode(StaticTypeId<WorldRenderSettings>(), "UnitRenderSettings");
    REQUIRE(RenderHandleResult);
    auto RenderAssetResult = CaptureNodeAsset(*RenderHandleResult->Borrowed());
    REQUIRE(RenderAssetResult);
    auto RenderJson = SerializeAuthoredAssetToJson(*RenderAssetResult);
    REQUIRE(RenderJson);
    WriteTextFile(Root.Path / "Rendering" / "UnitRenderSettings.prefab", *RenderJson);
#endif

    REQUIRE(Host.AssetService.RefreshDiscovery());

    const auto PawnEntries = TAssetRef<PawnBase>::EnumerateCompatibleAssets();
    CHECK(std::any_of(PawnEntries.begin(), PawnEntries.end(), [](const TAssetRef<PawnBase>::TEntry& Entry) {
        return Entry.Name == "Gameplay/UnitPawn.prefab";
    }));

#if defined(SNAPI_GF_ENABLE_RENDERER)
    const auto RenderEntries = TAssetRef<WorldRenderSettings>::EnumerateCompatibleAssets();
    CHECK(std::any_of(RenderEntries.begin(), RenderEntries.end(), [](const TAssetRef<WorldRenderSettings>::TEntry& Entry) {
        return Entry.Name == "Rendering/UnitRenderSettings.prefab";
    }));
#endif

    (void)Context;
}

TEST_CASE("Editor asset service creates PawnBase prefabs with registered default components",
          "[Assets][Editor][Source]")
{
    RegisterBuiltinTypes();
    REQUIRE(TypeAutoRegistry::Instance().Ensure(StaticTypeId<PawnBase>()));

    TestEditorHost Host{};
    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);
    EditorServiceContext Context(Host);

    REQUIRE(Host.AssetService.RefreshDiscovery());
    const auto CreateResult = Host.AssetService.CreatePrefabSourceAssetByNodeType(
        Context,
        StaticTypeId<PawnBase>(),
        "TypedPawn",
        "Gameplay");
    REQUIRE(CreateResult);

    const auto* Created = Host.AssetService.SelectedAsset();
    REQUIRE(Created != nullptr);
    const std::string CreatedKey = Created->Key;
    REQUIRE(CreatedKey == "Gameplay/TypedPawn.prefab");
    REQUIRE(std::filesystem::exists(Root.Path / "Gameplay" / "TypedPawn.prefab"));

    NodeAsset SavedPrefab{};
    REQUIRE(DeserializeAuthoredAssetFromJson(
        ReadTextFile(Root.Path / "Gameplay" / "TypedPawn.prefab"),
        SavedPrefab));
    REQUIRE(SavedPrefab.Nodes.size() == 1);
    CHECK(SavedPrefab.Nodes.front().Type == StaticTypeId<PawnBase>());
    CHECK(std::any_of(
        SavedPrefab.Nodes.front().Components.begin(),
        SavedPrefab.Nodes.front().Components.end(),
        [](const NodeComponentAsset& Component) {
            return Component.Type == StaticTypeId<TransformComponent>();
        }));

    REQUIRE(Host.AssetService.OpenAssetEditorByKey(CreatedKey));
    const auto Session = Host.AssetService.AssetEditorSession();
    REQUIRE(Session.IsOpen);
    REQUIRE(Session.TargetType == StaticTypeId<PawnBase>());

    auto* PawnNode = static_cast<PawnBase*>(Session.TargetObject);
    REQUIRE(PawnNode != nullptr);
    CHECK(PawnNode->Component<TransformComponent>());
#if defined(SNAPI_GF_ENABLE_RENDERER)
    CHECK(PawnNode->Component<CameraComponent>());
    CHECK(PawnNode->Component<SprintArmComponent>());
#endif
}

TEST_CASE("Typed prefabs persist default components and saved component settings", "[Assets][Editor][Source]")
{
    EnsureSourceAssetEditorDefaultNodeRegistered();

    TestEditorHost Host{};
    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);
    EditorServiceContext Context(Host);

    REQUIRE(Host.AssetService.RefreshDiscovery());
    REQUIRE(Host.AssetService.CreatePrefabSourceAssetByNodeType(
        Context,
        StaticTypeId<SourceAssetEditorDefaultNode>(),
        "DefaultNode",
        "Gameplay"));

    const auto* Created = Host.AssetService.SelectedAsset();
    REQUIRE(Created != nullptr);
    const std::string CreatedKey = Created->Key;
    REQUIRE(CreatedKey == "Gameplay/DefaultNode.prefab");

    NodeAsset CreatedPrefab{};
    REQUIRE(DeserializeAuthoredAssetFromJson(
        ReadTextFile(Root.Path / "Gameplay" / "DefaultNode.prefab"),
        CreatedPrefab));
    REQUIRE(CreatedPrefab.Nodes.size() == 1);
    CHECK(CreatedPrefab.Nodes.front().Type == StaticTypeId<SourceAssetEditorDefaultNode>());
    CHECK(std::any_of(
        CreatedPrefab.Nodes.front().Components.begin(),
        CreatedPrefab.Nodes.front().Components.end(),
        [](const NodeComponentAsset& Component) {
            return Component.Type == StaticTypeId<TransformComponent>();
        }));

    REQUIRE(Host.AssetService.OpenAssetEditorByKey(CreatedKey));
    auto Session = Host.AssetService.AssetEditorSession();
    REQUIRE(Session.IsOpen);
    REQUIRE(Session.TargetType == StaticTypeId<SourceAssetEditorDefaultNode>());

    auto* Node = static_cast<SourceAssetEditorDefaultNode*>(Session.TargetObject);
    REQUIRE(Node != nullptr);
    NodeHandle NodeHandleValue = Node->Handle();
    auto* Transform = static_cast<TransformComponent*>(
        Node->World()->BorrowedComponent(NodeHandleValue, StaticTypeId<TransformComponent>()));
    REQUIRE(Transform != nullptr);
    Transform->Position = Vec3(12.0, 34.0, 56.0);
    Host.AssetService.NotifyActiveAssetEditorRuntimeMutated(Session.TargetType, Session.TargetObject);

    REQUIRE(Host.AssetService.SaveAssetByKey(CreatedKey));

    Host.AssetService.CloseAssetEditor();
    REQUIRE(Host.AssetService.OpenAssetEditorByKey(CreatedKey));
    Session = Host.AssetService.AssetEditorSession();
    REQUIRE(Session.IsOpen);

    auto* ReopenedNode = static_cast<SourceAssetEditorDefaultNode*>(Session.TargetObject);
    REQUIRE(ReopenedNode != nullptr);
    NodeHandle ReopenedNodeHandle = ReopenedNode->Handle();
    auto* ReopenedTransform = static_cast<TransformComponent*>(
        ReopenedNode->World()->BorrowedComponent(ReopenedNodeHandle, StaticTypeId<TransformComponent>()));
    REQUIRE(ReopenedTransform != nullptr);
    CHECK(ReopenedTransform->Position.x() == Catch::Approx(12.0));
    CHECK(ReopenedTransform->Position.y() == Catch::Approx(34.0));
    CHECK(ReopenedTransform->Position.z() == Catch::Approx(56.0));
}

#if defined(SNAPI_GF_ENABLE_UI)

TEST_CASE("UI property panel edits on typed prefabs persist component settings through save and reopen",
          "[Assets][Editor][Source][UI]")
{
    EnsureSourceAssetEditorCameraNodeRegistered();

    auto Host = std::make_unique<TestEditorHost>();
    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);
    EditorServiceContext Context(*Host);

    REQUIRE(Host->AssetService.RefreshDiscovery());
    REQUIRE(Host->AssetService.CreatePrefabSourceAssetByNodeType(
        Context,
        StaticTypeId<SourceAssetEditorCameraNode>(),
        "UICameraNode",
        "Gameplay"));

    const auto* Created = Host->AssetService.SelectedAsset();
    REQUIRE(Created != nullptr);
    const std::string CreatedKey = Created->Key;
    REQUIRE(CreatedKey == "Gameplay/UICameraNode.prefab");

    REQUIRE(Host->AssetService.OpenAssetEditorByKey(CreatedKey));
    const auto Session = Host->AssetService.AssetEditorSession();
    REQUIRE(Session.IsOpen);

    auto* RootNode = static_cast<BaseNode*>(Session.TargetObject);
    REQUIRE(RootNode != nullptr);
    NodeHandle RootNodeHandle = RootNode->Handle();

    for (int Index = 0; Index < 8; ++Index)
    {
        auto ExtraNodeResult = RootNode->World()->CreateNode(
            StaticTypeId<SourceAssetEditorCameraNode>(),
            "ExtraCameraNode" + std::to_string(Index));
        REQUIRE(ExtraNodeResult);
        NodeHandle ExtraNodeHandle = *ExtraNodeResult;
        REQUIRE(RootNode->World()->RequestNodeOnCreate(ExtraNodeHandle));
    }

    RootNode = RootNode->World()->BorrowedNode(RootNodeHandle);
    REQUIRE(RootNode != nullptr);

    auto UiContext = std::make_unique<SnAPI::UI::UIContext>();
    UiContext->EnsureDefaultSetup();
    UiContext->SetViewportSize(900.0f, 1200.0f);
    UiContext->RegisterElementType<UIPropertyPanel>();

    auto RootBuilder = UiContext->Root();
    RootBuilder.Element().Padding().Set(0.0f);
    RootBuilder.Element().Gap().Set(0.0f);

    auto PanelBuilder = RootBuilder.Add(UIPropertyPanel{});
    auto& Panel = PanelBuilder.Element();
    Panel.Width().Set(SnAPI::UI::Sizing::Fill());
    Panel.Height().Set(SnAPI::UI::Sizing::Fill());

    REQUIRE(Panel.BindNode(RootNode));

    SnAPI::UI::RenderPacketList Packets{};
    UiContext->BuildRenderPackets(Packets);

    const auto LabelId = FindTextElementByText(*UiContext, PanelBuilder.Handle().Id, "Fov Degrees");
    REQUIRE(LabelId.has_value());

    const SnAPI::UI::ElementId RowId = UiContext->GetParent(*LabelId);
    REQUIRE(RowId.Value != 0);

    auto NumberFields = FindNumberFieldsUnder(*UiContext, RowId);
    REQUIRE(NumberFields.size() == 1);

    NumberFields.front()->Value().Set(91.0);
    UiContext->Tick(0.016f);

    auto* EditedComponent = static_cast<CameraComponent*>(
        RootNode->World()->BorrowedComponent(
            RootNodeHandle,
            StaticTypeId<CameraComponent>()));
    REQUIRE(EditedComponent != nullptr);
    CHECK(EditedComponent->GetSettings().FovDegrees == Catch::Approx(91.0f));

    Host->AssetService.NotifyActiveAssetEditorRuntimeMutated(Session.TargetType, Session.TargetObject);
    const bool RuntimeDirty = Host->AssetService.AssetEditorSession().RuntimeDirty;
    CHECK(RuntimeDirty);
    REQUIRE(Host->AssetService.SaveActiveAssetEditor());

    Host->AssetService.CloseAssetEditor();
    REQUIRE(Host->AssetService.OpenAssetEditorByKey(CreatedKey));

    const auto ReopenedSession = Host->AssetService.AssetEditorSession();
    REQUIRE(ReopenedSession.IsOpen);

    auto* ReopenedNode = static_cast<BaseNode*>(ReopenedSession.TargetObject);
    REQUIRE(ReopenedNode != nullptr);

    NodeHandle ReopenedCameraNodeHandle = ReopenedNode->Handle();
    auto* ReopenedComponent = static_cast<CameraComponent*>(
        ReopenedNode->World()->BorrowedComponent(
            ReopenedCameraNodeHandle,
            StaticTypeId<CameraComponent>()));
    REQUIRE(ReopenedComponent != nullptr);
    CHECK(ReopenedComponent->GetSettings().FovDegrees == Catch::Approx(91.0f));
}

TEST_CASE("UI property panel persists material instance parent material refs through save and reopen",
          "[Assets][Editor][Source][UI]")
{
    RegisterBuiltinTypes();

    TestEditorHost Host{};
    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);

    std::error_code DirectoryError{};
    std::filesystem::create_directories(Root.Path / "Rendering", DirectoryError);
    REQUIRE_FALSE(DirectoryError);

    MaterialAsset ParentMaterial{};
    ParentMaterial.ShaderModule = "UnitTestShader";
    ParentMaterial.ShadingModel = "GBufferShadingModel";
    auto ParentJson = SerializeAuthoredAssetToJson(ParentMaterial);
    REQUIRE(ParentJson);

    MaterialInstanceAsset InstanceAsset{};
    MaterialScalarParamPayload Roughness{};
    Roughness.Name = "Roughness";
    Roughness.Value = 0.42f;
    InstanceAsset.Scalars.push_back(Roughness);
    auto InstanceJson = SerializeAuthoredAssetToJson(InstanceAsset);
    REQUIRE(InstanceJson);

    {
        std::ofstream Out(Root.Path / "Rendering" / "Parent.material", std::ios::binary | std::ios::trunc);
        REQUIRE(Out.is_open());
        Out.write(ParentJson->data(), static_cast<std::streamsize>(ParentJson->size()));
    }
    {
        std::ofstream Out(Root.Path / "Rendering" / "Child.materialinstance", std::ios::binary | std::ios::trunc);
        REQUIRE(Out.is_open());
        Out.write(InstanceJson->data(), static_cast<std::streamsize>(InstanceJson->size()));
    }

    REQUIRE(Host.AssetService.RefreshDiscovery());
    REQUIRE(Host.AssetService.OpenAssetEditorByKey("Rendering/Child.materialinstance"));

    const auto Session = Host.AssetService.AssetEditorSession();
    REQUIRE(Session.IsOpen);
    REQUIRE(Session.TargetType == StaticTypeId<MaterialInstanceAsset>());
    auto* MaterialInstance = static_cast<MaterialInstanceAsset*>(Session.TargetObject);
    REQUIRE(MaterialInstance != nullptr);
    CHECK(MaterialInstance->ParentMaterial.AssetName.empty());
    REQUIRE(MaterialInstance->Scalars.size() == 1u);
    CHECK(MaterialInstance->Scalars.front().Value == Catch::Approx(0.42f));

    auto UiContext = std::make_unique<SnAPI::UI::UIContext>();
    UiContext->EnsureDefaultSetup();
    UiContext->SetViewportSize(900.0f, 1200.0f);
    UiContext->RegisterElementType<UIPropertyPanel>();

    auto RootBuilder = UiContext->Root();
    RootBuilder.Element().Padding().Set(0.0f);
    RootBuilder.Element().Gap().Set(0.0f);

    auto PanelBuilder = RootBuilder.Add(UIPropertyPanel{});
    auto& Panel = PanelBuilder.Element();
    Panel.Width().Set(SnAPI::UI::Sizing::Fill());
    Panel.Height().Set(SnAPI::UI::Sizing::Fill());

    REQUIRE(Panel.BindObject(Session.TargetType, Session.TargetObject));

    SnAPI::UI::RenderPacketList Packets{};
    UiContext->BuildRenderPackets(Packets);

    auto ComboBoxes = FindComboBoxesUnder(*UiContext, PanelBuilder.Handle().Id);
    REQUIRE(ComboBoxes.size() == 1u);
    auto* Combo = ComboBoxes.front();
    REQUIRE(Combo != nullptr);

    const auto& Items = Combo->Items();
    const auto ParentOptionIt = std::find_if(Items.begin(), Items.end(), [](const std::string& Item) {
        return Item.rfind("Rendering/Parent.material [", 0u) == 0u;
    });
    REQUIRE(ParentOptionIt != Items.end());
    REQUIRE(Combo->SelectByText(*ParentOptionIt, true));

    CHECK(MaterialInstance->ParentMaterial.AssetName == "Rendering/Parent.material");
    CHECK(MaterialInstance->ParentMaterial.AssetId == SourceAssetIdFromLogicalName("Rendering/Parent.material").ToString());
    CHECK(MaterialInstance->Scalars.front().Value == Catch::Approx(0.42f));

    auto DirectJson = SerializeAuthoredAssetToJson(*MaterialInstance);
    REQUIRE(DirectJson);
    INFO(*DirectJson);
    MaterialInstanceAsset DirectRoundTrip{};
    AuthoredAssetImportDiagnostics Diagnostics{};
    REQUIRE(DeserializeAuthoredAssetFromJson(*DirectJson, DirectRoundTrip, Diagnostics));
    INFO("Diagnostics count: " << Diagnostics.size());
    if (!Diagnostics.empty())
    {
        FAIL(Diagnostics.front());
    }
    CHECK(DirectRoundTrip.ParentMaterial.AssetName == "Rendering/Parent.material");
    CHECK(DirectRoundTrip.ParentMaterial.AssetId == SourceAssetIdFromLogicalName("Rendering/Parent.material").ToString());

    REQUIRE(Host.AssetService.SaveActiveAssetEditor());

    MaterialInstanceAsset SavedInstance{};
    REQUIRE(DeserializeAuthoredAssetFromJson(
        ReadTextFile(Root.Path / "Rendering" / "Child.materialinstance"),
        SavedInstance));
    CHECK(SavedInstance.ParentMaterial.AssetName == "Rendering/Parent.material");
    CHECK(SavedInstance.ParentMaterial.AssetId == SourceAssetIdFromLogicalName("Rendering/Parent.material").ToString());
    REQUIRE(SavedInstance.Scalars.size() == 1u);
    CHECK(SavedInstance.Scalars.front().Value == Catch::Approx(0.42f));

    Host.AssetService.CloseAssetEditor();
    REQUIRE(Host.AssetService.OpenAssetEditorByKey("Rendering/Child.materialinstance"));

    const auto ReopenedSession = Host.AssetService.AssetEditorSession();
    REQUIRE(ReopenedSession.IsOpen);
    auto* ReopenedInstance = static_cast<MaterialInstanceAsset*>(ReopenedSession.TargetObject);
    REQUIRE(ReopenedInstance != nullptr);
    CHECK(ReopenedInstance->ParentMaterial.AssetName == "Rendering/Parent.material");
    CHECK(ReopenedInstance->ParentMaterial.AssetId == SourceAssetIdFromLogicalName("Rendering/Parent.material").ToString());
    REQUIRE(ReopenedInstance->Scalars.size() == 1u);
    CHECK(ReopenedInstance->Scalars.front().Value == Catch::Approx(0.42f));
}

TEST_CASE("Material instance parent assignment immediately seeds editable params from the parent material",
          "[Assets][Editor][Source][UI]")
{
    RegisterBuiltinTypes();

    TestEditorHost Host{};
    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);

    std::error_code DirectoryError{};
    std::filesystem::create_directories(Root.Path / "Rendering", DirectoryError);
    REQUIRE_FALSE(DirectoryError);

    MaterialAsset ParentMaterial{};
    ParentMaterial.ShaderModule = "DefaultGBufferMaterial";
    ParentMaterial.ShadingModel = "GBufferShadingModel";
    ParentMaterial.FeatureAlbedoMap = true;
    ParentMaterial.FeatureNormalMap = true;
    ParentMaterial.FeatureRoughnessMap = true;
    ParentMaterial.FeatureMetalnessMap = true;
    ParentMaterial.FeatureOcclusionMap = true;
    auto ParentJson = SerializeAuthoredAssetToJson(ParentMaterial);
    REQUIRE(ParentJson);

    MaterialInstanceAsset InstanceAsset{};
    auto InstanceJson = SerializeAuthoredAssetToJson(InstanceAsset);
    REQUIRE(InstanceJson);

    {
        std::ofstream Out(Root.Path / "Rendering" / "Parent.material", std::ios::binary | std::ios::trunc);
        REQUIRE(Out.is_open());
        Out.write(ParentJson->data(), static_cast<std::streamsize>(ParentJson->size()));
    }
    {
        std::ofstream Out(Root.Path / "Rendering" / "Child.materialinstance", std::ios::binary | std::ios::trunc);
        REQUIRE(Out.is_open());
        Out.write(InstanceJson->data(), static_cast<std::streamsize>(InstanceJson->size()));
    }

    REQUIRE(Host.AssetService.RefreshDiscovery());
    REQUIRE(Host.AssetService.OpenAssetEditorByKey("Rendering/Child.materialinstance"));

    const auto Session = Host.AssetService.AssetEditorSession();
    REQUIRE(Session.IsOpen);
    REQUIRE(Session.TargetType == StaticTypeId<MaterialInstanceAsset>());
    auto* MaterialInstance = static_cast<MaterialInstanceAsset*>(Session.TargetObject);
    REQUIRE(MaterialInstance != nullptr);
    CHECK(MaterialInstance->ParentMaterial.AssetName.empty());
    CHECK(MaterialInstance->Scalars.empty());
    CHECK(MaterialInstance->Vectors.empty());
    CHECK(MaterialInstance->Textures.empty());

    auto UiContext = std::make_unique<SnAPI::UI::UIContext>();
    UiContext->EnsureDefaultSetup();
    UiContext->SetViewportSize(900.0f, 1200.0f);
    UiContext->RegisterElementType<UIPropertyPanel>();

    auto RootBuilder = UiContext->Root();
    RootBuilder.Element().Padding().Set(0.0f);
    RootBuilder.Element().Gap().Set(0.0f);

    auto PanelBuilder = RootBuilder.Add(UIPropertyPanel{});
    auto& Panel = PanelBuilder.Element();
    Panel.Width().Set(SnAPI::UI::Sizing::Fill());
    Panel.Height().Set(SnAPI::UI::Sizing::Fill());
    Panel.SetObjectMutatedHandler(
        SnAPI::UI::TDelegate<void(const TypeId&, void*)>::Bind([&Host](const TypeId& RootType, void* RootInstance) {
            Host.AssetService.NotifyActiveAssetEditorRuntimeMutated(RootType, RootInstance);
        }));

    REQUIRE(Panel.BindObject(Session.TargetType, Session.TargetObject));

    SnAPI::UI::RenderPacketList Packets{};
    UiContext->BuildRenderPackets(Packets);

    auto ComboBoxes = FindComboBoxesUnder(*UiContext, PanelBuilder.Handle().Id);
    REQUIRE(ComboBoxes.size() == 1u);
    auto* Combo = ComboBoxes.front();
    REQUIRE(Combo != nullptr);

    const auto& Items = Combo->Items();
    const auto ParentOptionIt = std::find_if(Items.begin(), Items.end(), [](const std::string& Item) {
        return Item.rfind("Rendering/Parent.material [", 0u) == 0u;
    });
    REQUIRE(ParentOptionIt != Items.end());
    REQUIRE(Combo->SelectByText(*ParentOptionIt, true));

    CHECK(MaterialInstance->ParentMaterial.AssetName == "Rendering/Parent.material");
    CHECK(MaterialInstance->ParentMaterial.AssetId == SourceAssetIdFromLogicalName("Rendering/Parent.material").ToString());

    REQUIRE(MaterialInstance->Vectors.size() == 1u);
    CHECK(MaterialInstance->Vectors.front().Name == "Color");
    CHECK(MaterialInstance->Vectors.front().Value[0] == Catch::Approx(1.0f));
    CHECK(MaterialInstance->Vectors.front().Value[1] == Catch::Approx(1.0f));
    CHECK(MaterialInstance->Vectors.front().Value[2] == Catch::Approx(1.0f));
    CHECK(MaterialInstance->Vectors.front().Value[3] == Catch::Approx(1.0f));

    REQUIRE(MaterialInstance->Scalars.size() == 3u);
    const auto RoughnessIt = std::find_if(MaterialInstance->Scalars.begin(),
                                          MaterialInstance->Scalars.end(),
                                          [](const MaterialScalarParamPayload& Param) {
                                              return Param.Name == "Roughness";
                                          });
    const auto MetallicIt = std::find_if(MaterialInstance->Scalars.begin(),
                                         MaterialInstance->Scalars.end(),
                                         [](const MaterialScalarParamPayload& Param) {
                                             return Param.Name == "Metallic";
                                         });
    const auto OcclusionIt = std::find_if(MaterialInstance->Scalars.begin(),
                                          MaterialInstance->Scalars.end(),
                                          [](const MaterialScalarParamPayload& Param) {
                                              return Param.Name == "Occlusion";
                                          });
    REQUIRE(RoughnessIt != MaterialInstance->Scalars.end());
    REQUIRE(MetallicIt != MaterialInstance->Scalars.end());
    REQUIRE(OcclusionIt != MaterialInstance->Scalars.end());
    CHECK(RoughnessIt->Value == Catch::Approx(0.8f));
    CHECK(MetallicIt->Value == Catch::Approx(0.0f));
    CHECK(OcclusionIt->Value == Catch::Approx(1.0f));

    REQUIRE(MaterialInstance->Textures.size() == 3u);
    CHECK(std::find_if(MaterialInstance->Textures.begin(),
                       MaterialInstance->Textures.end(),
                       [](const MaterialTextureParamPayload& Param) {
                           return Param.SlotName == "Material_Albedo";
                       }) != MaterialInstance->Textures.end());
    CHECK(std::find_if(MaterialInstance->Textures.begin(),
                       MaterialInstance->Textures.end(),
                       [](const MaterialTextureParamPayload& Param) {
                           return Param.SlotName == "Material_Normal";
                       }) != MaterialInstance->Textures.end());
    CHECK(std::find_if(MaterialInstance->Textures.begin(),
                       MaterialInstance->Textures.end(),
                       [](const MaterialTextureParamPayload& Param) {
                           return Param.SlotName == "Material_ORM";
                       }) != MaterialInstance->Textures.end());
}

TEST_CASE("Source material instance with stale empty params is repaired from its parent on open",
          "[Assets][Editor][Source]")
{
    RegisterBuiltinTypes();

    TestEditorHost Host{};
    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);

    std::error_code DirectoryError{};
    std::filesystem::create_directories(Root.Path / "Rendering", DirectoryError);
    REQUIRE_FALSE(DirectoryError);

    MaterialAsset ParentMaterial{};
    ParentMaterial.ShaderModule = "DefaultGBufferMaterial";
    ParentMaterial.ShadingModel = "GBufferShadingModel";
    auto ParentJson = SerializeAuthoredAssetToJson(ParentMaterial);
    REQUIRE(ParentJson);

    MaterialInstanceAsset InstanceAsset{};
    InstanceAsset.ParentMaterial.AssetName = "Rendering/Parent.material";
    InstanceAsset.ParentMaterial.AssetId = SourceAssetIdFromLogicalName("Rendering/Parent.material").ToString();
    auto InstanceJson = SerializeAuthoredAssetToJson(InstanceAsset);
    REQUIRE(InstanceJson);

    {
        std::ofstream Out(Root.Path / "Rendering" / "Parent.material", std::ios::binary | std::ios::trunc);
        REQUIRE(Out.is_open());
        Out.write(ParentJson->data(), static_cast<std::streamsize>(ParentJson->size()));
    }
    {
        std::ofstream Out(Root.Path / "Rendering" / "Child.materialinstance", std::ios::binary | std::ios::trunc);
        REQUIRE(Out.is_open());
        Out.write(InstanceJson->data(), static_cast<std::streamsize>(InstanceJson->size()));
    }

    REQUIRE(Host.AssetService.RefreshDiscovery());
    REQUIRE(Host.AssetService.OpenAssetEditorByKey("Rendering/Child.materialinstance"));

    const auto Session = Host.AssetService.AssetEditorSession();
    REQUIRE(Session.IsOpen);
    REQUIRE(Session.TargetType == StaticTypeId<MaterialInstanceAsset>());
    auto* MaterialInstance = static_cast<MaterialInstanceAsset*>(Session.TargetObject);
    REQUIRE(MaterialInstance != nullptr);
    CHECK(MaterialInstance->ParentMaterial.AssetName == "Rendering/Parent.material");
    REQUIRE(MaterialInstance->Vectors.size() == 1u);
    CHECK(MaterialInstance->Vectors.front().Name == "Color");
    REQUIRE(MaterialInstance->Scalars.size() == 3u);
    CHECK(std::find_if(MaterialInstance->Scalars.begin(),
                       MaterialInstance->Scalars.end(),
                       [](const MaterialScalarParamPayload& Param) {
                           return Param.Name == "Roughness";
                       }) != MaterialInstance->Scalars.end());
    CHECK(std::find_if(MaterialInstance->Scalars.begin(),
                       MaterialInstance->Scalars.end(),
                       [](const MaterialScalarParamPayload& Param) {
                           return Param.Name == "Metallic";
                       }) != MaterialInstance->Scalars.end());
    CHECK(std::find_if(MaterialInstance->Scalars.begin(),
                       MaterialInstance->Scalars.end(),
                       [](const MaterialScalarParamPayload& Param) {
                           return Param.Name == "Occlusion";
                       }) != MaterialInstance->Scalars.end());
    CHECK(MaterialInstance->Textures.empty());
    CHECK(Session.RuntimeDirty);
    CHECK(Session.IsDirty);
}

TEST_CASE("UI property panel persists PawnBase material instance overrides through save and reopen",
          "[Assets][Editor][Source][UI]")
{
    RegisterBuiltinTypes();
    REQUIRE(TypeAutoRegistry::Instance().Ensure(StaticTypeId<PawnBase>()));

    TestEditorHost Host{};
    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);
    EditorServiceContext Context(Host);

    std::error_code DirectoryError{};
    std::filesystem::create_directories(Root.Path / "Rendering", DirectoryError);
    REQUIRE_FALSE(DirectoryError);

    MaterialInstanceAsset InstanceAsset{};
    auto InstanceJson = SerializeAuthoredAssetToJson(InstanceAsset);
    REQUIRE(InstanceJson);
    {
        std::ofstream Out(Root.Path / "Rendering" / "PawnOverride.materialinstance", std::ios::binary | std::ios::trunc);
        REQUIRE(Out.is_open());
        Out.write(InstanceJson->data(), static_cast<std::streamsize>(InstanceJson->size()));
    }

    REQUIRE(Host.AssetService.RefreshDiscovery());
    REQUIRE(Host.AssetService.CreatePrefabSourceAssetByNodeType(
        Context,
        StaticTypeId<PawnBase>(),
        "TypedPawn",
        "Gameplay"));

    const auto* Created = Host.AssetService.SelectedAsset();
    REQUIRE(Created != nullptr);
    const std::string CreatedKey = Created->Key;
    REQUIRE(CreatedKey == "Gameplay/TypedPawn.prefab");

    REQUIRE(Host.AssetService.OpenAssetEditorByKey(CreatedKey));
    const auto Session = Host.AssetService.AssetEditorSession();
    REQUIRE(Session.IsOpen);
    REQUIRE(Session.TargetType == StaticTypeId<PawnBase>());

    auto* PawnNode = static_cast<PawnBase*>(Session.TargetObject);
    REQUIRE(PawnNode != nullptr);
    auto MeshResult = PawnNode->Component<StaticMeshComponent>();
    REQUIRE(MeshResult);
    auto* Mesh = &(*MeshResult);
    Mesh->EditSettings().MaterialInstanceOverrides.emplace_back();

    auto UiContext = std::make_unique<SnAPI::UI::UIContext>();
    UiContext->EnsureDefaultSetup();
    UiContext->SetViewportSize(900.0f, 1200.0f);
    UiContext->RegisterElementType<UIPropertyPanel>();

    auto RootBuilder = UiContext->Root();
    RootBuilder.Element().Padding().Set(0.0f);
    RootBuilder.Element().Gap().Set(0.0f);

    auto PanelBuilder = RootBuilder.Add(UIPropertyPanel{});
    auto& Panel = PanelBuilder.Element();
    Panel.Width().Set(SnAPI::UI::Sizing::Fill());
    Panel.Height().Set(SnAPI::UI::Sizing::Fill());

    REQUIRE(Panel.BindObject(StaticTypeId<StaticMeshComponent>(), Mesh));

    SnAPI::UI::RenderPacketList Packets{};
    UiContext->BuildRenderPackets(Packets);

    const auto LabelId = FindTextElementByText(*UiContext, PanelBuilder.Handle().Id, "Slot 0");
    REQUIRE(LabelId.has_value());
    const SnAPI::UI::ElementId RowId = UiContext->GetParent(*LabelId);
    REQUIRE(RowId.Value != 0);

    auto ComboBoxes = FindComboBoxesUnder(*UiContext, RowId);
    REQUIRE(ComboBoxes.size() == 1u);
    auto* Combo = ComboBoxes.front();
    REQUIRE(Combo != nullptr);

    const auto& Items = Combo->Items();
    const auto OptionIt = std::find_if(Items.begin(), Items.end(), [](const std::string& Item) {
        return Item.rfind("Rendering/PawnOverride.materialinstance [", 0u) == 0u;
    });
    REQUIRE(OptionIt != Items.end());
    REQUIRE(Combo->SelectByText(*OptionIt, true));

    REQUIRE(Mesh->GetSettings().MaterialInstanceOverrides.size() == 1u);
    CHECK(Mesh->GetSettings().MaterialInstanceOverrides.front().GetAssetName() == "Rendering/PawnOverride.materialinstance");
    CHECK(Mesh->GetSettings().MaterialInstanceOverrides.front().GetAssetId() ==
          SourceAssetIdFromLogicalName("Rendering/PawnOverride.materialinstance").ToString());

    REQUIRE(Host.AssetService.SaveActiveAssetEditor());

    NodeAsset SavedPrefab{};
    REQUIRE(DeserializeAuthoredAssetFromJson(
        ReadTextFile(Root.Path / "Gameplay" / "TypedPawn.prefab"),
        SavedPrefab));
    REQUIRE(SavedPrefab.Nodes.size() == 1u);
    const auto ComponentIt = std::find_if(
        SavedPrefab.Nodes.front().Components.begin(),
        SavedPrefab.Nodes.front().Components.end(),
        [](const NodeComponentAsset& Component) {
            return Component.Type == StaticTypeId<StaticMeshComponent>();
        });
    REQUIRE(ComponentIt != SavedPrefab.Nodes.front().Components.end());

    Host.AssetService.CloseAssetEditor();
    REQUIRE(Host.AssetService.OpenAssetEditorByKey(CreatedKey));

    const auto ReopenedSession = Host.AssetService.AssetEditorSession();
    REQUIRE(ReopenedSession.IsOpen);
    auto* ReopenedPawn = static_cast<PawnBase*>(ReopenedSession.TargetObject);
    REQUIRE(ReopenedPawn != nullptr);
    auto ReopenedMeshResult = ReopenedPawn->Component<StaticMeshComponent>();
    REQUIRE(ReopenedMeshResult);
    auto* ReopenedMesh = &(*ReopenedMeshResult);
    REQUIRE(ReopenedMesh->GetSettings().MaterialInstanceOverrides.size() == 1u);
    CHECK(ReopenedMesh->GetSettings().MaterialInstanceOverrides.front().GetAssetName() ==
          "Rendering/PawnOverride.materialinstance");
    CHECK(ReopenedMesh->GetSettings().MaterialInstanceOverrides.front().GetAssetId() ==
          SourceAssetIdFromLogicalName("Rendering/PawnOverride.materialinstance").ToString());
}

TEST_CASE("UI property panel persists authored static mesh material instance refs and closes cleanly",
          "[Assets][Editor][Source][UI]")
{
    RegisterBuiltinTypes();

    TestEditorHost Host{};
    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);

    std::error_code DirectoryError{};
    std::filesystem::create_directories(Root.Path / "Rendering", DirectoryError);
    REQUIRE_FALSE(DirectoryError);

    MaterialInstanceAsset InstanceAsset{};
    auto InstanceJson = SerializeAuthoredAssetToJson(InstanceAsset);
    REQUIRE(InstanceJson);
    {
        std::ofstream Out(Root.Path / "Rendering" / "MeshOverride.materialinstance", std::ios::binary | std::ios::trunc);
        REQUIRE(Out.is_open());
        Out.write(InstanceJson->data(), static_cast<std::streamsize>(InstanceJson->size()));
    }

    StaticMeshAsset MeshAsset{};
    MeshAsset.Mesh.Name = "UnitMesh";
    MeshAsset.Mesh.MaterialInstances.emplace_back();
    auto MeshJson = SerializeAuthoredAssetToJson(MeshAsset);
    REQUIRE(MeshJson);
    {
        std::ofstream Out(Root.Path / "Rendering" / "Unit.staticmesh", std::ios::binary | std::ios::trunc);
        REQUIRE(Out.is_open());
        Out.write(MeshJson->data(), static_cast<std::streamsize>(MeshJson->size()));
    }

    REQUIRE(Host.AssetService.RefreshDiscovery());
    REQUIRE(Host.AssetService.OpenAssetEditorByKey("Rendering/Unit.staticmesh"));

    const auto Session = Host.AssetService.AssetEditorSession();
    REQUIRE(Session.IsOpen);
    REQUIRE(Session.TargetType == StaticTypeId<StaticMeshAsset>());

    auto* Mesh = static_cast<StaticMeshAsset*>(Session.TargetObject);
    REQUIRE(Mesh != nullptr);
    REQUIRE(Mesh->Mesh.MaterialInstances.size() == 1u);
    CHECK(Mesh->Mesh.MaterialInstances.front().IsNull());

    auto UiContext = std::make_unique<SnAPI::UI::UIContext>();
    UiContext->EnsureDefaultSetup();
    UiContext->SetViewportSize(900.0f, 1200.0f);
    UiContext->RegisterElementType<UIPropertyPanel>();

    auto RootBuilder = UiContext->Root();
    RootBuilder.Element().Padding().Set(0.0f);
    RootBuilder.Element().Gap().Set(0.0f);

    auto PanelBuilder = RootBuilder.Add(UIPropertyPanel{});
    auto& Panel = PanelBuilder.Element();
    Panel.Width().Set(SnAPI::UI::Sizing::Fill());
    Panel.Height().Set(SnAPI::UI::Sizing::Fill());

    REQUIRE(Panel.BindObject(Session.TargetType, Session.TargetObject));

    SnAPI::UI::RenderPacketList Packets{};
    UiContext->BuildRenderPackets(Packets);

    const auto LabelId = FindTextElementByText(*UiContext, PanelBuilder.Handle().Id, "Slot 0");
    REQUIRE(LabelId.has_value());
    const SnAPI::UI::ElementId RowId = UiContext->GetParent(*LabelId);
    REQUIRE(RowId.Value != 0);

    auto ComboBoxes = FindComboBoxesUnder(*UiContext, RowId);
    REQUIRE(ComboBoxes.size() == 1u);
    auto* Combo = ComboBoxes.front();
    REQUIRE(Combo != nullptr);

    const auto& Items = Combo->Items();
    const auto OptionIt = std::find_if(Items.begin(), Items.end(), [](const std::string& Item) {
        return Item.rfind("Rendering/MeshOverride.materialinstance [", 0u) == 0u;
    });
    REQUIRE(OptionIt != Items.end());
    REQUIRE(Combo->SelectByText(*OptionIt, true));

    REQUIRE(Mesh->Mesh.MaterialInstances.size() == 1u);
    CHECK(Mesh->Mesh.MaterialInstances.front().GetAssetName() == "Rendering/MeshOverride.materialinstance");
    CHECK(Mesh->Mesh.MaterialInstances.front().GetAssetId() ==
          SourceAssetIdFromLogicalName("Rendering/MeshOverride.materialinstance").ToString());

    REQUIRE(Host.AssetService.SaveActiveAssetEditor());

    StaticMeshAsset SavedMesh{};
    REQUIRE(DeserializeAuthoredAssetFromJson(
        ReadTextFile(Root.Path / "Rendering" / "Unit.staticmesh"),
        SavedMesh));
    REQUIRE(SavedMesh.Mesh.MaterialInstances.size() == 1u);
    CHECK(SavedMesh.Mesh.MaterialInstances.front().GetAssetName() == "Rendering/MeshOverride.materialinstance");
    CHECK(SavedMesh.Mesh.MaterialInstances.front().GetAssetId() ==
          SourceAssetIdFromLogicalName("Rendering/MeshOverride.materialinstance").ToString());

    Host.AssetService.CloseAssetEditor();
    REQUIRE(Host.AssetService.OpenAssetEditorByKey("Rendering/Unit.staticmesh"));

    const auto ReopenedSession = Host.AssetService.AssetEditorSession();
    REQUIRE(ReopenedSession.IsOpen);
    auto* ReopenedMesh = static_cast<StaticMeshAsset*>(ReopenedSession.TargetObject);
    REQUIRE(ReopenedMesh != nullptr);
    REQUIRE(ReopenedMesh->Mesh.MaterialInstances.size() == 1u);
    CHECK(ReopenedMesh->Mesh.MaterialInstances.front().GetAssetName() == "Rendering/MeshOverride.materialinstance");
    CHECK(ReopenedMesh->Mesh.MaterialInstances.front().GetAssetId() ==
          SourceAssetIdFromLogicalName("Rendering/MeshOverride.materialinstance").ToString());
}

#endif

#if defined(SNAPI_GF_ENABLE_RENDERER)

TEST_CASE("World render settings prefab saves referenced fog params without deadlocking", "[Assets][Editor][Source][Renderer]")
{
    RegisterBuiltinTypes();
    REQUIRE(TypeAutoRegistry::Instance().Ensure(StaticTypeId<HeightFogParamsNode>()));
    REQUIRE(TypeAutoRegistry::Instance().Ensure(StaticTypeId<WorldRenderSettings>()));

    TestEditorHost Host{};
    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);
    EditorServiceContext Context(Host);

    REQUIRE(Host.AssetService.RefreshDiscovery());
    REQUIRE(Host.AssetService.CreatePrefabSourceAssetByNodeType(
        Context,
        StaticTypeId<HeightFogParamsNode>(),
        "UnitFogParams",
        "Rendering"));

    const auto* CreatedFog = Host.AssetService.SelectedAsset();
    REQUIRE(CreatedFog != nullptr);
    const std::string FogAssetKey = CreatedFog->Key;
    const std::string FogAssetId = CreatedFog->AssetId.ToString();
    REQUIRE(FogAssetKey == "Rendering/UnitFogParams.prefab");

    REQUIRE(Host.AssetService.CreatePrefabSourceAssetByNodeType(
        Context,
        StaticTypeId<WorldRenderSettings>(),
        "UnitWorldRenderSettings",
        "Rendering"));

    const auto* CreatedRenderSettings = Host.AssetService.SelectedAsset();
    REQUIRE(CreatedRenderSettings != nullptr);
    const std::string RenderSettingsKey = CreatedRenderSettings->Key;
    REQUIRE(RenderSettingsKey == "Rendering/UnitWorldRenderSettings.prefab");

    REQUIRE(Host.AssetService.OpenAssetEditorByKey(RenderSettingsKey));
    auto Session = Host.AssetService.AssetEditorSession();
    REQUIRE(Session.IsOpen);

    auto* SettingsNode = static_cast<WorldRenderSettings*>(Session.TargetObject);
    REQUIRE(SettingsNode != nullptr);

    SettingsNode->EditHeightFogParams().EditAssetName() = FogAssetKey;
    SettingsNode->EditHeightFogParams().EditAssetId() = FogAssetId;
    SettingsNode->EditorOnPropertyChanged("HeightFogParams");
    SettingsNode->EditorOnPropertyChanged("HeightFogParams");

    std::size_t FogChildCount = 0;
    for (const NodeHandle& ChildRef : SettingsNode->Children())
    {
        NodeHandle ChildHandle = ChildRef;
        auto* ChildNode = SettingsNode->World()->BorrowedNode(ChildHandle);
        if (ChildNode != nullptr &&
            TypeRegistry::Instance().IsA(ChildNode->TypeKey(), StaticTypeId<HeightFogParamsNode>()))
        {
            ++FogChildCount;
            CHECK(ChildNode->EditorTransient());
        }
    }
    CHECK(FogChildCount == 1);

    Host.AssetService.NotifyActiveAssetEditorRuntimeMutated(Session.TargetType, Session.TargetObject);
    REQUIRE(Host.AssetService.SaveActiveAssetEditor());

    Session = Host.AssetService.AssetEditorSession();
    REQUIRE(Session.IsOpen);
    auto* SavedSessionSettingsNode = static_cast<WorldRenderSettings*>(Session.TargetObject);
    REQUIRE(SavedSessionSettingsNode != nullptr);
    CHECK(SavedSessionSettingsNode->GetHeightFogParams().GetAssetName() == FogAssetKey);
    CHECK(SavedSessionSettingsNode->GetHeightFogParams().GetAssetId() == FogAssetId);

    const std::string SavedJson = ReadTextFile(Root.Path / "Rendering" / "UnitWorldRenderSettings.prefab");
    CHECK(SavedJson.find("\"HeightFogParams\"") != std::string::npos);
    CHECK(SavedJson.find(FogAssetKey) != std::string::npos);
    CHECK(SavedJson.find(FogAssetId) != std::string::npos);

    NodeAsset SavedPrefab{};
    REQUIRE(DeserializeAuthoredAssetFromJson(SavedJson, SavedPrefab));
    REQUIRE(SavedPrefab.Nodes.size() == 1);
    CHECK(SavedPrefab.Nodes.front().Children.empty());

    Host.AssetService.CloseAssetEditor();
    REQUIRE(Host.AssetService.OpenAssetEditorByKey(RenderSettingsKey));

    Session = Host.AssetService.AssetEditorSession();
    REQUIRE(Session.IsOpen);

    auto* ReopenedSettingsNode = static_cast<WorldRenderSettings*>(Session.TargetObject);
    REQUIRE(ReopenedSettingsNode != nullptr);
    CHECK(ReopenedSettingsNode->GetHeightFogParams().GetAssetName() == FogAssetKey);
    CHECK(ReopenedSettingsNode->GetHeightFogParams().GetAssetId() == FogAssetId);
}

TEST_CASE("Project default render settings do not duplicate authored world render settings roots during PIE",
          "[Assets][Editor][Source][Renderer][PIE]")
{
    RegisterBuiltinTypes();
    REQUIRE(TypeAutoRegistry::Instance().Ensure(StaticTypeId<HeightFogParamsNode>()));
    REQUIRE(TypeAutoRegistry::Instance().Ensure(StaticTypeId<WorldRenderSettings>()));

    TestEditorHost Host{};
    EditorPieService PieService{};
    EditorServiceContext Context(Host);
    REQUIRE(PieService.Initialize(Context));

    TempDir Root{};
    const std::filesystem::path ProjectRoot = Root.Path / "Project";
    const std::filesystem::path AssetRootPath = ProjectRoot / "Assets";
    std::filesystem::create_directories(AssetRootPath);
    ScopedAssetRoot AssetRoot(AssetRootPath);

    REQUIRE(Host.AssetService.RefreshDiscovery());
    REQUIRE(Host.AssetService.CreatePrefabSourceAssetByNodeType(
        Context,
        StaticTypeId<HeightFogParamsNode>(),
        "ProjectFogParams",
        "Rendering"));

    const auto* CreatedFog = Host.AssetService.SelectedAsset();
    REQUIRE(CreatedFog != nullptr);
    const std::string FogAssetKey = CreatedFog->Key;
    const std::string FogAssetId = CreatedFog->AssetId.ToString();

    REQUIRE(Host.AssetService.CreatePrefabSourceAssetByNodeType(
        Context,
        StaticTypeId<WorldRenderSettings>(),
        "ProjectDefaultRenderSettings",
        "Rendering"));

    const auto* CreatedRenderSettings = Host.AssetService.SelectedAsset();
    REQUIRE(CreatedRenderSettings != nullptr);
    const std::string RenderSettingsKey = CreatedRenderSettings->Key;
    const std::string RenderSettingsAssetId = CreatedRenderSettings->AssetId.ToString();

    REQUIRE(Host.AssetService.OpenAssetEditorByKey(RenderSettingsKey));
    auto Session = Host.AssetService.AssetEditorSession();
    REQUIRE(Session.IsOpen);

    auto* SettingsNode = static_cast<WorldRenderSettings*>(Session.TargetObject);
    REQUIRE(SettingsNode != nullptr);
    SettingsNode->EditHeightFogParams().EditAssetName() = FogAssetKey;
    SettingsNode->EditHeightFogParams().EditAssetId() = FogAssetId;
    SettingsNode->EditorOnPropertyChanged("HeightFogParams");
    Host.AssetService.NotifyActiveAssetEditorRuntimeMutated(Session.TargetType, Session.TargetObject);
    REQUIRE(Host.AssetService.SaveActiveAssetEditor());
    Host.AssetService.CloseAssetEditor();

    LevelAsset StartupLevel{};
    StartupLevel.Name = "Startup";
    StartupLevel.Nodes.push_back(NodeObjectAsset{
        .Id = NewUuid(),
        .Type = StaticTypeId<WorldRenderSettings>(),
        .Name = "AuthoredWorldRenderSettings",
        .Active = true,
    });

    auto LevelJson = SerializeAuthoredAssetToJson(StartupLevel);
    REQUIRE(LevelJson);
    WriteTextFile(AssetRootPath / "Levels" / "Startup.level", *LevelJson);

    const std::filesystem::path ProjectFilePath = ProjectRoot / "project.snproj.json";
    const std::string ProjectConfig =
        std::string("{\n") +
        "  \"version\": 1,\n"
        "  \"name\": \"WorldRenderSettingsPieProject\",\n"
        "  \"assetRoot\": \"Assets\",\n"
        "  \"startupLevelAsset\": \"Levels/Startup.level\",\n"
        "  \"defaultRenderSettings\": \"" + RenderSettingsAssetId + "\"\n"
        "}\n";
    WriteTextFile(ProjectFilePath, ProjectConfig);

    REQUIRE(Host.AssetService.LoadProject(Context, ProjectFilePath.string()));
    CHECK(CountNodesOfType(Host.Runtime.World(), StaticTypeId<WorldRenderSettings>()) == 1);
    CHECK(CountNodesOfType(Host.Runtime.World(), StaticTypeId<HeightFogParamsNode>()) == 0);

    Host.AssetService.Tick(Context, 0.0f);
    Host.Runtime.Update(0.0f);
    CHECK(CountNodesOfType(Host.Runtime.World(), StaticTypeId<WorldRenderSettings>()) == 1);
    CHECK(CountNodesOfType(Host.Runtime.World(), StaticTypeId<HeightFogParamsNode>()) == 0);

    REQUIRE(PieService.Play(Context));
    Host.AssetService.Tick(Context, 0.0f);
    Host.Runtime.Update(0.0f);
    CHECK(CountNodesOfType(Host.Runtime.World(), StaticTypeId<WorldRenderSettings>()) == 1);
    CHECK(CountNodesOfType(Host.Runtime.World(), StaticTypeId<HeightFogParamsNode>()) == 0);

    REQUIRE(PieService.Stop(Context));
    Host.AssetService.Tick(Context, 0.0f);
    Host.Runtime.Update(0.0f);
    CHECK(CountNodesOfType(Host.Runtime.World(), StaticTypeId<WorldRenderSettings>()) == 1);
    CHECK(CountNodesOfType(Host.Runtime.World(), StaticTypeId<HeightFogParamsNode>()) == 0);

    PieService.Shutdown(Context);
}

TEST_CASE("PIE stop clears transient fog nodes created during play", "[Assets][Editor][Source][Renderer][PIE]")
{
    RegisterBuiltinTypes();
    REQUIRE(TypeAutoRegistry::Instance().Ensure(StaticTypeId<HeightFogParamsNode>()));

    TestEditorHost Host{};
    EditorPieService PieService{};
    EditorServiceContext Context(Host);
    REQUIRE(PieService.Initialize(Context));

    auto& WorldRef = Host.Runtime.World();
    REQUIRE(PieService.Play(Context));

    auto FogNodeResult = WorldRef.CreateNode<HeightFogParamsNode>("PieFog");
    REQUIRE(FogNodeResult.has_value());
    auto* FogNode = static_cast<HeightFogParamsNode*>(FogNodeResult->Borrowed());
    REQUIRE(FogNode != nullptr);

    FogNode->EditDensity() = 0.37f;
    FogNode->EditStartDistance() = 42.0f;
    FogNode->EditorOnPropertyChanged("Density");
    WorldRef.Tick(0.0f);

    CHECK(CountNodesOfType(WorldRef, StaticTypeId<HeightFogParamsNode>()) == 1);

    REQUIRE(PieService.Stop(Context));
    Host.AssetService.Tick(Context, 0.0f);
    Host.Runtime.Update(0.0f);

    CHECK(CountNodesOfType(WorldRef, StaticTypeId<HeightFogParamsNode>()) == 0);

    PieService.Shutdown(Context);
}

#endif

TEST_CASE("Editor asset service creates projects with the structured descriptor schema", "[Assets][Editor][Project]")
{
    RegisterBuiltinTypes();

    TestEditorHost Host{};
    TempDir Root{};
    EditorServiceContext Context(Host);

    REQUIRE(Host.AssetService.CreateProject(Context, "StructuredEditorProject", Root.Path.string()));

    const std::filesystem::path ProjectRoot = Root.Path / "StructuredEditorProject";
    const std::filesystem::path ProjectFilePath = ProjectRoot / "project.snproj.json";
    const std::filesystem::path StartupLevelPath = ProjectRoot / "Assets" / "Levels" / "StarterLevel.level";

    REQUIRE(std::filesystem::exists(ProjectFilePath));
    REQUIRE(std::filesystem::exists(StartupLevelPath));

    const auto ResolvedProject = ProjectDescriptorService::LoadResolved(ProjectFilePath.string());
    REQUIRE(ResolvedProject);
    CHECK(ResolvedProject->Descriptor.Project.Name == "StructuredEditorProject");
    CHECK(ResolvedProject->Descriptor.Project.DisplayName == "StructuredEditorProject");
    CHECK(ResolvedProject->Descriptor.Paths.AssetRoot == "Assets");
    CHECK(ResolvedProject->Descriptor.Startup.StartupLevelAsset == "Levels/StarterLevel.level");

    const auto& CurrentProject = Host.AssetService.CurrentProject();
    CHECK(CurrentProject.IsLoaded);
    CHECK(CurrentProject.Name == "StructuredEditorProject");
    CHECK(std::filesystem::path(CurrentProject.ProjectFilePath).lexically_normal() == ProjectFilePath.lexically_normal());
    CHECK(std::filesystem::path(CurrentProject.AssetRootDirectory).lexically_normal()
          == (ProjectRoot / "Assets").lexically_normal());

    const std::string ProjectFileText = ReadTextFile(ProjectFilePath);
    const nlohmann::ordered_json RootJson = nlohmann::ordered_json::parse(ProjectFileText, nullptr, false);
    REQUIRE_FALSE(RootJson.is_discarded());
    CHECK(RootJson.contains("Format"));
    CHECK(RootJson.contains("Project"));
    CHECK(RootJson.contains("Paths"));
    CHECK(RootJson.contains("Startup"));
    CHECK_FALSE(RootJson.contains("version"));
    CHECK_FALSE(RootJson.contains("name"));
}

TEST_CASE("Editor asset service creates plugins with structured starter scaffolding", "[Assets][Editor][Plugin]")
{
    RegisterBuiltinTypes();

    TestEditorHost Host{};
    TempDir Root{};
    EditorServiceContext Context(Host);

    PluginCreationRequest Request{};
    Request.PluginName = "StructuredEditorPlugin";
    Request.ParentDirectory = Root.Path;
    auto Descriptor = PluginCreationService::BuildDefaultDescriptor(Request.PluginName);
    REQUIRE(Descriptor);
    Request.Descriptor = *Descriptor;
    Request.Code.CreateStarterRuntimeModule = true;
    Request.Code.RuntimeModuleName = "StructuredEditorPlugin";
    Request.Code.CreateStarterEditorModule = true;
    Request.Code.EditorModuleName = "StructuredEditorPluginEditor";

    PluginCreationResult CreateResult{};
    REQUIRE(Host.AssetService.CreatePlugin(Context, Request, &CreateResult));

    const std::filesystem::path PluginRoot = Root.Path / "StructuredEditorPlugin";
    const std::filesystem::path PluginFilePath = PluginRoot / PluginDescriptorService::kDefaultPluginFileName;

    REQUIRE(std::filesystem::exists(PluginFilePath));
    REQUIRE(std::filesystem::exists(PluginRoot / "Modules" / "CMakeLists.txt"));
    REQUIRE(std::filesystem::exists(PluginRoot / "Intermediate" / "Build" / "Generated" / "PluginModules.cmake"));
    REQUIRE(std::filesystem::exists(CreateResult.RuntimeModuleDirectory));
    REQUIRE(std::filesystem::exists(CreateResult.EditorModuleDirectory));

    const auto ResolvedPlugin = PluginDescriptorService::LoadResolved(PluginFilePath.string());
    REQUIRE(ResolvedPlugin);
    CHECK(ResolvedPlugin->Descriptor.Plugin.Name == "StructuredEditorPlugin");
    CHECK(ResolvedPlugin->Descriptor.Modules.size() == 2u);
    CHECK(ResolvedPlugin->Descriptor.Modules[0].Type == EProjectModuleType::Runtime);
    CHECK(ResolvedPlugin->Descriptor.Modules[1].Type == EProjectModuleType::Editor);
}

TEST_CASE("Editor asset service adds project and plugin modules with type-specific starter hooks", "[Assets][Editor][Module]")
{
    RegisterBuiltinTypes();

    TestEditorHost Host{};
    TempDir Root{};
    EditorServiceContext Context(Host);

    REQUIRE(Host.AssetService.CreateProject(Context, "EditorModuleProject", Root.Path.string()));
    const std::filesystem::path ProjectFilePath = Root.Path / "EditorModuleProject" / "project.snproj.json";

    ModuleCreationRequest ProjectModuleRequest{};
    ProjectModuleRequest.ModuleName = "GameplayShared";
    ProjectModuleRequest.ModuleType = EProjectModuleType::Shared;
    ProjectModuleRequest.PublicDependencies = {"SnAPI.GameFramework"};

    ModuleCreationResult ProjectModuleResult{};
    REQUIRE(Host.AssetService.CreateProjectModule(Context, ProjectModuleRequest, &ProjectModuleResult));
    REQUIRE(std::filesystem::exists(ProjectModuleResult.ModuleHeaderPath));
    REQUIRE(std::filesystem::exists(ProjectModuleResult.ModuleSourcePath));
    CHECK(ReadTextFile(ProjectModuleResult.ModuleHeaderPath).find("RegisterSharedServices") != std::string::npos);

    ModuleCreationRequest GameplayModuleRequest{};
    GameplayModuleRequest.ModuleName = "GameplayRuntime";
    GameplayModuleRequest.ModuleType = EProjectModuleType::Runtime;
    GameplayModuleRequest.GenerateGameplayBootstrap = true;

    ModuleCreationResult GameplayModuleResult{};
    REQUIRE(Host.AssetService.CreateProjectModule(Context, GameplayModuleRequest, &GameplayModuleResult));
    REQUIRE(std::filesystem::exists(GameplayModuleResult.GameHeaderPath));
    REQUIRE(std::filesystem::exists(GameplayModuleResult.GameModeHeaderPath));
    CHECK(ReadTextFile(GameplayModuleResult.GameHeaderPath).find("class GameplayRuntimeGame final") != std::string::npos);
    CHECK(ReadTextFile(GameplayModuleResult.GameModeHeaderPath).find("class GameplayRuntimeGameMode final") !=
          std::string::npos);

    PluginCreationRequest PluginRequest{};
    PluginRequest.PluginName = "EditorModulePlugin";
    PluginRequest.ParentDirectory = Root.Path;
    auto PluginDescriptor = PluginCreationService::BuildDefaultDescriptor(PluginRequest.PluginName);
    REQUIRE(PluginDescriptor);
    PluginRequest.Descriptor = *PluginDescriptor;
    REQUIRE(Host.AssetService.CreatePlugin(Context, PluginRequest));

    const std::filesystem::path PluginFilePath = Root.Path / "EditorModulePlugin" / PluginDescriptorService::kDefaultPluginFileName;

    PluginModuleCreationRequest PluginModuleRequest{};
    PluginModuleRequest.PluginFilePath = PluginFilePath;
    PluginModuleRequest.ModuleName = "EditorModuleDiagnostics";
    PluginModuleRequest.ModuleType = EProjectModuleType::Developer;
    PluginModuleRequest.PrivateDependencies = {"SnAPI.GameFramework"};

    PluginModuleCreationResult PluginModuleResult{};
    REQUIRE(Host.AssetService.CreatePluginModule(Context, PluginModuleRequest, &PluginModuleResult));
    REQUIRE(std::filesystem::exists(PluginModuleResult.ModuleHeaderPath));
    REQUIRE(std::filesystem::exists(PluginModuleResult.ModuleSourcePath));
    CHECK(ReadTextFile(PluginModuleResult.ModuleHeaderPath).find("RegisterDeveloperTools") != std::string::npos);

    const auto ResolvedProject = ProjectDescriptorService::LoadResolved(ProjectFilePath.string());
    REQUIRE(ResolvedProject);
    CHECK(std::any_of(ResolvedProject->Descriptor.Modules.begin(),
                      ResolvedProject->Descriptor.Modules.end(),
                      [](const ProjectModuleDescriptor& Module) { return Module.Name == "GameplayShared"; }));

    const auto ResolvedPlugin = PluginDescriptorService::LoadResolved(PluginFilePath.string());
    REQUIRE(ResolvedPlugin);
    CHECK(std::any_of(ResolvedPlugin->Descriptor.Modules.begin(),
                      ResolvedPlugin->Descriptor.Modules.end(),
                      [](const ProjectModuleDescriptor& Module) { return Module.Name == "EditorModuleDiagnostics"; }));
}

TEST_CASE("Editor asset service routes Conduit source assets through the Conduit document service", "[Assets][Editor][Source][Conduit]")
{
    RegisterBuiltinTypes();

    TestEditorHost Host{};
    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);
    EditorServiceContext Context(Host);

    REQUIRE(Host.AssetService.RefreshDiscovery());
    REQUIRE(Host.AssetService.CreateSourceAssetByType(
        Context,
        StaticTypeId<Conduit::GraphAsset>(),
        "GameplayLogic",
        "Conduit"));

    const auto* Created = Host.AssetService.SelectedAsset();
    REQUIRE(Created != nullptr);
    const std::string CreatedKey = Created->Key;
    REQUIRE(CreatedKey == "Conduit/GameplayLogic.conduitgraph");

    REQUIRE(Host.AssetService.OpenAssetEditorByKey(Context, CreatedKey));
    CHECK_FALSE(Host.AssetService.AssetEditorSession().IsOpen);

    auto* Document = Host.ConduitService.FindDocument(CreatedKey);
    REQUIRE(Document != nullptr);
    CHECK(Document->Asset().Name.empty());

    auto AddedVariable = Host.ConduitService.CreateVariable("Health", StaticTypeId<int>());
    REQUIRE(AddedVariable);
    REQUIRE(Host.ConduitService.SelectVariable((*AddedVariable)->Id));
    REQUIRE(Host.ConduitService.SetSelectedVariableDefaultText("42"));

    auto EntryNode = Host.ConduitService.SpawnNode("entry.custom");
    REQUIRE(EntryNode);
    REQUIRE(Host.ConduitService.SetSelectedNodePrimaryText("OnInteract"));

    auto LabelNode = Host.ConduitService.SpawnNode("builtin.label");
    REQUIRE(LabelNode);
    REQUIRE(Host.ConduitService.SetSelectedNodePrimaryText("LoopStart"));

    auto BranchNode = Host.ConduitService.SpawnNode("builtin.branch");
    REQUIRE(BranchNode);
    REQUIRE(Host.ConduitService.SetSelectedNodePrimaryText("LoopStart"));
    REQUIRE(Host.ConduitService.SetSelectedNodeSecondaryText("LoopExit"));

    REQUIRE(Host.AssetService.SaveAssetByKey(Context, CreatedKey));

    Conduit::GraphAsset SavedGraph{};
    REQUIRE(DeserializeAuthoredAssetFromJson(
        ReadTextFile(Root.Path / "Conduit" / "GameplayLogic.conduitgraph"),
        SavedGraph));
    REQUIRE(SavedGraph.Variables.size() == 1);
    CHECK(SavedGraph.Variables.front().Name == "Health");
    CHECK(SavedGraph.Variables.front().Type == StaticTypeId<int>());
    REQUIRE(SavedGraph.Nodes.size() == 3);
    CHECK(SavedGraph.Nodes[0].Kind == Conduit::EGraphAssetNodeKind::EntryPoint);
    CHECK(SavedGraph.Nodes[0].EntryPointName == "OnInteract");
    CHECK(SavedGraph.Nodes[1].Kind == Conduit::EGraphAssetNodeKind::Label);
    CHECK(SavedGraph.Nodes[1].LabelName == "LoopStart");
    CHECK(SavedGraph.Nodes[2].Kind == Conduit::EGraphAssetNodeKind::Branch);
    CHECK(SavedGraph.Nodes[2].LabelName == "LoopStart");
    CHECK(SavedGraph.Nodes[2].FalseLabelName == "LoopExit");
}

TEST_CASE("Editor asset service routes Conduit class source assets through the Conduit document service",
          "[Assets][Editor][Source][Conduit]")
{
    EnsureSourceAssetEditorNodeHostRegistered();

    TestEditorHost Host{};
    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);
    EditorServiceContext Context(Host);

    REQUIRE(Host.AssetService.RefreshDiscovery());
    REQUIRE(Host.AssetService.CreateSourceAssetByType(
        Context,
        StaticTypeId<Conduit::GraphAsset>(),
        "EnemyGraph",
        "Conduit"));
    REQUIRE(Host.AssetService.CreateSourceAssetByType(
        Context,
        StaticTypeId<Conduit::ClassAsset>(),
        "EnemyClass",
        "Conduit"));

    const auto* Created = Host.AssetService.SelectedAsset();
    REQUIRE(Created != nullptr);
    const std::string CreatedKey = Created->Key;
    REQUIRE(CreatedKey == "Conduit/EnemyClass.conduitclass");

    REQUIRE(Host.AssetService.OpenAssetEditorByKey(Context, CreatedKey));
    CHECK_FALSE(Host.AssetService.AssetEditorSession().IsOpen);

    auto* Document = Host.ConduitService.FindClassDocument(CreatedKey);
    REQUIRE(Document != nullptr);
    REQUIRE(Host.ConduitService.SetActiveClassHostType(StaticTypeId<SourceAssetEditorNodeHost>()));
    REQUIRE(Host.ConduitService.RenameActiveClass("EnemyController"));
    REQUIRE(Host.ConduitService.SetActiveClassGraph("Conduit/EnemyGraph.conduitgraph"));

    REQUIRE(Host.AssetService.SaveAssetByKey(Context, CreatedKey));

    Conduit::ClassAsset SavedClass{};
    REQUIRE(DeserializeAuthoredAssetFromJson(
        ReadTextFile(Root.Path / "Conduit" / "EnemyClass.conduitclass"),
        SavedClass));
    CHECK(SavedClass.Name == "EnemyController");
    CHECK(SavedClass.HostType == StaticTypeId<SourceAssetEditorNodeHost>());
    CHECK(SavedClass.Graph.GetAssetName() == "Conduit/EnemyGraph.conduitgraph");
}

TEST_CASE("UI property panel uses the asset picker for Conduit class refs",
          "[Assets][Editor][Source][Conduit]")
{
    RegisterBuiltinTypes();

    TestEditorHost Host{};
    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);
    EditorServiceContext Context(Host);

    REQUIRE(Host.AssetService.RefreshDiscovery());
    REQUIRE(Host.AssetService.CreateSourceAssetByType(
        Context,
        StaticTypeId<Conduit::GraphAsset>(),
        "EnemyGraph",
        "Conduit"));
    REQUIRE(Host.AssetService.CreateSourceAssetByType(
        Context,
        StaticTypeId<Conduit::ClassAsset>(),
        "EnemyClass",
        "Conduit"));

    World WorldInstance("ConduitClassRefPropertyPanel");
    auto NodeHandleResult = WorldInstance.CreateNode<BaseNode>("ConduitHost");
    REQUIRE(NodeHandleResult);

    auto* Node = NodeHandleResult->Borrowed();
    REQUIRE(Node != nullptr);

    auto ComponentResult = Node->Add<Conduit::ClassComponent>();
    REQUIRE(ComponentResult);

    auto UiContext = std::make_unique<SnAPI::UI::UIContext>();
    UiContext->EnsureDefaultSetup();
    UiContext->SetViewportSize(900.0f, 1200.0f);
    UiContext->RegisterElementType<UIPropertyPanel>();

    auto RootBuilder = UiContext->Root();
    RootBuilder.Element().Padding().Set(0.0f);
    RootBuilder.Element().Gap().Set(0.0f);

    auto PanelBuilder = RootBuilder.Add(UIPropertyPanel{});
    auto& Panel = PanelBuilder.Element();
    Panel.Width().Set(SnAPI::UI::Sizing::Fill());
    Panel.Height().Set(SnAPI::UI::Sizing::Fill());

    REQUIRE(Panel.BindNode(Node));

    SnAPI::UI::RenderPacketList Packets{};
    UiContext->BuildRenderPackets(Packets);

    const auto LabelId = FindTextElementByText(*UiContext, PanelBuilder.Handle().Id, "Class");
    REQUIRE(LabelId.has_value());

    const SnAPI::UI::ElementId RowId = UiContext->GetParent(*LabelId);
    REQUIRE(RowId.Value != 0);

    auto ComboBoxes = FindComboBoxesUnder(*UiContext, RowId);
    REQUIRE(ComboBoxes.size() == 1);

    const auto& Items = ComboBoxes.front()->Items();
    REQUIRE_FALSE(Items.empty());
    CHECK(Items.front() == "<None>");
}

TEST_CASE("UI property panel renders TFlags fields as accordion checkbox groups",
          "[Assets][Editor][Source]")
{
    RegisterBuiltinTypes();
    EnsureSourceAssetEditorFlagsComponentRegistered();

    World WorldInstance("FlagsPropertyPanelWorld");
    auto NodeHandleResult = WorldInstance.CreateNode<BaseNode>("FlagsHost");
    REQUIRE(NodeHandleResult);

    auto* Node = NodeHandleResult->Borrowed();
    REQUIRE(Node != nullptr);

    auto ComponentResult = Node->Add<SourceAssetEditorFlagsComponent>();
    REQUIRE(ComponentResult);

    auto UiContext = std::make_unique<SnAPI::UI::UIContext>();
    UiContext->EnsureDefaultSetup();
    UiContext->SetViewportSize(900.0f, 1200.0f);
    UiContext->RegisterElementType<UIPropertyPanel>();

    auto RootBuilder = UiContext->Root();
    RootBuilder.Element().Padding().Set(0.0f);
    RootBuilder.Element().Gap().Set(0.0f);

    auto PanelBuilder = RootBuilder.Add(UIPropertyPanel{});
    auto& Panel = PanelBuilder.Element();
    Panel.Width().Set(SnAPI::UI::Sizing::Fill());
    Panel.Height().Set(SnAPI::UI::Sizing::Fill());

    REQUIRE(Panel.BindNode(Node));

    SnAPI::UI::RenderPacketList Packets{};
    UiContext->BuildRenderPackets(Packets);

    const auto LabelId = FindTextElementByText(*UiContext, PanelBuilder.Handle().Id, "Flags");
    REQUIRE(LabelId.has_value());

    const SnAPI::UI::ElementId RowId = UiContext->GetParent(*LabelId);
    REQUIRE(RowId.Value != 0);

    auto Accordions = FindAccordionsUnder(*UiContext, RowId);
    REQUIRE(Accordions.size() == 1);

    auto Checkboxes = FindCheckboxesUnder(*UiContext, RowId);
    REQUIRE(Checkboxes.size() >= 3);

    bool FoundReliable = false;
    bool FoundServer = false;
    SnAPI::UI::UICheckbox* ClientCheckbox = nullptr;
    for (auto* Checkbox : Checkboxes)
    {
        const std::string Label =
            Checkbox->Properties().GetPropertyOr(SnAPI::UI::UICheckbox::LabelKey, std::string{});
        if (Label == "RpcReliable")
        {
            FoundReliable = Checkbox->Properties().GetPropertyOr(SnAPI::UI::UICheckbox::CheckedKey, false);
        }
        else if (Label == "RpcNetServer")
        {
            FoundServer = Checkbox->Properties().GetPropertyOr(SnAPI::UI::UICheckbox::CheckedKey, false);
        }
        else if (Label == "RpcNetClient")
        {
            ClientCheckbox = Checkbox;
        }
    }

    CHECK(FoundReliable);
    CHECK(FoundServer);
    REQUIRE(ClientCheckbox != nullptr);

    ClientCheckbox->Checked().Set(true);
    CHECK(ComponentResult->Flags.Has(EMethodFlagBits::RpcNetClient));
}

TEST_CASE("UI property panel renders TypeId fields as reflected-type combo boxes",
          "[Assets][Editor][Source]")
{
    RegisterBuiltinTypes();
    EnsureSourceAssetEditorNodeHostRegistered();
    EnsureSourceAssetEditorTypeIdComponentRegistered();
    REQUIRE(TypeAutoRegistry::Instance().Ensure(StaticTypeId<TSubClassOf<PawnBase>>()));
    REQUIRE(TypeRegistry::Instance().Find(StaticTypeId<TSubClassOf<PawnBase>>()) != nullptr);

    World WorldInstance("TypeIdPropertyPanelWorld");
    auto NodeHandleResult = WorldInstance.CreateNode<BaseNode>("TypeIdHost");
    REQUIRE(NodeHandleResult);

    auto* Node = NodeHandleResult->Borrowed();
    REQUIRE(Node != nullptr);

    auto ComponentResult = Node->Add<SourceAssetEditorTypeIdComponent>();
    REQUIRE(ComponentResult);

    auto UiContext = std::make_unique<SnAPI::UI::UIContext>();
    UiContext->EnsureDefaultSetup();
    UiContext->SetViewportSize(900.0f, 1200.0f);
    UiContext->RegisterElementType<UIPropertyPanel>();

    auto RootBuilder = UiContext->Root();
    RootBuilder.Element().Padding().Set(0.0f);
    RootBuilder.Element().Gap().Set(0.0f);

    auto PanelBuilder = RootBuilder.Add(UIPropertyPanel{});
    auto& Panel = PanelBuilder.Element();
    Panel.Width().Set(SnAPI::UI::Sizing::Fill());
    Panel.Height().Set(SnAPI::UI::Sizing::Fill());

    REQUIRE(Panel.BindNode(Node));

    SnAPI::UI::RenderPacketList Packets{};
    UiContext->BuildRenderPackets(Packets);

    const auto LabelId = FindTextElementByText(*UiContext, PanelBuilder.Handle().Id, "Selected Type");
    REQUIRE(LabelId.has_value());

    const SnAPI::UI::ElementId RowId = UiContext->GetParent(*LabelId);
    REQUIRE(RowId.Value != 0);

    auto ComboBoxes = FindComboBoxesUnder(*UiContext, RowId);
    REQUIRE(ComboBoxes.size() == 1);

    SnAPI::UI::UIComboBox* Combo = ComboBoxes.front();
    REQUIRE(Combo != nullptr);

    const auto& Items = Combo->Items();
    REQUIRE_FALSE(Items.empty());
    CHECK(Items.front() == "<None>");
    CHECK(std::find(Items.begin(), Items.end(), "BaseNode") != Items.end());
    CHECK(std::find(Items.begin(), Items.end(), "SourceAssetEditorNodeHost") != Items.end());
    CHECK(std::find(Items.begin(), Items.end(), "TSubClassOf<PawnBase>") != Items.end());
    CHECK(Combo->SelectedText() == "BaseNode");

    REQUIRE(Combo->SelectByText("SourceAssetEditorNodeHost", true));
    CHECK(ComponentResult->SelectedType == StaticTypeId<SourceAssetEditorNodeHost>());

    ComponentResult->SelectedType = StaticTypeId<TSubClassOf<PawnBase>>();
    Panel.RefreshFromModel();
    CHECK(Combo->SelectedText() == "TSubClassOf<PawnBase>");
}

TEST_CASE("Typed prefabs with Conduit class components reopen in the asset editor",
          "[Assets][Editor][Source][Conduit]")
{
    RegisterBuiltinTypes();

    TestEditorHost Host{};
    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);
    EditorServiceContext Context(Host);

    REQUIRE(Host.AssetService.RefreshDiscovery());
    REQUIRE(Host.AssetService.CreatePrefabSourceAssetByNodeType(
        Context,
        StaticTypeId<BaseNode>(),
        "ConduitHost",
        "Gameplay"));

    const auto* Created = Host.AssetService.SelectedAsset();
    REQUIRE(Created != nullptr);
    const std::string CreatedKey = Created->Key;
    REQUIRE(CreatedKey == "Gameplay/ConduitHost.prefab");

    REQUIRE(Host.AssetService.OpenAssetEditorByKey(CreatedKey));
    auto Session = Host.AssetService.AssetEditorSession();
    REQUIRE(Session.IsOpen);

    auto* RootNode = static_cast<BaseNode*>(Session.TargetObject);
    REQUIRE(RootNode != nullptr);

    auto ComponentResult = RootNode->Add<Conduit::ClassComponent>();
    REQUIRE(ComponentResult);
    ComponentResult->Class.EditAssetName() = "Conduit/TestClass.conduitclass";
    Host.AssetService.NotifyActiveAssetEditorRuntimeMutated(Session.TargetType, Session.TargetObject);

    REQUIRE(Host.AssetService.AssetEditorSession().RuntimeDirty);
    REQUIRE(Host.AssetService.SaveActiveAssetEditor());

    Host.AssetService.CloseAssetEditor();
    REQUIRE(Host.AssetService.OpenAssetEditorByKey(CreatedKey));

    Session = Host.AssetService.AssetEditorSession();
    REQUIRE(Session.IsOpen);

    auto* ReopenedNode = static_cast<BaseNode*>(Session.TargetObject);
    REQUIRE(ReopenedNode != nullptr);
    auto ReopenedComponent = ReopenedNode->Component<Conduit::ClassComponent>();
    REQUIRE(ReopenedComponent);
    CHECK(ReopenedComponent->Class.GetAssetName() == "Conduit/TestClass.conduitclass");
}

TEST_CASE("Editor asset service imports raw textures as authored texture assets",
          "[Assets][Editor][Source][Import]")
{
    RegisterBuiltinTypes();

    TestEditorHost Host{};
    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);
    EditorServiceContext Context(Host);

    const std::filesystem::path SourceTexture = WriteTinyPngFixture(Root.Path / "ImportInput");

    REQUIRE(Host.AssetService.RefreshDiscovery());
    REQUIRE(Host.AssetService.ImportSourceAsset(Context, SourceTexture.string(), "Rendering", {}));

    const auto TextureAssets = FindDiscoveredAssetsByType<TextureAsset>(Host.AssetService, "Rendering/");
    REQUIRE(TextureAssets.size() == 1u);

    const auto* ImportedTexture = TextureAssets.front();
    REQUIRE(ImportedTexture != nullptr);
    const std::string ImportedTextureKey = ImportedTexture->Key;
    CHECK(ImportedTexture->Key.ends_with(".texture"));
    CHECK(ImportedTexture->CanSave);

    const auto* Selected = Host.AssetService.SelectedAsset();
    REQUIRE(Selected != nullptr);
    CHECK(Selected->Key == ImportedTexture->Key);
    CHECK(Selected->AssetType == StaticTypeId<TextureAsset>());

    TextureAsset SavedTexture{};
    const std::string SavedTextureJson = ReadTextFile(Root.Path / std::filesystem::path(ImportedTexture->Key));
    REQUIRE(DeserializeAuthoredAssetFromJson(SavedTextureJson, SavedTexture));
    CHECK(SavedTexture.Image.Width == 1u);
    CHECK(SavedTexture.Image.Height == 1u);
    CHECK(SavedTexture.Image.Channels == 4u);
    CHECK_FALSE(SavedTexture.Image.EncodedBytes.empty());
    CHECK(SavedTexture.Image.Pixels.empty());
    CHECK(SavedTextureJson.find("\"EncodedBytesBase64\"") != std::string::npos);
    CHECK(SavedTextureJson.find("\"Pixels\"") == std::string::npos);
    CHECK(SavedTexture.ImportSettings.Target == ETextureCompressionTarget::BCn);
    CHECK(SavedTexture.ImportSettings.Format == ETextureCompressionFormat::Auto);

    REQUIRE(Host.AssetService.OpenAssetEditorByKey(ImportedTexture->Key));
    const auto Session = Host.AssetService.AssetEditorSession();
    REQUIRE(Session.IsOpen);
    CHECK(Session.TargetType == StaticTypeId<TextureAsset>());
    auto* Texture = static_cast<TextureAsset*>(Session.TargetObject);
    REQUIRE(Texture != nullptr);
    CHECK(Texture->Image.Width == 1u);
    CHECK(Texture->Image.Height == 1u);
    CHECK(Session.HasImportSettings);
    CHECK(Session.ImportSettingsType == StaticTypeId<TextureImporterSettings>());
    auto* ImportSettings = static_cast<TextureImporterSettings*>(Session.ImportSettingsObject);
    REQUIRE(ImportSettings != nullptr);
    CHECK(ImportSettings->Target == ETextureCompressionTarget::BCn);
    CHECK(ImportSettings->Format == ETextureCompressionFormat::Auto);
    CHECK(Session.CanReimport);

    ImportSettings->ForceLinear = true;
    ImportSettings->MaxMips = 3u;
    Host.AssetService.NotifyActiveAssetEditorImportSettingsMutated(Session.ImportSettingsType, Session.ImportSettingsObject);
    CHECK(Host.AssetService.AssetEditorSession().IsDirty);
    CHECK(Host.AssetService.AssetEditorSession().ImportSettingsDirty);
    REQUIRE(Host.AssetService.SaveActiveAssetEditor());
    CHECK(Host.AssetService.AssetEditorSession().IsOpen);
    CHECK(Host.AssetService.AssetEditorSession().AssetKey == ImportedTextureKey);

    TextureAsset ResavedTexture{};
    REQUIRE(DeserializeAuthoredAssetFromJson(
        ReadTextFile(Root.Path / std::filesystem::path(ImportedTextureKey)),
        ResavedTexture));
    CHECK(ResavedTexture.ImportSettings.ForceLinear);
    CHECK(ResavedTexture.ImportSettings.MaxMips == 3u);

    const auto* RefreshedImportedTexture = Host.AssetService.SelectedAsset();
    REQUIRE(RefreshedImportedTexture != nullptr);
    CHECK(RefreshedImportedTexture->Key == ImportedTextureKey);

    EditorAssetIconService IconService{};
    REQUIRE(IconService.Initialize(Context));
    const auto ThumbnailMetadata = IconService.ResolveAssetIcon(Context, *RefreshedImportedTexture, nullptr);
    CHECK(ThumbnailMetadata.TextureId == 0u);
    CHECK_FALSE(ThumbnailMetadata.IconSource.empty());
    CHECK(std::filesystem::exists(std::filesystem::path(ThumbnailMetadata.IconSource)));
    CHECK(std::filesystem::path(ThumbnailMetadata.IconSource).extension() == ".png");

    const auto CachedThumbnailMetadata = IconService.ResolveAssetIcon(Context, *RefreshedImportedTexture, nullptr);
    CHECK(CachedThumbnailMetadata.IconSource == ThumbnailMetadata.IconSource);
    CHECK(CachedThumbnailMetadata.TextureId == 0u);
    IconService.Shutdown(Context);

    REQUIRE(Host.AssetService.OpenAssetEditorByKey(ImportedTextureKey));
    const auto ReopenedSession = Host.AssetService.AssetEditorSession();
    REQUIRE(ReopenedSession.IsOpen);
    auto* ReopenedImportSettings = static_cast<TextureImporterSettings*>(ReopenedSession.ImportSettingsObject);
    REQUIRE(ReopenedImportSettings != nullptr);
    CHECK(ReopenedImportSettings->ForceLinear);
    CHECK(ReopenedImportSettings->MaxMips == 3u);
}

TEST_CASE("Editor asset service imports models into authored sibling assets and opens the authored mesh",
          "[Assets][Editor][Source][Import]")
{
    TempDir ImportRoot{};
    RegisterBuiltinTypes();

    TestEditorHost Host{};
    TempDir Root{};
    ScopedAssetRoot AssetRoot(Root.Path);
    EditorServiceContext Context(Host);
    const std::filesystem::path SourceModel = WriteEmbeddedTextureGltfFixture(ImportRoot.Path / "ImportInputModel");

    REQUIRE(Host.AssetService.RefreshDiscovery());
    const auto ImportResult = Host.AssetService.ImportSourceAsset(Context, SourceModel.string(), "Rendering", {});
    const std::string ImportErrorMessage = ImportResult ? std::string{} : ImportResult.error().Message;
    INFO(ImportErrorMessage);
    REQUIRE(ImportResult);

    const auto StaticMeshes = FindDiscoveredAssetsByType<StaticMeshAsset>(Host.AssetService, "Rendering/");
    const auto Materials = FindDiscoveredAssetsByType<MaterialAsset>(Host.AssetService, "Rendering/");
    const auto MaterialInstances = FindDiscoveredAssetsByType<MaterialInstanceAsset>(Host.AssetService, "Rendering/");
    const auto Textures = FindDiscoveredAssetsByType<TextureAsset>(Host.AssetService, "Rendering/");

    REQUIRE(StaticMeshes.size() == 1u);
    REQUIRE(Materials.size() == 1u);
    REQUIRE(MaterialInstances.size() == 1u);
    REQUIRE(Textures.size() == 1u);
    const std::string StaticMeshKey = StaticMeshes.front()->Key;
    const std::string MaterialInstanceKey = MaterialInstances.front()->Key;

    const auto* Selected = Host.AssetService.SelectedAsset();
    REQUIRE(Selected != nullptr);
    CHECK(Selected->Key == StaticMeshes.front()->Key);
    CHECK(Selected->AssetType == StaticTypeId<StaticMeshAsset>());

    StaticMeshAsset SavedMesh{};
    REQUIRE(DeserializeAuthoredAssetFromJson(ReadTextFile(Root.Path / std::filesystem::path(StaticMeshes.front()->Key)), SavedMesh));
    REQUIRE(SavedMesh.Mesh.MaterialInstances.size() == 1u);
    CHECK(SavedMesh.Mesh.MaterialInstances.front().GetAssetName() == MaterialInstances.front()->Key);
    CHECK(SavedMesh.Mesh.MaterialInstances.front().GetAssetId() == MaterialInstances.front()->AssetId.ToString());

    MaterialInstanceAsset SavedMaterialInstance{};
    REQUIRE(DeserializeAuthoredAssetFromJson(
        ReadTextFile(Root.Path / std::filesystem::path(MaterialInstances.front()->Key)),
        SavedMaterialInstance));
    CHECK(SavedMaterialInstance.ParentMaterial.AssetName == Materials.front()->Key);
    CHECK(SavedMaterialInstance.ParentMaterial.AssetId == Materials.front()->AssetId.ToString());
    REQUIRE(SavedMaterialInstance.Textures.size() == 1u);
    CHECK(SavedMaterialInstance.Textures.front().Texture.AssetName == Textures.front()->Key);
    CHECK(SavedMaterialInstance.Textures.front().Texture.AssetId == Textures.front()->AssetId.ToString());
    CHECK(SavedMaterialInstance.Textures.front().Texture.AssetName.ends_with(".texture"));

    TextureAsset SavedTexture{};
    REQUIRE(DeserializeAuthoredAssetFromJson(ReadTextFile(Root.Path / std::filesystem::path(Textures.front()->Key)), SavedTexture));
    CHECK(SavedTexture.Image.Width == 1u);
    CHECK(SavedTexture.Image.Height == 1u);
    CHECK_FALSE(SavedTexture.Image.EncodedBytes.empty());
    CHECK(SavedTexture.Image.Pixels.empty());

    REQUIRE(Host.AssetService.OpenAssetEditorByKey(StaticMeshes.front()->Key));
    const auto Session = Host.AssetService.AssetEditorSession();
    REQUIRE(Session.IsOpen);
    CHECK(Session.TargetType == StaticTypeId<StaticMeshAsset>());
    auto* Mesh = static_cast<StaticMeshAsset*>(Session.TargetObject);
    REQUIRE(Mesh != nullptr);
    REQUIRE(Mesh->Mesh.MaterialInstances.size() == 1u);
    CHECK(Mesh->Mesh.MaterialInstances.front().GetAssetName() == MaterialInstances.front()->Key);
    CHECK(Session.HasImportSettings);
    CHECK(Session.ImportSettingsType == StaticTypeId<AssimpImporterSettings>());
    auto* ImportSettings = static_cast<AssimpImporterSettings*>(Session.ImportSettingsObject);
    REQUIRE(ImportSettings != nullptr);
    CHECK(ImportSettings->Mesh.ImportMaterials);
    CHECK(ImportSettings->Mesh.ImportTextures);
    CHECK(Session.CanReimport);

    ImportSettings->Mesh.FlipUVs = true;
    ImportSettings->Mesh.MaxBonesPerVertex = 6u;
    Host.AssetService.NotifyActiveAssetEditorImportSettingsMutated(Session.ImportSettingsType, Session.ImportSettingsObject);
    CHECK(Host.AssetService.AssetEditorSession().IsDirty);
    CHECK(Host.AssetService.AssetEditorSession().ImportSettingsDirty);
    REQUIRE(Host.AssetService.SaveActiveAssetEditor());

    StaticMeshAsset ResavedMesh{};
    REQUIRE(DeserializeAuthoredAssetFromJson(
        ReadTextFile(Root.Path / std::filesystem::path(StaticMeshKey)),
        ResavedMesh));
    CHECK(ResavedMesh.ImportSettings.Mesh.FlipUVs);
    CHECK(ResavedMesh.ImportSettings.Mesh.MaxBonesPerVertex == 6u);

    REQUIRE(Host.AssetService.OpenAssetEditorByKey(StaticMeshKey));
    const auto ReopenedSession = Host.AssetService.AssetEditorSession();
    REQUIRE(ReopenedSession.IsOpen);
    auto* ReopenedImportSettings = static_cast<AssimpImporterSettings*>(ReopenedSession.ImportSettingsObject);
    REQUIRE(ReopenedImportSettings != nullptr);
    CHECK(ReopenedImportSettings->Mesh.FlipUVs);
    CHECK(ReopenedImportSettings->Mesh.MaxBonesPerVertex == 6u);

    const std::filesystem::path MetadataPath = Root.Path / ".snapi_editor" / "asset_import_metadata.json";
    std::error_code RemoveError{};
    REQUIRE(std::filesystem::remove(MetadataPath, RemoveError));
    REQUIRE_FALSE(RemoveError);

    {
        TestEditorHost ReloadedHost{};
        ScopedAssetRoot ReloadedAssetRoot(Root.Path);

        REQUIRE(ReloadedHost.AssetService.RefreshDiscovery());
        REQUIRE(ReloadedHost.AssetService.OpenAssetEditorByKey(MaterialInstanceKey));
        const auto FallbackSession = ReloadedHost.AssetService.AssetEditorSession();
        REQUIRE(FallbackSession.IsOpen);
        CHECK(FallbackSession.HasImportSettings);
        CHECK(FallbackSession.ImportSettingsType == StaticTypeId<AssimpImporterSettings>());
        CHECK(FallbackSession.CanReimport);
        auto* FallbackImportSettings = static_cast<AssimpImporterSettings*>(FallbackSession.ImportSettingsObject);
        REQUIRE(FallbackImportSettings != nullptr);
        CHECK(FallbackImportSettings->Mesh.FlipUVs);
        CHECK(FallbackImportSettings->Mesh.MaxBonesPerVertex == 6u);
    }
}
