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
 * @note Type identity is tracked by deterministic reflected `TypeId`.
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
        Result.m_type = VoidTypeId();
        Result.m_isRef = false;
        Result.m_isConst = false;
        return Result;
    }

    /**
     * @brief Create a variant that owns a value.
     * @tparam T Value type.
     * @param Value Value to store (moved or copied).
     * @return Variant owning the value.
     * @remarks Stores value on heap via shared ownership to preserve copyable variant semantics.
     */
    template<typename T>
    static Variant FromValue(T Value)
    {
        using Decayed = std::decay_t<T>;
        Variant Result;
        Result.m_type = CachedTypeId<Decayed>();
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
     * @note Caller must guarantee lifetime; no ownership is transferred.
     */
    template<typename T>
    static Variant FromRef(T& Value)
    {
        Variant Result;
        Result.m_type = CachedTypeId<std::decay_t<T>>();
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
     * @note Caller must guarantee lifetime; mutable extraction will fail by design.
     */
    template<typename T>
    static Variant FromConstRef(const T& Value)
    {
        Variant Result;
        Result.m_type = CachedTypeId<std::decay_t<T>>();
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
        return m_type == VoidTypeId();
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
     * @remarks Low-level escape hatch for performance-critical internals; caller is responsible for type safety.
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
        return m_type == CachedTypeId<std::decay_t<T>>();
    }

    /**
     * @brief Get a mutable reference to the stored value.
     * @tparam T Expected type.
     * @return Reference wrapper on success; error otherwise.
     * @remarks Fails on type mismatch or when backing storage is const-referenced.
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
    static const TypeId& VoidTypeId()
    {
        static const TypeId Type = TypeIdFromName("void");
        return Type;
    }

    template<typename T>
    static const TypeId& CachedTypeId()
    {
        static const TypeId Type = TypeIdFromName(TTypeNameV<T>);
        return Type;
    }

    TypeId m_type{}; /**< @brief Reflected type id of stored payload. */
    std::shared_ptr<void> m_storage{}; /**< @brief Owned object storage or non-owning reference wrapper pointer. */
    bool m_isRef = false; /**< @brief Reference mode flag (`true` for non-owning reference payload). */
    bool m_isConst = false; /**< @brief Const-reference qualifier for reference mode payloads. */
};

/**
 * @brief Non-owning view into a Variant-like value.
 * @remarks
 * Lightweight read/write view used to avoid allocating/copying `Variant` in hot paths
 * (serialization/replication field traversal).
 */
class VariantView
{
public:
    /** @brief Construct an empty invalid view. */
    VariantView() = default;
    /**
     * @brief Construct explicit typed view.
     * @param Type Reflected payload type id.
     * @param Ptr Raw payload pointer.
     * @param IsConst Whether mutable access is disallowed.
     */
    VariantView(TypeId Type, const void* Ptr, bool IsConst)
        : m_type(std::move(Type))
        , m_ptr(Ptr)
        , m_isConst(IsConst)
    {
    }

    /** @brief Get reflected payload type id for this view. */
    const TypeId& Type() const
    {
        return m_type;
    }

    /** @brief Check if mutable access is disallowed. */
    bool IsConst() const
    {
        return m_isConst;
    }

    /** @brief Borrow const payload pointer. */
    const void* Borrowed() const
    {
        return m_ptr;
    }

    /**
     * @brief Borrow mutable payload pointer.
     * @return Mutable pointer when view is non-const, otherwise nullptr.
     */
    void* BorrowedMutable()
    {
        return m_isConst ? nullptr : const_cast<void*>(m_ptr);
    }

private:
    TypeId m_type{}; /**< @brief Reflected payload type id. */
    const void* m_ptr = nullptr; /**< @brief Non-owning payload pointer. */
    bool m_isConst = true; /**< @brief Constness gate for mutable borrowing. */
};

} // namespace SnAPI::GameFramework
