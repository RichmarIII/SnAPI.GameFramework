#pragma once

#include <memory>
#include <type_traits>
#include <utility>

#include "BuiltinTypes.h"
#include "Expected.h"
#include "TypeName.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

class Variant
{
public:
    Variant() = default;

    static Variant Void()
    {
        Variant Result;
        Result.m_type = TypeIdFromName("void");
        Result.m_isRef = false;
        Result.m_isConst = false;
        return Result;
    }

    template<typename T>
    static Variant FromValue(T Value)
    {
        using Decayed = std::decay_t<T>;
        Variant Result;
        Result.m_type = TypeIdFromName(TTypeNameV<Decayed>);
        Result.m_storage = std::make_shared<Decayed>(std::move(Value));
        Result.m_isRef = false;
        Result.m_isConst = false;
        return Result;
    }

    template<typename T>
    static Variant FromRef(T& Value)
    {
        Variant Result;
        Result.m_type = TypeIdFromName(TTypeNameV<std::decay_t<T>>);
        Result.m_storage = std::shared_ptr<void>(&Value, [](void*) {});
        Result.m_isRef = true;
        Result.m_isConst = false;
        return Result;
    }

    template<typename T>
    static Variant FromConstRef(const T& Value)
    {
        Variant Result;
        Result.m_type = TypeIdFromName(TTypeNameV<std::decay_t<T>>);
        Result.m_storage = std::shared_ptr<void>(const_cast<T*>(&Value), [](void*) {});
        Result.m_isRef = true;
        Result.m_isConst = true;
        return Result;
    }

    const TypeId& Type() const
    {
        return m_type;
    }

    bool IsVoid() const
    {
        return m_type == TypeIdFromName("void");
    }

    bool IsRef() const
    {
        return m_isRef;
    }

    bool IsConst() const
    {
        return m_isConst;
    }

    void* Borrowed()
    {
        return m_storage.get();
    }

    const void* Borrowed() const
    {
        return m_storage.get();
    }

    template<typename T>
    bool Is() const
    {
        return m_type == TypeIdFromName(TTypeNameV<std::decay_t<T>>);
    }

    template<typename T>
    TExpected<std::reference_wrapper<T>> AsRef()
    {
        using Decayed = std::decay_t<T>;
        if (!Is<Decayed>())
        {
            return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Variant type mismatch"));
        }
        if (m_isRef && m_isConst)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Variant holds const ref"));
        }
        if (!m_storage)
        {
            return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Variant value missing"));
        }
        return std::ref(*static_cast<Decayed*>(m_storage.get()));
    }

    template<typename T>
    TExpected<std::reference_wrapper<const T>> AsConstRef() const
    {
        using Decayed = std::decay_t<T>;
        if (!Is<Decayed>())
        {
            return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Variant type mismatch"));
        }
        if (!m_storage)
        {
            return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Variant value missing"));
        }
        return std::cref(*static_cast<const Decayed*>(m_storage.get()));
    }

private:
    TypeId m_type{};
    std::shared_ptr<void> m_storage{};
    bool m_isRef = false;
    bool m_isConst = false;
};

} // namespace SnAPI::GameFramework
