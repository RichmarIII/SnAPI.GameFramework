#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <vector>

#include "Conduit/Types.h"
#include "StaticTypeId.h"

namespace SnAPI::GameFramework::Conduit
{

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Compiled storage layout for one Conduit execution frame.
 *
 * A frame layout is the owned-memory contract for a compiled graph instance.
 * It is built once and then reused by every `GraphInstance` constructed from the same
 * `CompiledGraph`.
 *
 * Responsibilities:
 * - assign dense slot ids
 * - compute aligned byte offsets
 * - record per-slot type/size/alignment metadata
 * - expose the total allocation size required for one frame
 *
 * Design intent:
 * - all runtime slot storage is contiguous
 * - layout computation happens at bind/build time, not during hot execution
 * - slots are strongly typed through reflected `TypeInfo`
 */
struct FrameLayout
{
    std::vector<SlotDesc> Slots; /**< @brief Ordered slot table for this layout. */
    std::uint32_t TotalSize = 0; /**< @brief Total frame byte size after alignment padding. */
    std::size_t MaxAlign = alignof(std::max_align_t); /**< @brief Maximum slot alignment needed by the frame. */

    /**
     * @brief Find one slot by id.
     * @param Id Slot identifier.
     * @return Pointer to the slot description, or null when the id is invalid.
     */
    [[nodiscard]] const SlotDesc* FindSlot(SlotId Id) const;

    /**
     * @brief Add one slot to the layout.
     * @param Type Reflected slot type.
     * @param Kind Value vs handle interpretation.
     * @return Newly assigned slot id or an error when the type cannot be stored.
     *
     * Requirements:
     * - `Type.RuntimeOps` must be populated
     * - `Type.Size` and `Type.Align` must be concrete non-zero values
     */
    TExpected<SlotId> AddSlot(const TypeInfo& Type, ESlotKind Kind = ESlotKind::Value);
};

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Owned raw storage backing one live execution frame.
 *
 * `FrameStorage` is the actual memory buffer used by a `GraphInstance`.
 * It owns one aligned allocation and uses `TypeRuntimeOps` to manage object lifetime
 * in each slot.
 *
 * Important semantics:
 * - each slot is independently tracked as initialized/uninitialized
 * - reading an uninitialized slot is an error
 * - writing a slot resets and destroys any previous live value first
 * - storage is frame-owned, not externally borrowed
 *
 * This is the core structure that keeps Conduit fast:
 * - no per-node heap allocation during normal execution
 * - raw slot access is offset-based
 * - type lifecycle is routed through reflected runtime ops
 */
class FrameStorage
{
public:
    /**
     * @brief Allocate storage for one frame layout.
     * @param Layout Compiled layout to instantiate.
     */
    explicit FrameStorage(const FrameLayout& Layout);
    /**
     * @brief Destroy all initialized slot values and free frame memory.
     */
    ~FrameStorage();

    FrameStorage(const FrameStorage&) = delete;
    FrameStorage& operator=(const FrameStorage&) = delete;
    FrameStorage(FrameStorage&&) = delete;
    FrameStorage& operator=(FrameStorage&&) = delete;

    /**
     * @brief Access the owning layout.
     * @return Layout reference.
     */
    [[nodiscard]] const FrameLayout& Layout() const
    {
        return *m_layout;
    }

    /**
     * @brief Check whether a slot currently holds a live value.
     * @param Id Slot id.
     * @return True when initialized.
     */
    [[nodiscard]] bool IsInitialized(SlotId Id) const;

    /**
     * @brief Destroy the current value in a slot, if present.
     * @param Id Slot id.
     * @return Success or error.
     */
    Result ResetSlot(SlotId Id);

    /**
     * @brief Copy-construct a new value into a slot.
     * @param Id Destination slot.
     * @param Source Pointer to a source object of the slot's reflected type.
     * @return Success or error.
     *
     * This is the standard way most nodes write value outputs.
     */
    Result StoreCopy(SlotId Id, const void* Source);

    /**
     * @brief Default-construct a slot in place.
     * @param Id Destination slot.
     * @return Success or error.
     */
    Result DefaultConstructSlot(SlotId Id);

    /**
     * @brief Read a slot as a raw const pointer.
     * @param Id Slot id.
     * @return Pointer to the live stored value or an error.
     */
    TExpected<const void*> ReadSlot(SlotId Id) const;

    /**
     * @brief Borrow a mutable raw pointer to an initialized slot.
     * @param Id Slot id.
     * @return Mutable pointer or an error.
     *
     * Use this when a bound method needs a raw argument pointer without rebuilding
     * a wrapper object.
     */
    TExpected<void*> BorrowMutableSlot(SlotId Id);

    /**
     * @brief Reset a slot and return raw output storage for in-place construction/writing.
     * @param Id Slot id.
     * @return Writable raw storage pointer or an error.
     *
     * Callers must later mark the slot initialized if they successfully produce a value.
     */
    TExpected<void*> PrepareOutputSlot(SlotId Id);

    /**
     * @brief Mark a slot as initialized after in-place output construction.
     * @param Id Slot id.
     * @return Success or error.
     */
    Result MarkInitialized(SlotId Id);

    /**
     * @brief Read one slot as a typed const reference.
     * @tparam T Expected slot type.
     * @param Id Slot id.
     * @return Borrowed const reference wrapper or an error.
     *
     * This is mainly intended for tests, debugging, and external inspection code.
     * Hot execution paths should usually work with raw slot pointers instead.
     */
    template<typename T>
    TExpected<std::reference_wrapper<const T>> AsConstRef(SlotId Id) const
    {
        const SlotDesc* Slot = m_layout->FindSlot(Id);
        if (!Slot || !Slot->Type || Slot->Type->Id != StaticTypeId<std::remove_cvref_t<T>>())
        {
            return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Conduit slot type mismatch"));
        }
        auto ReadResult = ReadSlot(Id);
        if (!ReadResult)
        {
            return std::unexpected(ReadResult.error());
        }
        return std::cref(*static_cast<const std::remove_cvref_t<T>*>(ReadResult.value()));
    }

private:
    /**
     * @brief Internal slot lookup helper.
     * @param Id Slot id.
     * @return Slot description or null.
     */
    [[nodiscard]] const SlotDesc* RequireSlot(SlotId Id) const;
    /**
     * @brief Compute mutable raw storage pointer for one slot.
     * @param Slot Slot metadata.
     * @return Pointer into the frame allocation.
     */
    [[nodiscard]] void* RawSlotStorage(const SlotDesc& Slot);
    /**
     * @brief Compute const raw storage pointer for one slot.
     * @param Slot Slot metadata.
     * @return Const pointer into the frame allocation.
     */
    [[nodiscard]] const void* RawSlotStorage(const SlotDesc& Slot) const;

    const FrameLayout* m_layout = nullptr; /**< @brief Non-owning pointer to the compiled layout contract. */
    void* m_storage = nullptr; /**< @brief Owned aligned raw allocation for all slots. */
    std::vector<std::uint8_t> m_initialized; /**< @brief Per-slot initialization flags. */
};

} // namespace SnAPI::GameFramework::Conduit
