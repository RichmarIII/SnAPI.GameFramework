#pragma once

#include "Expected.h"

#include <string_view>
#include <typeindex>
#include <vector>

namespace SnAPI::GameFramework
{
class GameRuntime;
}

namespace SnAPI::GameFramework::Editor
{

class IEditorService;
class IEditorServiceHost;

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Lightweight execution context passed to editor services.
 *
 * `EditorServiceContext` is the narrow bridge between a service and the editor host.
 * It intentionally exposes only:
 * - the owning `GameRuntime`
 * - service lookup through the hosting `GameEditor`
 *
 * The context exists so services can cooperate without directly depending on the concrete
 * `GameEditor` implementation or reaching through global state.
 *
 * Ownership and lifetime:
 * - The context stores a non-owning pointer to the active `IEditorServiceHost`.
 * - It is created transiently by the host during initialize/tick/shutdown calls.
 * - Pointers returned from `GetService()` remain borrowed and are invalidated if the
 *   referenced service is unregistered or the editor shuts down.
 *
 * Threading model:
 * - Main-thread only.
 *
 * @see IEditorService
 * @see IEditorServiceHost
 * @see GameEditor
 */
class EditorServiceContext final
{
public:
    explicit EditorServiceContext(IEditorServiceHost& Host)
        : m_host(&Host)
    {
    }

    /**
     * @brief Access the runtime owned by the hosting editor.
     * @return Borrowed runtime reference.
     * @pre The host pointer supplied at construction must still be valid.
     * @warning The returned reference becomes invalid when the editor shuts down.
     */
    [[nodiscard]] SnAPI::GameFramework::GameRuntime& Runtime();
    /**
     * @brief Access the runtime owned by the hosting editor.
     * @return Borrowed runtime reference.
     * @pre The host pointer supplied at construction must still be valid.
     */
    [[nodiscard]] const SnAPI::GameFramework::GameRuntime& Runtime() const;
    /**
     * @brief Access the service host that created this context.
     * @return Borrowed host reference.
     */
    [[nodiscard]] IEditorServiceHost& Host() const { return *m_host; }

    /**
     * @brief Resolve another registered editor service by exact concrete type.
     * @tparam TService Concrete service type to query.
     * @return Non-owning pointer to the matching service, or `nullptr` when no such service is registered.
     * @remarks
     * Resolution is keyed by `std::type_index(typeid(TService))`. This is not a polymorphic
     * "find any derived type" lookup.
     */
    template<typename TService>
    [[nodiscard]] TService* GetService();

    /**
     * @brief Resolve another registered editor service by exact concrete type.
     * @tparam TService Concrete service type to query.
     * @return Non-owning pointer to the matching service, or `nullptr` when no such service is registered.
     */
    template<typename TService>
    [[nodiscard]] const TService* GetService() const;

private:
    IEditorServiceHost* m_host = nullptr;
};

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Internal host contract consumed by `EditorServiceContext`.
 *
 * `IEditorServiceHost` abstracts the minimum services the editor runtime must provide so
 * `EditorServiceContext` can remain decoupled from `GameEditor`. External code typically
 * should not implement or consume this interface directly unless it is providing an alternate
 * editor host.
 *
 * Ownership and lifetime:
 * - Implementations own the runtime and registered services they expose.
 * - Returned pointers are non-owning and follow the host's shutdown/unregister lifetime.
 *
 * Threading model:
 * - Main-thread only.
 */
class IEditorServiceHost
{
public:
    virtual ~IEditorServiceHost() = default;
    /** @brief Access the runtime used for editor service execution. @return Borrowed runtime reference. */
    [[nodiscard]] virtual SnAPI::GameFramework::GameRuntime& RuntimeForServices() = 0;
    /** @brief Access the runtime used for editor service execution. @return Borrowed runtime reference. */
    [[nodiscard]] virtual const SnAPI::GameFramework::GameRuntime& RuntimeForServices() const = 0;
    /** @brief Resolve a registered service by exact concrete type. @return Non-owning pointer or `nullptr`. */
    [[nodiscard]] virtual IEditorService* ResolveServiceForContext(const std::type_index& Type) = 0;
    /** @brief Resolve a registered service by exact concrete type. @return Non-owning pointer or `nullptr`. */
    [[nodiscard]] virtual const IEditorService* ResolveServiceForContext(const std::type_index& Type) const = 0;
};

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Contract for modular editor subsystems.
 *
 * `IEditorService` is the extension point used by `GameEditor` to assemble editor behavior
 * from independent modules. A service typically owns one focused concern such as selection,
 * scene bootstrapping, layout, asset management, or viewport binding.
 *
 * Core semantics:
 * - Services are registered by concrete type.
 * - `Dependencies()` declares hard initialization requirements on other service types.
 * - `Priority()` breaks ties only among services whose dependencies are already satisfied.
 * - `Initialize()` is called at most once per registration lifetime.
 * - `Shutdown()` is called before removal or editor shutdown if initialization succeeded.
 *
 * Ownership and lifetime:
 * - Services are owned by `GameEditor`.
 * - Services may keep borrowed references to runtime/world state only while initialized.
 *
 * Threading model:
 * - Main-thread only.
 *
 * @warning `Dependencies()` must list exact concrete service types registered in the host.
 *          Missing or circular dependencies cause editor initialization to fail.
 *
 * @see GameEditor
 * @see EditorServiceContext
 */
class IEditorService
{
public:
    virtual ~IEditorService() = default;

    /**
     * @brief Stable service name for diagnostics and error reporting.
     * @return Borrowed string view. Implementations typically return static storage.
     */
    [[nodiscard]] virtual std::string_view Name() const = 0;

    /**
     * @brief Hard dependencies required before this service may initialize.
     * @return List of exact concrete service types.
     * @remarks
     * The returned type list is interpreted as an acyclic dependency graph by `GameEditor`.
     * Returning a type that is not registered is an initialization error.
     */
    [[nodiscard]] virtual std::vector<std::type_index> Dependencies() const { return {}; }

    /**
     * @brief Ordering hint among services whose dependencies are already satisfied.
     * @return Signed priority value; lower values initialize earlier.
     * @remarks This value never overrides declared dependencies.
     */
    [[nodiscard]] virtual int Priority() const { return 0; }

    /**
     * @brief Initialize service state.
     * @param Context Borrowed execution context for runtime and peer-service access.
     * @return Success or an initialization error.
     * @pre All dependencies listed by `Dependencies()` have already initialized successfully.
     * @post On success, `Tick()` and `Shutdown()` may be called later in the same registration lifetime.
     * @warning If initialization fails, the editor host may roll back previously initialized services.
     */
    virtual Result Initialize(EditorServiceContext& Context) = 0;

    /**
     * @brief Per-frame update hook.
     * @param Context Borrowed execution context.
     * @param DeltaSeconds Variable-step frame delta in seconds.
     * @remarks Called only after successful initialization.
     */
    virtual void Tick(EditorServiceContext& Context, float DeltaSeconds)
    {
        (void)Context;
        (void)DeltaSeconds;
    }

    /**
     * @brief Shutdown and release service state.
     * @param Context Borrowed execution context.
     * @remarks
     * Called before the service is destroyed or unregistered. Implementations should release
     * borrowed runtime/world state here and must tolerate being called during broader editor teardown.
     */
    virtual void Shutdown(EditorServiceContext& Context) = 0;
};

inline SnAPI::GameFramework::GameRuntime& EditorServiceContext::Runtime()
{
    return m_host->RuntimeForServices();
}

inline const SnAPI::GameFramework::GameRuntime& EditorServiceContext::Runtime() const
{
    return m_host->RuntimeForServices();
}

template<typename TService>
TService* EditorServiceContext::GetService()
{
    return static_cast<TService*>(m_host->ResolveServiceForContext(std::type_index(typeid(TService))));
}

template<typename TService>
const TService* EditorServiceContext::GetService() const
{
    return static_cast<const TService*>(m_host->ResolveServiceForContext(std::type_index(typeid(TService))));
}

} // namespace SnAPI::GameFramework::Editor
