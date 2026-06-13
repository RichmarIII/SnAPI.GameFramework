#pragma once

#include "Editor/EditorExport.h"
#include "Conduit/Editor/Document.h"

namespace SnAPI::GameFramework::Conduit::Editor
{

/**
 * @ingroup SnAPI_GameFramework_Conduit_Editor
 * @brief Narrow bridge that compiles authored graph documents into runtime Conduit graphs.
 *
 * The bridge exists so the editor layer can cache diagnostics, later add authored-to-runtime
 * lowering, and eventually map diagnostics back to authored nodes without forcing the UI to call
 * low-level runtime compile functions directly.
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API CompilerBridge
{
public:
    /**
     * @brief Compile one authored graph payload.
     * @param Asset Authored graph asset.
     * @return Compiled runtime graph and diagnostics.
     */
    [[nodiscard]] CompileOutput Compile(const GraphAsset& Asset) const;

    /**
     * @brief Compile one open authored graph document.
     * @param Document Open document to compile.
     * @return Compiled runtime graph and diagnostics.
     */
    [[nodiscard]] CompileOutput Compile(const GraphDocument& Document) const;
};

} // namespace SnAPI::GameFramework::Conduit::Editor
