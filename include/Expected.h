#pragma once

#include <expected>
#include <functional>
#include <string>
#include <utility>

namespace SnAPI::GameFramework
{

/**
 * @brief Canonical error codes used by the framework.
 * @remarks These codes accompany Error to categorize failures.
 * @note EErrorCode::None indicates success.
 */
enum class EErrorCode
{
    None = 0,        /**< @brief No error. */
    NotFound,        /**< @brief Requested item was not found. */
    InvalidArgument, /**< @brief One or more arguments are invalid. */
    TypeMismatch,    /**< @brief Type mismatch or unsafe conversion. */
    OutOfRange,      /**< @brief Index or value is out of range. */
    AlreadyExists,   /**< @brief Attempted to create an object that already exists. */
    NotReady,        /**< @brief Subsystem or object is not ready. */
    InternalError    /**< @brief Unexpected internal failure. */
};

/**
 * @brief Error payload for TExpected results.
 * @remarks Use Code for programmatic checks and Message for human-readable details.
 * @note A default-constructed Error represents success (EErrorCode::None).
 */
struct Error
{
    EErrorCode Code = EErrorCode::None; /**< @brief Error category. */
    std::string Message; /**< @brief Human-readable diagnostic message. */

    /**
     * @brief Construct a success error value.
     * @remarks Code defaults to EErrorCode::None.
     * @note A success Error evaluates to false.
     */
    Error() = default;
    /**
     * @brief Construct an error with code and message.
     * @param InCode Error category.
     * @param InMessage Diagnostic message.
     * @remarks Prefer MakeError for convenience.
     */
    Error(EErrorCode InCode, std::string InMessage)
        : Code(InCode)
        , Message(std::move(InMessage))
    {
    }

    /**
     * @brief Boolean conversion for quick success checks.
     * @remarks Returns true when Code is not EErrorCode::None.
     * @note This intentionally inverts the typical "success" meaning.
     */
    explicit operator bool() const noexcept
    {
        return Code != EErrorCode::None;
    }
};

/**
 * @brief Convenience alias for std::expected with framework Error.
 * @remarks Use TExpected<T> for functions that may fail.
 */
template<typename T>
using TExpected = std::expected<T, Error>;

/**
 * @brief Lightweight expected wrapper that stores a reference.
 * @remarks Use for APIs that return non-owning references with error handling.
 * @note The referenced object must outlive this wrapper.
 */
template<typename T>
class TExpectedRef
{
public:
    /**
     * @brief Construct an empty (invalid) expected reference.
     * @remarks Useful as a default value before assignment.
     */
    TExpectedRef() = default;

    /**
     * @brief Construct a success result from a reference.
     * @param Value Referenced object.
     * @remarks Stores a reference wrapper internally.
     */
    TExpectedRef(T& Value)
        : m_expected(std::ref(Value))
    {
    }

    /**
     * @brief Construct a success result from a reference wrapper.
     * @param Value Reference wrapper.
     */
    TExpectedRef(std::reference_wrapper<T> Value)
        : m_expected(std::move(Value))
    {
    }

    /**
     * @brief Construct a failure result.
     * @param ErrorValue Error payload.
     * @remarks Use std::unexpected(MakeError(...)).
     */
    TExpectedRef(std::unexpected<Error> ErrorValue)
        : m_expected(std::unexpected(ErrorValue.error()))
    {
    }

    /**
     * @brief Boolean conversion for success checks.
     * @return True when a valid reference is present.
     * @remarks Mirrors std::expected semantics.
     */
    explicit operator bool() const
    {
        return m_expected.has_value();
    }

    /**
     * @brief Dereference to the underlying object.
     * @return Reference to the contained object.
     * @note Behavior is undefined if this is in an error state.
     */
    T& operator*()
    {
        return m_expected->get();
    }

    /**
     * @brief Const dereference to the underlying object.
     * @return Const reference to the contained object.
     * @note Behavior is undefined if this is in an error state.
     */
    const T& operator*() const
    {
        return m_expected->get();
    }

    /**
     * @brief Arrow operator access.
     * @return Pointer to the contained object.
     * @note Behavior is undefined if this is in an error state.
     */
    T* operator->()
    {
        return &m_expected->get();
    }

    /**
     * @brief Const arrow operator access.
     * @return Pointer to the contained object.
     * @note Behavior is undefined if this is in an error state.
     */
    const T* operator->() const
    {
        return &m_expected->get();
    }

    /**
     * @brief Get the contained reference.
     * @return Reference to the object.
     * @note Throws if no value is present.
     */
    T& Get()
    {
        return m_expected->get();
    }

    /**
     * @brief Get the contained reference (const).
     * @return Const reference to the object.
     * @note Throws if no value is present.
     */
    const T& Get() const
    {
        return m_expected->get();
    }

    /**
     * @brief Access the error payload.
     * @return Mutable reference to the Error.
     * @remarks Valid only when in an error state.
     */
    Error& error()
    {
        return m_expected.error();
    }

    /**
     * @brief Access the error payload (const).
     * @return Const reference to the Error.
     * @remarks Valid only when in an error state.
     */
    const Error& error() const
    {
        return m_expected.error();
    }

    /**
     * @brief Access the value, throwing on error.
     * @return Reference to the object.
     * @remarks Mirrors std::expected::value().
     */
    T& value()
    {
        return m_expected.value().get();
    }

    /**
     * @brief Access the value (const), throwing on error.
     * @return Const reference to the object.
     * @remarks Mirrors std::expected::value().
     */
    const T& value() const
    {
        return m_expected.value().get();
    }

    /**
     * @brief Access the underlying std::expected.
     * @return Mutable reference to the internal std::expected wrapper.
     * @remarks Useful for interoperability with std::expected APIs.
     */
    std::expected<std::reference_wrapper<T>, Error>& Raw()
    {
        return m_expected;
    }

    /**
     * @brief Access the underlying std::expected (const).
     * @return Const reference to the internal std::expected wrapper.
     */
    const std::expected<std::reference_wrapper<T>, Error>& Raw() const
    {
        return m_expected;
    }

private:
    std::expected<std::reference_wrapper<T>, Error> m_expected{}; /**< @brief Stored expected reference. */
};

/**
 * @brief Convenience alias for operations returning only success/failure.
 * @remarks Equivalent to TExpected<void>.
 */
using Result = TExpected<void>;

/**
 * @brief Construct a success Result.
 * @return Result with no error.
 * @note Use for functions returning Result.
 */
inline Result Ok()
{
    return {};
}

/**
 * @brief Construct an Error value.
 * @param Code Error category.
 * @param Message Diagnostic message.
 * @return Error instance with the provided data.
 * @remarks Prefer this over manually constructing Error for clarity.
 */
inline Error MakeError(EErrorCode Code, std::string Message)
{
    return Error(Code, std::move(Message));
}

} // namespace SnAPI::GameFramework
