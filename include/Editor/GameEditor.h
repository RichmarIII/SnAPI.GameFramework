#pragma once

#include "Editor/EditorExport.h"
#include "Editor/IEditorService.h"
#include "GameRuntime.h"

#include <memory>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace SnAPI::GameFramework::Editor
{

/**
 * @brief High-level bootstrap settings for the editor runtime host.
 */
struct GameEditorSettings
{
    GameRuntimeSettings Runtime{}; /**< @brief Runtime settings used to initialize the editor world. */
};

/**
 * @brief Minimal editor runtime facade over `GameRuntime`.
 * @remarks
 * This class provides a stable entry point for the future editor target while
 * reusing GameFramework runtime/bootstrap behavior.
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API GameEditor final : public IEditorServiceHost
{
public:
    /**
     * @brief Initialize editor runtime.
     * @param Settings Editor settings.
     * @return Success or error.
     */
    Result Initialize(const GameEditorSettings& Settings);

    /**
     * @brief Shutdown editor runtime.
     */
    void Shutdown();

    /**
     * @brief Check whether editor runtime is initialized.
     */
    [[nodiscard]] bool IsInitialized() const;

    /**
     * @brief Update one frame.
     * @param DeltaSeconds Frame delta time in seconds.
     * @return `true` to continue running; `false` when runtime requests exit.
     */
    bool Update(float DeltaSeconds);

    /**
     * @brief Mutable access to wrapped `GameRuntime`.
     */
    [[nodiscard]] GameRuntime& Runtime();

    /**
     * @brief Const access to wrapped `GameRuntime`.
     */
    [[nodiscard]] const GameRuntime& Runtime() const;

    /**
     * @brief Last applied editor settings.
     */
    [[nodiscard]] const GameEditorSettings& Settings() const;

    /**
     * @brief Register a concrete editor service type.
     * @remarks
     * Registration is idempotent by type; if already registered, returns the existing service.
     */
    template<typename TService, typename... TArgs>
    TService& RegisterService(TArgs&&... Args);

    /**
     * @brief Register a runtime-provided service instance.
     * @remarks
     * Registration is idempotent by concrete dynamic type. When editor runtime is
     * already initialized, newly registered services are initialized immediately
     * (dependency order is recomputed first).
     */
    Result RegisterService(std::unique_ptr<IEditorService> Service);

    /**
     * @brief Unregister a registered service type.
     * @remarks
     * Removes the target service and any transitive dependents safely.
     */
    Result UnregisterService(const std::type_index& ServiceType);

    /**
     * @brief Unregister a registered service type.
     */
    template<typename TService>
    Result UnregisterService();

    /**
     * @brief Query a registered service by type.
     */
    template<typename TService>
    [[nodiscard]] TService* GetService();

    /**
     * @brief Query a registered service by type (const).
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
