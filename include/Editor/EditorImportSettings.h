#pragma once

#include <cstdint>
#include <string>

namespace SnAPI::GameFramework::Editor
{

/**
 * @brief Reflected settings for Assimp-driven model imports.
 */
struct AssimpImportSettings
{
    static constexpr const char* kTypeName = "SnAPI.GameFramework.Editor.AssimpImportSettings";

    bool GenerateNormals = true;
    bool GenerateTangents = true;
    bool FlipUVs = false;
    bool OptimizeMeshes = true;
    bool ForceSkeletal = false;
    bool ForceStatic = false;
    uint32_t MaxBonesPerVertex = 4;
};

/**
 * @brief Reflected settings for TextureCompressor-driven texture imports.
 */
struct TextureImportSettings
{
    static constexpr const char* kTypeName = "SnAPI.GameFramework.Editor.TextureImportSettings";

    std::string Target = "bcn";
    std::string Format{};
    float Quality = 0.6f;
    bool ForceSrgb = false;
    bool ForceLinear = false;
    bool ForceNormalMap = false;
    uint32_t MaxMips = 0;
};

} // namespace SnAPI::GameFramework::Editor
