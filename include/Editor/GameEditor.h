#pragma once

#include "Editor/EditorExport.h"
#include "Editor/IEditorService.h"
#include "GameRuntime.h"

#include <memory>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace SnAPI::GameFramework::Editor
{

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Bootstrap settings for `GameEditor`.
 *
 * `GameEditorSettings` is intentionally small because the editor host delegates most runtime
 * concerns to `GameRuntimeSettings`. The editor-specific layer mainly decides which world
 * flavor to create and which editor services should run around it.
 *
 * Ownership:
 * - `Runtime` is copied into the editor host during `Initialize()`.
 *
 * @see GameEditor
 * @see GameRuntimeSettings
 */
struct GameEditorSettings
{
    GameRuntimeSettings Runtime{}; /**< @brief Runtime bootstrap settings used to create the editor world and optional subsystems. */
};

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief High-level editor host layered on top of `GameRuntime`.
 *
 * `GameEditor` is the application-facing entry point for the editor module. It owns one
 * `GameRuntime`, registers and orders editor services, and coordinates the special bootstrap
 * work needed to make editor-only scene content safe to initialize.
 *
 * Core semantics:
 * - `Initialize()` resets any previous session, initializes the runtime, then initializes editor services.
 * - `Update()` ticks editor services before forwarding the frame to `GameRuntime::Update()`.
 * - Service initialization obeys dependency order first and `Priority()` second.
 * - `UnregisterService()` removes the target service and any transitive dependents.
 *
 * Bootstrap ordering:
 * - During editor module startup the host defers node/component `OnCreate` work until the
 *   editor viewport and related UI bindings have had a chance to materialize.
 * - This keeps editor-authored scene bootstrap nodes from running render-dependent setup
 *   before viewports and feature profiles are ready.
 *
 * Ownership and lifetime:
 * - `GameEditor` owns the runtime and every registered service instance.
 * - References returned by `Runtime()`, `Settings()`, and `GetService()` are borrowed.
 * - Borrowed service pointers become invalid after unregistration or `Shutdown()`.
 *
 * Threading model:
 * - Main-thread only.
 *
 * @see GameRuntime
 * @see IEditorService
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API GameEditor final : public IEditorServiceHost
{
public:
    /**
     * @brief Initialize editor runtime.
     * @param Settings Editor bootstrap settings.
     * @return Success or an initialization error.
     * @remarks
     * Calling `Initialize()` always tears down any previous editor session first.
     * On success, default editor services are registered if they were not already present.
     */
    Result Initialize(const GameEditorSettings& Settings);

    /**
     * @brief Shutdown editor runtime.
     * @remarks
     * Services are shut down before the runtime is torn down. Safe to call repeatedly.
     */
    void Shutdown();

    /**
     * @brief Check whether editor runtime is initialized.
     * @return `true` when runtime and editor services completed initialization.
     */
    [[nodiscard]] bool IsInitialized() const;

    /**
     * @brief Update one frame.
     * @param DeltaSeconds Frame delta time in seconds.
     * @return `true` to continue running; `false` when runtime requests exit.
     * @remarks
     * Per-frame order:
     * 1. tick initialized editor services
     * 2. run `GameRuntime::Update()`
     */
    bool Update(float DeltaSeconds);

    /**
     * @brief Mutable access to wrapped `GameRuntime`.
     * @return Borrowed runtime reference.
     * @pre The editor should be initialized before callers depend on subsystem readiness.
     */
    [[nodiscard]] GameRuntime& Runtime();

    /**
     * @brief Const access to wrapped `GameRuntime`.
     * @return Borrowed runtime reference.
     */
    [[nodiscard]] const GameRuntime& Runtime() const;

    /**
     * @brief Access the last applied settings snapshot.
     * @return Borrowed settings reference.
     */
    [[nodiscard]] const GameEditorSettings& Settings() const;

    /**
     * @brief Register a concrete editor service type.
     * @tparam TService Concrete service type to create.
     * @tparam TArgs Constructor argument pack.
     * @param Args Constructor arguments forwarded into the new service.
     * @return Borrowed reference to the existing or newly created service.
     * @remarks
     * Registration is idempotent by exact type. This overload only inserts the service;
     * it does not attempt immediate initialization when the editor is already running.
     */
    template<typename TService, typename... TArgs>
    TService& RegisterService(TArgs&&... Args);

    /**
     * @brief Register a runtime-provided service instance.
     * @param Service Owning pointer to the service instance to adopt.
     * @return Success or an error.
     * @remarks
     * Registration is idempotent by concrete dynamic type. When editor runtime is
     * already initialized, newly registered services are initialized immediately
     * after dependency validation and ordering are recomputed.
     * @warning Ownership transfers to `GameEditor` on success.
     */
    Result RegisterService(std::unique_ptr<IEditorService> Service);

    /**
     * @brief Unregister a registered service type.
     * @param ServiceType Exact concrete service type to remove.
     * @return Success or an error.
     * @remarks
     * Removes the target service and any transitive dependents safely. Initialized
     * services are shut down in reverse dependency-safe order before destruction.
     */
    Result UnregisterService(const std::type_index& ServiceType);

    /**
     * @brief Unregister a registered service type.
     * @tparam TService Exact concrete service type to remove.
     * @return Success or an error.
     */
    template<typename TService>
    Result UnregisterService();

    /**
     * @brief Query a registered service by type.
     * @tparam TService Exact concrete service type to query.
     * @return Non-owning pointer or `nullptr` when the service is not registered.
     */
    template<typename TService>
    [[nodiscard]] TService* GetService();

    /**
     * @brief Query a registered service by type (const).
     * @tparam TService Exact concrete service type to query.
     * @return Non-owning pointer or `nullptr` when the service is not registered.
     */
    template<typename TService>
    [[nodiscard]] const TService* GetService() const;

private:
    struct ServiceEntry
    {
        std::type_index Type = std::type_index(typeid(void));
        std::unique_ptr<IEditorService> Instance{};
        bool Initialized = false;
    };

    Result InitializeRuntime(const GameEditorSettings& Settings);
    void EnsureDefaultServicesRegistered();
    Result BuildServiceOrder();
    Result InitializeServices();
    Result FinalizeBootstrapLifecycle();
    Result ApplyStartupSelectionIfPending();
    void TickServices(float DeltaSeconds);
    void ShutdownServices();
    void RebuildServiceIndexByType();
    Result InitializeEditorModules();
    void ShutdownEditorModules();

    [[nodiscard]] GameRuntime& RuntimeForServices() override;
    [[nodiscard]] const GameRuntime& RuntimeForServices() const override;
    [[nodiscard]] IEditorService* ResolveServiceForContext(const std::type_index& Type) override;
    [[nodiscard]] const IEditorService* ResolveServiceForContext(const std::type_index& Type) const override;

    GameEditorSettings m_settings{};
    GameRuntime m_runtime{};
    std::vector<ServiceEntry> m_services{};
    std::unordered_map<std::type_index, std::size_t> m_serviceIndexByType{};
    std::vector<std::size_t> m_serviceOrder{};
    std::string m_startupSelectionNodeName{};
    bool m_startupSelectionPending = false;
    bool m_defaultServicesRegistered = false;
    bool m_initialized = false;
};

template<typename TService, typename... TArgs>
TService& GameEditor::RegisterService(TArgs&&... Args)
{
    static_assert(std::is_base_of_v<IEditorService, TService>, "TService must derive from IEditorService");

    const std::type_index ServiceType = std::type_index(typeid(TService));
    if (const auto Existing = m_serviceIndexByType.find(ServiceType); Existing != m_serviceIndexByType.end())
    {
        return static_cast<TService&>(*m_services[Existing->second].Instance);
    }

    ServiceEntry Entry{};
    Entry.Type = ServiceType;
    Entry.Instance = std::make_unique<TService>(std::forward<TArgs>(Args)...);
    Entry.Initialized = false;

    const std::size_t NewIndex = m_services.size();
    m_services.emplace_back(std::move(Entry));
    m_serviceIndexByType.emplace(ServiceType, NewIndex);
    return static_cast<TService&>(*m_services.back().Instance);
}

template<typename TService>
TService* GameEditor::GetService()
{
    static_assert(std::is_base_of_v<IEditorService, TService>, "TService must derive from IEditorService");

    const auto It = m_serviceIndexByType.find(std::type_index(typeid(TService)));
    if (It == m_serviceIndexByType.end())
    {
        return nullptr;
    }

    return static_cast<TService*>(m_services[It->second].Instance.get());
}

template<typename TService>
const TService* GameEditor::GetService() const
{
    static_assert(std::is_base_of_v<IEditorService, TService>, "TService must derive from IEditorService");

    const auto It = m_serviceIndexByType.find(std::type_index(typeid(TService)));
    if (It == m_serviceIndexByType.end())
    {
        return nullptr;
    }

    return static_cast<const TService*>(m_services.at(It->second).Instance.get());
}

template<typename TService>
Result GameEditor::UnregisterService()
{
    static_assert(std::is_base_of_v<IEditorService, TService>, "TService must derive from IEditorService");
    return UnregisterService(std::type_index(typeid(TService)));
}

} // namespace SnAPI::GameFramework::Editor
