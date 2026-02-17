#include "TypeAutoRegistry.h"
#include "GameThreading.h"

#include "Assert.h"

namespace SnAPI::GameFramework
{

TypeAutoRegistry& TypeAutoRegistry::Instance()
{
    static TypeAutoRegistry Instance;
    return Instance;
}

void TypeAutoRegistry::Register(const TypeId& Id, std::string_view Name, EnsureFn Fn)
{
    if (!Fn)
    {
        return;
    }

    GameLockGuard Lock(m_mutex);
    auto It = m_entries.find(Id);
    if (It != m_entries.end())
    {
        // If this happens, it usually means the same type was registered in more than one TU.
        // Keep the first registration to avoid non-deterministic behavior.
        DEBUG_ASSERT(It->second == Fn, "Duplicate TypeAutoRegistry registration for type: {}", std::string(Name));
        return;
    }
    m_entries.emplace(Id, Fn);
    m_names.emplace(Id, std::string(Name));
}

Result TypeAutoRegistry::Ensure(const TypeId& Id) const
{
    EnsureFn Fn = nullptr;
    {
        GameLockGuard Lock(m_mutex);
        auto It = m_entries.find(Id);
        if (It == m_entries.end() || !It->second)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "No auto-registration entry for type id"));
        }
        Fn = It->second;
    }

    return Fn ? Fn() : std::unexpected(MakeError(EErrorCode::NotFound, "No auto-registration function"));
}

bool TypeAutoRegistry::Has(const TypeId& Id) const
{
    GameLockGuard Lock(m_mutex);
    return m_entries.find(Id) != m_entries.end();
}

} // namespace SnAPI::GameFramework

