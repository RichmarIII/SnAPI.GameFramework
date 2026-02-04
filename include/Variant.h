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

/**
 * @brief Type-erased value container used by reflection and scripting.
 * @remarks Stores either an owned value or a reference with constness tracking.
 * @note Type identity is tracked via TypeIdFromName.
 */
class Variant
{
public:
    /**
     * @brief Construct an empty (void) variant.
     */
    Variant() = default;

    /**
     * @brief Create a void variant.
     * @return Variant representing void.
     * @remarks Useful for void return types.
     */
    static Variant Void()
    {
        Variant Result;
        Result.m_type = TypeIdFromName("void");
        Result.m_isRef = false;
        Result.m_isConst = false;
        return Result;
    }

    /**
     * @brief Create a variant that owns a value.
     * @tparam T Value type.
     * @param Value Value to store (moved or copied).
     * @return Variant owning the value.
     * @remarks Stores value on the heap via shared_ptr.
     */
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

    /**
     * @brief Create a variant that references a mutable object.
     * @tparam T Referenced type.
     * @param Value Reference to the object.
     * @return Variant referencing the object.
     * @note Caller must ensure the referenced object outlives the Variant.
     */
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

    /**
     * @brief Create a variant that references a const object.
     * @tparam T Referenced type.
     * @param Value Const reference to the object.
     * @return Variant referencing the object as const.
     * @note Caller must ensure the referenced object outlives the Variant.
     */
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

    /**
     * @brief Get the stored type id.
     * @return TypeId for the stored value.
     */
    const TypeId& Type() const
    {
        return m_type;
    }

    /**
     * @brief Check whether this is a void variant.
     * @return True if the variant represents void.
     */
    bool IsVoid() const
    {
        return m_type == TypeIdFromName("void");
    }

    /**
     * @brief Check whether this variant stores a reference.
     * @return True if it stores a reference; false if it owns the value.
     */
    bool IsRef() const
    {
        return m_isRef;
    }

    /**
     * @brief Check whether a referenced value is const.
     * @return True if reference is const.
     */
    bool IsConst() const
    {
        return m_isConst;
    }

    /**
     * @brief Borrow the underlying pointer (mutable).
     * @return Pointer to stored value or reference.
     * @remarks Use with care; type safety is the caller's responsibility.
     */
    void* Borrowed()
    {
        return m_storage.get();
    }

    /**
     * @brief Borrow the underlying pointer (const).
     * @return Pointer to stored value or reference.
     */
    const void* Borrowed() const
    {
        return m_storage.get();
    }

    /**
     * @brief Type check helper.
     * @tparam T Expected type.
     * @return True if the stored type matches T.
     */
    template<typename T>
    bool Is() const
    {
        return m_type == TypeIdFromName(TTypeNameV<std::decay_t<T>>);
    }

    /**
     * @brief Get a mutable reference to the stored value.
     * @tparam T Expected type.
     * @return Reference wrapper on success; error otherwise.
     * @remarks Fails if the variant holds a const reference.
     */
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

    /**
     * @brief Get a const reference to the stored value.
     * @tparam T Expected type.
     * @return Const reference wrapper on success; error otherwise.
     */
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
    TypeId m_type{}; /**< @brief Type id of the stored value. */
    std::shared_ptr<void> m_storage{}; /**< @brief Owned value or referenced pointer. */
    bool m_isRef = false; /**< @brief True if the variant holds a reference. */
    bool m_isConst = false; /**< @brief True if the reference is const. */
};

} // namespace SnAPI::GameFramework
