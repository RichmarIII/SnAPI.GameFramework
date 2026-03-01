#pragma once

#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "Expected.h"
#include "Export.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Global schema-aware path resolver for all filesystem path lookups.
 * @remarks
 * `SPathResolver` translates logical URI-like inputs (for example `asset://...` and `editor://...`)
 * into concrete filesystem paths before consumers pass them to file APIs.
 *
 * Callers should resolve any incoming path text through this singleton before
 * performing filesystem operations.
 */
struct SNAPI_GAMEFRAMEWORK_API SPathResolver final
{
    /**
     * @brief Callback signature for custom schema resolution.
     * @param Remainder Text after `scheme://`.
     * @return Resolved filesystem path, or an error when unresolved/invalid.
     */
    using SchemaHandler = std::function<TExpected<std::filesystem::path>(std::string_view Remainder)>;

    /**
     * @brief Access the singleton resolver.
     */
    [[nodiscard]] static SPathResolver& Instance();

    /**
     * @brief Resolve path text to a normalized filesystem path.
     * @param Value Input path or URI (`scheme://...`).
     * @return Absolute, normalized filesystem path.
     *
     * Resolution behavior:
     * - `scheme://...` => dispatch to registered schema handler
     * - otherwise => treat as native filesystem path
     */
    [[nodiscard]] TExpected<std::filesystem::path> Resolve(std::string_view Value) const;

    /**
     * @brief Resolve and stringify a path.
     */
    [[nodiscard]] TExpected<std::string> ResolveToString(std::string_view Value) const;

    /**
     * @brief Register (or replace) a schema handler.
     * @param Scheme Schema identifier without delimiter (for example `asset`).
     * @param Handler Handler callback.
     */
    Result RegisterSchemaHandler(std::string_view Scheme, SchemaHandler Handler);

    /**
     * @brief Remove a previously registered schema handler.
     * @return True if removed.
     */
    bool UnregisterSchemaHandler(std::string_view Scheme);

    /**
     * @brief Set the root directory used by the built-in `asset://` schema.
     */
    Result SetAssetRoot(std::filesystem::path RootPath);

    /**
     * @brief Get the current `asset://` root directory.
     */
    [[nodiscard]] std::filesystem::path AssetRoot() const;

    /**
     * @brief Set the root directory used by the built-in `editor://` schema.
     */
    Result SetEditorRoot(std::filesystem::path RootPath);

    /**
     * @brief Get the current `editor://` root directory.
     */
    [[nodiscard]] std::filesystem::path EditorRoot() const;

private:
    SPathResolver();

    struct ParsedSchema
    {
        std::string Scheme{};
        std::string_view Remainder{};
    };

    [[nodiscard]] static std::optional<ParsedSchema> ParseSchema(std::string_view Value);
    [[nodiscard]] static bool IsValidSchemaName(std::string_view Name);
    [[nodiscard]] static std::string ToLowerAscii(std::string_view Value);
    [[nodiscard]] static std::filesystem::path NormalizeForFilesystem(std::filesystem::path Value);
    [[nodiscard]] static std::filesystem::path ResolveDefaultAssetRoot();
    [[nodiscard]] static std::filesystem::path ResolveDefaultEditorRoot();
    [[nodiscard]] TExpected<std::filesystem::path> ResolveAssetPath(std::string_view Remainder) const;
    [[nodiscard]] TExpected<std::filesystem::path> ResolveEditorPath(std::string_view Remainder) const;
    [[nodiscard]] static bool IsPathWithin(const std::filesystem::path& Root, const std::filesystem::path& Candidate);

    mutable std::mutex m_mutex{};
    std::unordered_map<std::string, SchemaHandler> m_handlers{};
    std::filesystem::path m_assetRoot{};
    std::filesystem::path m_editorRoot{};
};

} // namespace SnAPI::GameFramework
