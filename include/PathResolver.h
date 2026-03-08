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
 * @ingroup SnAPI_GameFramework
 * @brief Global schema-aware resolver for filesystem-like path strings.
 *
 * `SPathResolver` translates logical URI-style inputs such as `asset://...` and `editor://...`
 * into normalized filesystem paths. It centralizes schema registration so subsystems can exchange
 * stable logical paths without hard-coding project-relative or install-relative disk layouts.
 *
 * Core semantics:
 * - Known schemas dispatch to registered handlers.
 * - Plain paths without `scheme://` are normalized and treated as native filesystem paths.
 * - Built-in `asset://` and `editor://` handlers enforce that resolved paths stay within their
 *   configured roots.
 *
 * Ownership and lifetime:
 * - This is a process-wide singleton.
 * - Registered schema handlers are copied into internal storage and remain active until explicitly removed.
 *
 * Threading model:
 * - Concurrent calls are internally synchronized.
 *
 * @warning Handlers should avoid calling back into `SPathResolver` in a way that would deadlock on
 * the resolver mutex.
 */
struct SNAPI_GAMEFRAMEWORK_API SPathResolver final
{
    /**
     * @brief Callback signature for custom schema resolution.
     * @param Remainder Text after `scheme://`.
     * @return Resolved filesystem path, or an error when unresolved/invalid.
     */
    using SchemaHandler = std::function<TExpected<std::filesystem::path>(std::string_view Remainder)>;

    /** @brief Access the process-wide resolver singleton. */
    [[nodiscard]] static SPathResolver& Instance();

    /**
     * @brief Resolve path text to a normalized filesystem path.
     * @param Value Input path or URI in `scheme://...` form or a native filesystem path.
     * @return Resolved filesystem path or an error.
     *
     * Resolution behavior:
     * - `scheme://...` => dispatch to registered schema handler
     * - otherwise => treat as native filesystem path
     */
    [[nodiscard]] TExpected<std::filesystem::path> Resolve(std::string_view Value) const;

    /**
     * @brief Resolve a path and return it as a string.
     * @param Value Input path or URI.
     * @return Resolved filesystem path encoded as a string, or an error.
     */
    [[nodiscard]] TExpected<std::string> ResolveToString(std::string_view Value) const;

    /**
     * @brief Register or replace a custom schema handler.
     * @param Scheme Schema identifier without the `://` delimiter.
     * @param Handler Resolution callback.
     * @return Success or an error.
     *
     * Schema names are normalized to lowercase ASCII and must satisfy the resolver's schema-name rules.
     */
    Result RegisterSchemaHandler(std::string_view Scheme, SchemaHandler Handler);

    /**
     * @brief Remove a previously registered schema handler.
     * @param Scheme Schema identifier without delimiter.
     * @return `true` if a handler was removed.
     */
    bool UnregisterSchemaHandler(std::string_view Scheme);

    /**
     * @brief Set the root directory used by the built-in `asset://` schema.
     * @param RootPath Filesystem root.
     * @return Success or an error.
     */
    Result SetAssetRoot(std::filesystem::path RootPath);

    /**
     * @brief Get the current `asset://` root directory.
     * @return Copy of the configured asset root path.
     */
    [[nodiscard]] std::filesystem::path AssetRoot() const;

    /**
     * @brief Set the root directory used by the built-in `editor://` schema.
     * @param RootPath Filesystem root.
     * @return Success or an error.
     */
    Result SetEditorRoot(std::filesystem::path RootPath);

    /**
     * @brief Get the current `editor://` root directory.
     * @return Copy of the configured editor root path.
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
