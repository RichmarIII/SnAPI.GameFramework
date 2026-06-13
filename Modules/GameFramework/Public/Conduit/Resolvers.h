#pragma once

#include "Conduit/Types.h"
#include "StaticTypeId.h"

namespace SnAPI::GameFramework::Conduit
{

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Simplified resolver signature stored in the builtin handle-family registry.
 *
 * Unlike `HandleResolverFn`, this registry-level signature assumes the caller already knows
 * the handle type associated with the slot. It is mainly used for default builtin handle
 * families such as `NodeHandle` and `ComponentHandle`.
 */
using RegisteredHandleResolverFn = TExpected<ResolvedTarget> (*)(const TypeInfo& ExpectedType, const void* HandleValue);

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Process-global registry mapping reflected handle types to Conduit resolver callbacks.
 *
 * Purpose:
 * - provide builtin automatic resolution for standard engine handle families
 * - let custom handle families plug into Conduit once, without per-concrete-type boilerplate
 *
 * Important design boundary:
 * - reflected types tell Conduit what a handle value is
 * - resolvers tell Conduit how to map that handle to a live runtime instance
 *
 * This registry is for handle families, not for every reflected node/component type.
 */
class HandleResolverRegistry
{
public:
    /**
     * @brief Access the process-global registry instance.
     * @return Registry singleton.
     */
    static HandleResolverRegistry& Instance();

    /**
     * @brief Register a resolver for one reflected handle type.
     * @param HandleType Reflected type id of the handle payload.
     * @param Resolver Resolver callback for that handle family.
     * @return Success or error.
     *
     * Re-registering the exact same callback is treated as success.
     * Registering a different callback for an existing handle type returns `AlreadyExists`.
     */
    Result Register(const TypeId& HandleType, RegisteredHandleResolverFn Resolver);

    /**
     * @brief Convenience overload registering by C++ handle type.
     * @tparam THandle Concrete handle type.
     * @param Resolver Resolver callback.
     * @return Success or error.
     */
    template<typename THandle>
    Result Register(RegisteredHandleResolverFn Resolver)
    {
        return Register(StaticTypeId<THandle>(), Resolver);
    }

    /**
     * @brief Find a resolver for one reflected handle type.
     * @param HandleType Reflected handle type id.
     * @return Resolver callback or null when no resolver is registered.
     */
    [[nodiscard]] RegisteredHandleResolverFn Find(const TypeId& HandleType) const;

private:
    HandleResolverRegistry() = default;
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Register the engine-provided builtin handle-family resolvers.
 *
 * Current builtin families:
 * - `NodeHandle`
 * - `ComponentHandle`
 *
 * The function is idempotent and safe to call repeatedly.
 */
void RegisterBuiltinHandleResolvers();

} // namespace SnAPI::GameFramework::Conduit
