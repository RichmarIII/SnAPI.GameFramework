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
    /** @brief Concrete storage mode for the current payload. */
    enum class EStorageKind
    {
        Void,
        Owned,
        BorrowedMutable,
        BorrowedConst
    };

    /** @brief Construct an empty variant. The default state behaves like a void variant with no payload storage. */
    Variant()
        : m_type(VoidTypeId()) {
    }

    /**
     * @brief Create an explicit void variant.
     * @return Variant representing void.
     */
    static Variant Void()
    {
        return {};
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
    static Variant FromValue(T&& Value)
    {
        using Decayed = std::decay_t<T>;
        Variant Result;
        Result.m_type = CachedTypeId<Decayed>();
        Result.m_storage = std::make_shared<Decayed>(std::forward<T>(Value));
        Result.m_storageKind = EStorageKind::Owned;
        Result.m_cloneOwnedStorage = CloneOwnedStorageFnFor<Decayed>();
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
        Result.m_storageKind = EStorageKind::BorrowedMutable;
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
        Result.m_storageKind = EStorageKind::BorrowedConst;
        return Result;
    }

    /**
     * @brief Get the stored reflected type id.
     * @return Type id for the stored value or the void marker type.
     */
    [[nodiscard]] const TypeId& Type() const
    {
        return m_type;
    }

    /**
     * @brief Check whether this variant represents `void`.
     * @return `true` when the stored type id is the void marker type.
     */
    [[nodiscard]] bool IsVoid() const
    {
        return m_storageKind == EStorageKind::Void;
    }

    /**
     * @brief Get the current storage mode.
     * @return Storage classification for the payload.
     */
    [[nodiscard]] EStorageKind StorageKind() const
    {
        return m_storageKind;
    }

    /**
     * @brief Check whether this variant stores a borrowed reference.
     * @return `true` for borrowed reference payloads, `false` for owned values and void.
     */
    [[nodiscard]] bool IsBorrowed() const
    {
        return m_storageKind == EStorageKind::BorrowedMutable
            || m_storageKind == EStorageKind::BorrowedConst;
    }

    /**
     * @brief Check whether the stored borrowed payload is const-qualified.
     * @return `true` only for const-reference payloads.
     */
    [[nodiscard]] bool IsConstBorrowed() const
    {
        return m_storageKind == EStorageKind::BorrowedConst;
    }

    /**
     * @brief Borrow the underlying payload pointer as mutable.
     * @return Raw payload pointer, or `nullptr` when no payload exists.
     *
     * @warning This bypasses type and constness checks. Callers are responsible for correctness.
     */
    void* UnsafeBorrowedMutable()
    {
        if (m_storageKind == EStorageKind::Void || m_storageKind == EStorageKind::BorrowedConst)
        {
            return nullptr;
        }
        if (const auto DetachResult = EnsureUniqueOwnedStorage(); !DetachResult)
        {
            return nullptr;
        }
        return m_storage.get();
    }

    /**
     * @brief Borrow the underlying payload pointer as const.
     * @return Raw payload pointer, or `nullptr` when no payload exists.
     */
    [[nodiscard]] const void* UnsafeBorrowed() const
    {
        return m_storage.get();
    }

    /**
     * @brief Check whether the stored payload type matches `T`.
     * @tparam T Expected type.
     * @return `true` when the stored reflected type id equals `T`.
     */
    template<typename T>
    [[nodiscard]] bool Is() const
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
        if (m_storageKind == EStorageKind::BorrowedConst)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Variant holds const ref"));
        }
        if (!m_storage)
        {
            return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Variant value missing"));
        }
        if (auto DetachResult = EnsureUniqueOwnedStorage(); !DetachResult)
        {
            return std::unexpected(DetachResult.error());
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
    using CloneOwnedStorageFn = std::shared_ptr<void> (*)(const std::shared_ptr<void>&);

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

    template<typename T>
    static std::shared_ptr<void> CloneOwnedStorage(const std::shared_ptr<void>& Storage)
    {
        static_assert(std::is_copy_constructible_v<T>, "Owned variant cloning requires a copyable payload");
        if (!Storage)
        {
            return {};
        }
        return std::make_shared<T>(*static_cast<const T*>(Storage.get()));
    }

    template<typename T>
    static CloneOwnedStorageFn CloneOwnedStorageFnFor()
    {
        if constexpr (std::is_copy_constructible_v<T>)
        {
            return &CloneOwnedStorage<T>;
        }
        else
        {
            return nullptr;
        }
    }

    Result EnsureUniqueOwnedStorage()
    {
        if (m_storageKind != EStorageKind::Owned || !m_storage || m_storage.use_count() == 1)
        {
            return Ok();
        }
        if (!m_cloneOwnedStorage)
        {
            return std::unexpected(
                MakeError(EErrorCode::InvalidArgument,
                          "Variant owns a non-copyable value and cannot detach shared storage"));
        }

        auto UniqueStorage = m_cloneOwnedStorage(m_storage);
        if (!UniqueStorage)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, "Variant storage detach failed"));
        }

        m_storage = std::move(UniqueStorage);
        return Ok();
    }

    TypeId m_type; /**< @brief Reflected type id of stored payload. */
    std::shared_ptr<void> m_storage{}; /**< @brief Owned payload storage or non-owning reference wrapper pointer. */
    EStorageKind m_storageKind = EStorageKind::Void; /**< @brief Active storage classification for the payload. */
    CloneOwnedStorageFn m_cloneOwnedStorage = nullptr; /**< @brief Optional detach callback used when owned storage is shared. */
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
    VariantView(const TypeId Type, const void* Ptr, const bool IsConst)
        : m_type(Type)
        , m_ptr(Ptr)
        , m_isConst(IsConst)
    {
    }

    /** @brief Get the reflected payload type id for this view. */
    [[nodiscard]] const TypeId& Type() const
    {
        return m_type;
    }

    /** @brief Check whether mutable access is disallowed. */
    [[nodiscard]] bool IsConst() const
    {
        return m_isConst;
    }

    /** @brief Borrow the payload pointer as const. */
    [[nodiscard]] const void* UnsafeBorrowed() const
    {
        return m_ptr;
    }

    /**
     * @brief Borrow the payload pointer as mutable.
     * @return Mutable pointer when the view is non-const, otherwise `nullptr`.
     */
    [[nodiscard]] void* UnsafeBorrowedMutable() const {
        return m_isConst ? nullptr : const_cast<void*>(m_ptr);
    }

private:
    TypeId m_type{}; /**< @brief Reflected payload type id. */
    const void* m_ptr = nullptr; /**< @brief Non-owning payload pointer. */
    bool m_isConst = true; /**< @brief Constness gate for mutable borrowing. */
};

} // namespace SnAPI::GameFramework
