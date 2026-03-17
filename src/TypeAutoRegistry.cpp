#include "TypeAutoRegistry.h"
#include "GameThreading.h"

#include "Assert.h"

#include <optional>
#include <vector>

namespace SnAPI::GameFramework
{

namespace
{

#if defined(SNAPI_REFLECTION_LINK_ANCHOR_SYMBOL)
extern "C" void SNAPI_REFLECTION_LINK_ANCHOR_SYMBOL();
#endif

void EnsureGeneratedReflectionLinked()
{
#if defined(SNAPI_REFLECTION_LINK_ANCHOR_SYMBOL)
    static const bool Linked = [] {
        SNAPI_REFLECTION_LINK_ANCHOR_SYMBOL();
        return true;
    }();
    (void)Linked;
#endif
}

} // namespace

TypeAutoRegistry& TypeAutoRegistry::Instance()
{
    EnsureGeneratedReflectionLinked();
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
        // Generated reflection and manual registration can both contribute the same type.
        // Keep the first registration to avoid non-deterministic behavior.
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

Result TypeAutoRegistry::EnsureAll() const
{
    std::vector<TypeId> TypeIds{};
    {
        GameLockGuard Lock(m_mutex);
        TypeIds.reserve(m_entries.size());
        for (const auto& [Type, Fn] : m_entries)
        {
            if (Fn)
            {
                TypeIds.push_back(Type);
            }
        }
    }

    std::optional<Error> FirstError{};
    for (const TypeId& Type : TypeIds)
    {
        const Result EnsureResult = Ensure(Type);
        if (!EnsureResult && !FirstError.has_value())
        {
            FirstError = EnsureResult.error();
        }
    }

    if (FirstError.has_value())
    {
        return std::unexpected(*FirstError);
    }
    return Ok();
}

bool TypeAutoRegistry::Has(const TypeId& Id) const
{
    GameLockGuard Lock(m_mutex);
    return m_entries.find(Id) != m_entries.end();
}

} // namespace SnAPI::GameFramework
