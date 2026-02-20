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
 * @brief Lightweight service execution context passed to editor services.
 */
class EditorServiceContext final
{
public:
    explicit EditorServiceContext(IEditorServiceHost& Host)
        : m_host(&Host)
    {
    }

    [[nodiscard]] SnAPI::GameFramework::GameRuntime& Runtime();
    [[nodiscard]] const SnAPI::GameFramework::GameRuntime& Runtime() const;
    [[nodiscard]] IEditorServiceHost& Host() const { return *m_host; }

    template<typename TService>
    [[nodiscard]] TService* GetService();

    template<typename TService>
    [[nodiscard]] const TService* GetService() const;

private:
    IEditorServiceHost* m_host = nullptr;
};

/**
 * @brief Internal host contract consumed by `EditorServiceContext`.
 */
class IEditorServiceHost
{
public:
    virtual ~IEditorServiceHost() = default;
    [[nodiscard]] virtual SnAPI::GameFramework::GameRuntime& RuntimeForServices() = 0;
    [[nodiscard]] virtual const SnAPI::GameFramework::GameRuntime& RuntimeForServices() const = 0;
    [[nodiscard]] virtual IEditorService* ResolveServiceForContext(const std::type_index& Type) = 0;
    [[nodiscard]] virtual const IEditorService* ResolveServiceForContext(const std::type_index& Type) const = 0;
};

/**
 * @brief Contract for modular editor subsystems.
 * @remarks
 * Services are registered into `GameEditor`, initialized in dependency order,
 * ticked each frame, then shut down in reverse order.
 */
class IEditorService
{
public:
    virtual ~IEditorService() = default;

    /**
     * @brief Stable service name for diagnostics.
     */
    [[nodiscard]] virtual std::string_view Name() const = 0;

    /**
     * @brief Optional dependency list by concrete service type.
     */
    [[nodiscard]] virtual std::vector<std::type_index> Dependencies() const { return {}; }

    /**
     * @brief Optional ordering priority among dependency-ready services.
     * @remarks Lower values initialize earlier.
     */
    [[nodiscard]] virtual int Priority() const { return 0; }

    /**
     * @brief Initialize service state.
     */
    virtual Result Initialize(EditorServiceContext& Context) = 0;

    /**
     * @brief Per-frame update hook.
     */
    virtual void Tick(EditorServiceContext& Context, float DeltaSeconds)
    {
        (void)Context;
        (void)DeltaSeconds;
    }

    /**
     * @brief Shutdown and release service state.
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
