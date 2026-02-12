#pragma once

#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include "Expected.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Auto-registration registry for reflected types keyed by TypeId.
 * @remarks This is used to lazily register reflection metadata on first use.
 *
 * The intent is:
 * - Each reflected type registers a lightweight "ensure" callback at static-init time
 *   (via SNAPI_REFLECT_TYPE in a .cpp).
 * - TypeRegistry/serialization can call Ensure(TypeId) on demand when a TypeId is
 *   encountered before its TypeInfo has been registered.
 *
 * This avoids relying on cross-TU static initialization order for the heavy
 * TypeRegistry registration work.
 *
 * Contract:
 * - ensure callbacks must be idempotent and thread-safe for repeated calls
 * - registration collisions are tolerated only when callback identity matches
 */
class TypeAutoRegistry
{
public:
    /** @brief Ensure callback signature. Should be idempotent. */
    using EnsureFn = Result(*)();

    /** @brief Access singleton instance. */
    static TypeAutoRegistry& Instance();

    /**
     * @brief Register an ensure callback for a TypeId.
     * @param Id Stable type id.
     * @param Name Stable type name (for diagnostics).
     * @param Fn Ensure function pointer.
     * @remarks Duplicate registrations are ignored if identical.
     */
    void Register(const TypeId& Id, std::string_view Name, EnsureFn Fn);

    /**
     * @brief Ensure a TypeId has been registered with TypeRegistry.
     * @param Id Type id.
     * @return Success or error.
     * @remarks Returns NotFound if no ensure callback exists for Id.
     */
    Result Ensure(const TypeId& Id) const;

    /** @brief Check whether an ensure callback is registered for Id. */
    bool Has(const TypeId& Id) const;

private:
    mutable std::mutex m_mutex{}; /**< @brief Protects ensure callback and diagnostics maps. */
    std::unordered_map<TypeId, EnsureFn, UuidHash> m_entries{}; /**< @brief TypeId -> ensure callback mapping. */
    std::unordered_map<TypeId, std::string, UuidHash> m_names{}; /**< @brief Optional diagnostics map of TypeId -> human-readable type name. */
};

} // namespace SnAPI::GameFramework
