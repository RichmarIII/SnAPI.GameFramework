#pragma once

#include <mutex>
#include "GameThreading.h"
#include <string>
#include <string_view>
#include <unordered_map>

#include "Expected.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Registry of lazy reflection-registration callbacks keyed by deterministic `TypeId`.
 *
 * `TypeAutoRegistry` decouples cheap static initialization from expensive `TypeRegistry` mutation.
 * Each reflected type installs an ensure callback during static initialization, and the callback is
 * executed only when some runtime path first needs the metadata.
 *
 * Core semantics:
 * - `Register()` stores the first callback for a `TypeId`.
 * - A second registration for the same `TypeId` is ignored and debug-asserted unless it is the same callback.
 * - `Ensure()` looks up the callback without holding the lock during execution.
 * - `EnsureAll()` snapshots the current key set and attempts every ensure callback, returning the first error but continuing best-effort.
 *
 * Threading model:
 * - Thread-safe. Internal maps are guarded by `GameMutex`.
 * - Ensure callbacks themselves must still be idempotent and safe for repeated calls.
 */
class TypeAutoRegistry
{
public:
    /** @brief Ensure callback signature. Implementations should be idempotent and return `Ok()` if the type is already registered. */
    using EnsureFn = Result(*)();

    /** @brief Access the process-wide singleton. */
    static TypeAutoRegistry& Instance();

    /**
     * @brief Register an ensure callback for a `TypeId`.
     * @param Id Stable type id.
     * @param Name Stable type name (for diagnostics).
     * @param Fn Ensure function pointer.
     *
     * The first callback wins. Later registrations for the same id are ignored; in debug builds the
     * registry asserts if the callback pointer differs.
     */
    void Register(const TypeId& Id, std::string_view Name, EnsureFn Fn);

    /**
     * @brief Ensure that a `TypeId` has registered metadata in `TypeRegistry`.
     * @param Id Type id.
     * @return Success or error.
     *
     * Returns `NotFound` when no auto-registration entry exists for the supplied id.
     */
    Result Ensure(const TypeId& Id) const;

    /**
     * @brief Ensure every currently registered auto-type has been registered with `TypeRegistry`.
     * @return Success or the first encountered error.
     *
     * The registry continues best-effort after the first failure so later entries still get a chance
     * to register.
     */
    Result EnsureAll() const;

    /** @brief Check whether an ensure callback exists for a `TypeId`. */
    bool Has(const TypeId& Id) const;

private:
    mutable GameMutex m_mutex{}; /**< @brief Protects ensure callback and diagnostics maps. */
    std::unordered_map<TypeId, EnsureFn, UuidHash> m_entries{}; /**< @brief TypeId -> ensure callback mapping. */
    std::unordered_map<TypeId, std::string, UuidHash> m_names{}; /**< @brief Optional diagnostics map of TypeId -> human-readable type name. */
};

} // namespace SnAPI::GameFramework
