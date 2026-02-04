#pragma once

#include <utility>

#include "HandleFwd.h"
#include "ObjectRegistry.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

template<typename T>
struct THandle
{
    THandle() = default;

    explicit THandle(Uuid InId)
        : Id(std::move(InId))
    {
    }

    Uuid Id{};

    bool IsNull() const noexcept
    {
        return Id.is_nil();
    }

    explicit operator bool() const noexcept
    {
        return !IsNull();
    }

    bool operator==(const THandle& Other) const noexcept
    {
        return Id == Other.Id;
    }

    bool operator!=(const THandle& Other) const noexcept
    {
        return !(*this == Other);
    }

    // Borrowed pointers are valid only for the current frame; do not cache or store them.
    T* Borrowed() const
    {
        return ObjectRegistry::Instance().Resolve<T>(Id);
    }

    // Borrowed pointers are valid only for the current frame; do not cache or store them.
    T* Borrowed()
    {
        return ObjectRegistry::Instance().Resolve<T>(Id);
    }

    bool IsValid() const
    {
        return ObjectRegistry::Instance().IsValid<T>(Id);
    }
};

struct HandleHash
{
    template<typename T>
    std::size_t operator()(const THandle<T>& Handle) const noexcept
    {
        return UuidHash{}(Handle.Id);
    }
};

} // namespace SnAPI::GameFramework
