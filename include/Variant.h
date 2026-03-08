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
 * @ingroup SnAPI_GameFramework
 * @brief Type-erased value container used by reflection, scripting, and generic invocation.
 *
 * `Variant` stores either:
 * - an owned heap-allocated value
 * - a borrowed mutable reference
 * - a borrowed const reference
 * - a distinguished `void` marker
 *
 * Core semantics:
 * - Type identity is tracked by deterministic reflected `TypeId`.
 * - Owned values use shared heap storage so `Variant` remains cheap to copy.
 * - Reference variants do not own the referenced object; callers must guarantee lifetime.
 * - Reference constness is enforced by `AsRef()`.
 *
 * Threading model:
 * - Copying and moving the `Variant` object is thread-safe in isolation.
 * - Access to referenced payloads follows the thread-safety rules of the referenced object.
 */
class Variant
{
public:
    /** @brief Construct an empty variant. The default state behaves like a void variant with no payload storage. */
    Variant() = default;

    /**
     * @brief Create an explicit void variant.
     * @return Variant representing void.
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
     *
     * The value is copied or moved into heap storage owned by the variant.
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
     *
     * @warning Borrowed reference valid only while `Value` remains alive.
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
     *
     * @warning Borrowed reference valid only while `Value` remains alive.
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
     * @brief Get the stored reflected type id.
     * @return Type id for the stored value or the void marker type.
     */
    const TypeId& Type() const
    {
        return m_type;
    }

    /**
     * @brief Check whether this variant represents `void`.
     * @return `true` when the stored type id is the void marker type.
     */
    bool IsVoid() const
    {
        return m_type == VoidTypeId();
    }

    /**
     * @brief Check whether this variant stores a borrowed reference.
     * @return `true` for borrowed reference payloads, `false` for owned values and void.
     */
    bool IsRef() const
    {
        return m_isRef;
    }

    /**
     * @brief Check whether the stored reference payload is const-qualified.
     * @return `true` only for const-reference payloads.
     */
    bool IsConst() const
    {
        return m_isConst;
    }

    /**
     * @brief Borrow the underlying payload pointer as mutable.
     * @return Raw payload pointer, or `nullptr` when no payload exists.
     *
     * @warning This bypasses type and constness checks. Callers are responsible for correctness.
     */
    void* Borrowed()
    {
        return m_storage.get();
    }

    /**
     * @brief Borrow the underlying payload pointer as const.
     * @return Raw payload pointer, or `nullptr` when no payload exists.
     */
    const void* Borrowed() const
    {
        return m_storage.get();
    }

    /**
     * @brief Check whether the stored payload type matches `T`.
     * @tparam T Expected type.
     * @return `true` when the stored reflected type id equals `T`.
     */
    template<typename T>
    bool Is() const
    {
        return m_type == CachedTypeId<std::decay_t<T>>();
    }

    /**
     * @brief Extract a mutable reference to the stored payload.
     * @tparam T Expected type.
     * @return Reference wrapper on success; error otherwise.
     *
     * Fails when:
     * - the stored type does not match `T`
     * - the payload is a const reference
     * - no payload storage exists
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
     * @brief Extract a const reference to the stored payload.
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
 * @ingroup SnAPI_GameFramework
 * @brief Non-owning typed view into external payload storage.
 *
 * `VariantView` is the zero-allocation counterpart to `Variant` used on hot reflective traversal
 * paths such as serialization and replication.
 *
 * Ownership and lifetime:
 * - `VariantView` never owns storage.
 * - The caller must guarantee that the referenced payload remains alive for the lifetime of the view.
 */
class VariantView
{
public:
    /** @brief Construct an empty invalid view. */
    VariantView() = default;
    /**
     * @brief Construct an explicit typed view.
     * @param Type Reflected payload type id.
     * @param Ptr Raw payload pointer.
     * @param IsConst Whether mutable borrowing is disallowed.
     */
    VariantView(TypeId Type, const void* Ptr, bool IsConst)
        : m_type(std::move(Type))
        , m_ptr(Ptr)
        , m_isConst(IsConst)
    {
    }

    /** @brief Get the reflected payload type id for this view. */
    const TypeId& Type() const
    {
        return m_type;
    }

    /** @brief Check whether mutable access is disallowed. */
    bool IsConst() const
    {
        return m_isConst;
    }

    /** @brief Borrow the payload pointer as const. */
    const void* Borrowed() const
    {
        return m_ptr;
    }

    /**
     * @brief Borrow the payload pointer as mutable.
     * @return Mutable pointer when the view is non-const, otherwise `nullptr`.
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
