#pragma once

#include <expected>
#include <functional>
#include <string>
#include <utility>

namespace SnAPI::GameFramework
{

enum class EErrorCode
{
    None = 0,
    NotFound,
    InvalidArgument,
    TypeMismatch,
    OutOfRange,
    AlreadyExists,
    NotReady,
    InternalError
};

struct Error
{
    EErrorCode Code = EErrorCode::None;
    std::string Message;

    Error() = default;
    Error(EErrorCode InCode, std::string InMessage)
        : Code(InCode)
        , Message(std::move(InMessage))
    {
    }

    explicit operator bool() const noexcept
    {
        return Code != EErrorCode::None;
    }
};

template<typename T>
using TExpected = std::expected<T, Error>;

template<typename T>
class TExpectedRef
{
public:
    TExpectedRef() = default;

    TExpectedRef(T& Value)
        : m_expected(std::ref(Value))
    {
    }

    TExpectedRef(std::reference_wrapper<T> Value)
        : m_expected(std::move(Value))
    {
    }

    TExpectedRef(std::unexpected<Error> ErrorValue)
        : m_expected(std::unexpected(ErrorValue.error()))
    {
    }

    explicit operator bool() const
    {
        return m_expected.has_value();
    }

    T& operator*()
    {
        return m_expected->get();
    }

    const T& operator*() const
    {
        return m_expected->get();
    }

    T* operator->()
    {
        return &m_expected->get();
    }

    const T* operator->() const
    {
        return &m_expected->get();
    }

    T& Get()
    {
        return m_expected->get();
    }

    const T& Get() const
    {
        return m_expected->get();
    }

    Error& error()
    {
        return m_expected.error();
    }

    const Error& error() const
    {
        return m_expected.error();
    }

    T& value()
    {
        return m_expected.value().get();
    }

    const T& value() const
    {
        return m_expected.value().get();
    }

    std::expected<std::reference_wrapper<T>, Error>& Raw()
    {
        return m_expected;
    }

    const std::expected<std::reference_wrapper<T>, Error>& Raw() const
    {
        return m_expected;
    }

private:
    std::expected<std::reference_wrapper<T>, Error> m_expected{};
};

using Result = TExpected<void>;

inline Result Ok()
{
    return {};
}

inline Error MakeError(EErrorCode Code, std::string Message)
{
    return Error(Code, std::move(Message));
}

} // namespace SnAPI::GameFramework
