#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Expected.h"
#include "Math.h"
#include "ObjectRegistry.h"
#include "StaticTypeId.h"
#include "TypeName.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

class IWorld;

/**
 * @brief Runtime object contract marker for the ECS refactor path.
 * @remarks
 * Runtime node/component types are required to be non-polymorphic so hot-path
 * updates stay free of vtable dispatch.
 */
template<typename T>
concept NonPolymorphicRuntimeType =
    std::is_class_v<T> &&
    !std::is_polymorphic_v<T> &&
    requires {
        { TTypeNameV<T> } -> std::convertible_to<const char*>;
    };

/**
 * @brief CRTP helper that forwards phase hooks to optional `*Impl` methods.
 * @tparam TDerived Concrete runtime object type.
 * @remarks
 * This provides the required runtime interface without virtual methods.
 */
template<typename TDerived>
struct TRuntimeTickCRTP
{
    void OnCreate(IWorld& WorldRef)
    {
        if constexpr (requires(TDerived& Obj, IWorld& WorldValue) {
                          { Obj.OnCreateImpl(WorldValue) } -> std::same_as<void>;
                      })
        {
            static_cast<TDerived&>(*this).OnCreateImpl(WorldRef);
        }
    }

    void OnDestroy(IWorld& WorldRef)
    {
        if constexpr (requires(TDerived& Obj, IWorld& WorldValue) {
                          { Obj.OnDestroyImpl(WorldValue) } -> std::same_as<void>;
                      })
        {
            static_cast<TDerived&>(*this).OnDestroyImpl(WorldRef);
        }
    }

    void PreTick(IWorld& WorldRef, float DeltaSeconds)
    {
        if constexpr (requires(TDerived& Obj, IWorld& WorldValue, float Dt) {
                          { Obj.PreTickImpl(WorldValue, Dt) } -> std::same_as<void>;
                      })
        {
            static_cast<TDerived&>(*this).PreTickImpl(WorldRef, DeltaSeconds);
        }
    }

    void Tick(IWorld& WorldRef, float DeltaSeconds)
    {
        if constexpr (requires(TDerived& Obj, IWorld& WorldValue, float Dt) {
                          { Obj.TickImpl(WorldValue, Dt) } -> std::same_as<void>;
                      })
        {
            static_cast<TDerived&>(*this).TickImpl(WorldRef, DeltaSeconds);
        }
    }

    void FixedTick(IWorld& WorldRef, float DeltaSeconds)
    {
        if constexpr (requires(TDerived& Obj, IWorld& WorldValue, float Dt) {
                          { Obj.FixedTickImpl(WorldValue, Dt) } -> std::same_as<void>;
                      })
        {
            static_cast<TDerived&>(*this).FixedTickImpl(WorldRef, DeltaSeconds);
        }
    }

    void LateTick(IWorld& WorldRef, float DeltaSeconds)
    {
        if constexpr (requires(TDerived& Obj, IWorld& WorldValue, float Dt) {
                          { Obj.LateTickImpl(WorldValue, Dt) } -> std::same_as<void>;
                      })
        {
            static_cast<TDerived&>(*this).LateTickImpl(WorldRef, DeltaSeconds);
        }
    }

    void PostTick(IWorld& WorldRef, float DeltaSeconds)
    {
        if constexpr (requires(TDerived& Obj, IWorld& WorldValue, float Dt) {
                          { Obj.PostTickImpl(WorldValue, Dt) } -> std::same_as<void>;
                      })
        {
            static_cast<TDerived&>(*this).PostTickImpl(WorldRef, DeltaSeconds);
        }
    }
};

template<typename T>
inline constexpr bool kUsesRuntimeTickCRTP = std::is_base_of_v<TRuntimeTickCRTP<T>, T>;

template<typename TDerived>
struct NodeCRTP
{
};

template<typename TDerived>
struct ComponentCRTP
{
};

template<typename T>
inline constexpr bool kUsesNodeCRTP = std::is_base_of_v<NodeCRTP<T>, T>;

template<typename T>
inline constexpr bool kUsesComponentCRTP = std::is_base_of_v<ComponentCRTP<T>, T>;

/**
 * @brief Full runtime phase contract for non-polymorphic ECS objects.
 * @remarks
 * Runtime types must use `TRuntimeTickCRTP` and provide optional `*Impl`
 * hooks (`TickImpl`, `FixedTickImpl`, ...).
 */
template<typename T>
concept RuntimeTickType =
    NonPolymorphicRuntimeType<T> &&
    (kUsesRuntimeTickCRTP<T> || kUsesNodeCRTP<T> || kUsesComponentCRTP<T>);

template<typename T>
concept HasOnCreateImpl =
    requires(T& Obj, IWorld& WorldRef) {
        { Obj.OnCreateImpl(WorldRef) } -> std::same_as<void>;
    };

template<typename T>
concept HasOnDestroyImpl =
    requires(T& Obj, IWorld& WorldRef) {
        { Obj.OnDestroyImpl(WorldRef) } -> std::same_as<void>;
    };

template<typename T>
concept HasPreTickImpl =
    requires(T& Obj, IWorld& WorldRef, float DeltaSeconds) {
        { Obj.PreTickImpl(WorldRef, DeltaSeconds) } -> std::same_as<void>;
    };

template<typename T>
concept HasTickImpl =
    requires(T& Obj, IWorld& WorldRef, float DeltaSeconds) {
        { Obj.TickImpl(WorldRef, DeltaSeconds) } -> std::same_as<void>;
    };

template<typename T>
concept HasFixedTickImpl =
    requires(T& Obj, IWorld& WorldRef, float DeltaSeconds) {
        { Obj.FixedTickImpl(WorldRef, DeltaSeconds) } -> std::same_as<void>;
    };

template<typename T>
concept HasLateTickImpl =
    requires(T& Obj, IWorld& WorldRef, float DeltaSeconds) {
        { Obj.LateTickImpl(WorldRef, DeltaSeconds) } -> std::same_as<void>;
    };

template<typename T>
concept HasPostTickImpl =
    requires(T& Obj, IWorld& WorldRef, float DeltaSeconds) {
        { Obj.PostTickImpl(WorldRef, DeltaSeconds) } -> std::same_as<void>;
    };

/**
 * @brief Compile-time tick priority helper.
 * @tparam TObject Runtime object type.
 * @return Tick priority (default 0 when no `kTickPriority` exists).
 */
template<typename TObject>
consteval int RuntimeTickPriority()
{
    if constexpr (requires { TObject::kTickPriority; })
    {
        return static_cast<int>(TObject::kTickPriority);
    }
    else
    {
        return 0;
    }
}

/**
 * @brief Dense runtime handle used by world-owned typed storages.
 * @tparam TObject Runtime object type.
 */
template<typename TObject>
struct TDenseRuntimeHandle
{
    static constexpr uint32_t kInvalidStorageToken = 0;
    static constexpr uint32_t kInvalidIndex = std::numeric_limits<uint32_t>::max();

    Uuid Id{};
    uint32_t StorageToken = kInvalidStorageToken;
    uint32_t Index = kInvalidIndex;
    uint32_t Generation = 0;

    bool IsNull() const noexcept
    {
        return Id.is_nil();
    }

    bool HasRuntimeKey() const noexcept
    {
        return StorageToken != kInvalidStorageToken && Index != kInvalidIndex;
    }

    explicit operator bool() const noexcept
    {
        return !IsNull();
    }

    bool operator==(const TDenseRuntimeHandle&) const noexcept = default;
};

/**
 * @brief Hot-path dense typed storage with generation-safe handles.
 * @tparam TObject Non-polymorphic runtime object type.
 * @remarks
 * Storage is contiguous by type and ticked without virtual dispatch.
 */
template<RuntimeTickType TObject>
class TDenseRuntimeStorage final
{
public:
    static_assert(!std::is_polymorphic_v<TObject>,
                  "Node/component runtime types must be non-polymorphic");

    static constexpr bool kHasOnCreatePhase = HasOnCreateImpl<TObject>;
    static constexpr bool kHasOnDestroyPhase = HasOnDestroyImpl<TObject>;
    static constexpr bool kHasPreTickPhase = HasPreTickImpl<TObject>;
    static constexpr bool kHasTickPhase = HasTickImpl<TObject>;
    static constexpr bool kHasFixedTickPhase = HasFixedTickImpl<TObject>;
    static constexpr bool kHasLateTickPhase = HasLateTickImpl<TObject>;
    static constexpr bool kHasPostTickPhase = HasPostTickImpl<TObject>;

    using Handle = TDenseRuntimeHandle<TObject>;

    explicit TDenseRuntimeStorage(
        const uint32_t StorageToken = 1)
        : m_storageToken(StorageToken == Handle::kInvalidStorageToken ? 1u : StorageToken)
    {
    }

    [[nodiscard]] uint32_t StorageToken() const noexcept
    {
        return m_storageToken;
    }

    [[nodiscard]] std::size_t Size() const noexcept
    {
        return m_denseObjects.size();
    }

    [[nodiscard]] bool Empty() const noexcept
    {
        return m_denseObjects.empty();
    }

    template<typename... TArgs>
    TExpected<Handle> Create(IWorld& WorldRef, TArgs&&... Args)
    {
        return CreateWithId(WorldRef, NewUuid(), std::forward<TArgs>(Args)...);
    }

    template<typename... TArgs>
    TExpected<Handle> CreateWithId(IWorld& WorldRef, const Uuid& Id, TArgs&&... Args)
    {
        if (Id.is_nil())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Runtime object UUID is nil"));
        }
        if (m_idToSlot.contains(Id))
        {
            return std::unexpected(MakeError(EErrorCode::AlreadyExists, "Runtime object already exists"));
        }

        const uint32_t SlotIndex = AcquireSlot(Id);
        SlotMeta& Slot = m_slots[SlotIndex];
        const uint32_t DenseIndex = static_cast<uint32_t>(m_denseObjects.size());

        try
        {
            m_denseObjects.emplace_back(std::forward<TArgs>(Args)...);
        }
        catch (...)
        {
            RollbackCreate(SlotIndex);
            return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to construct runtime object"));
        }

        m_denseSlotIndices.push_back(SlotIndex);
        Slot.Alive = true;
        Slot.DenseIndex = DenseIndex;

        if constexpr (kHasOnCreatePhase)
        {
            InvokeOnCreate(m_denseObjects.back(), WorldRef);
        }
        else
        {
            (void)WorldRef;
        }
        return MakeHandle(SlotIndex);
    }

    bool Destroy(IWorld& WorldRef, const Handle& InHandle)
    {
        uint32_t SlotIndex = Handle::kInvalidIndex;
        if (!ResolveSlot(InHandle, SlotIndex))
        {
            return false;
        }

        DestroyBySlot(WorldRef, SlotIndex);
        return true;
    }

    bool DestroySlow(IWorld& WorldRef, const Uuid& Id)
    {
        auto It = m_idToSlot.find(Id);
        if (It == m_idToSlot.end())
        {
            return false;
        }

        const uint32_t SlotIndex = It->second;
        if (SlotIndex >= m_slots.size() || !m_slots[SlotIndex].Alive)
        {
            return false;
        }

        DestroyBySlot(WorldRef, SlotIndex);
        return true;
    }

    TObject* Resolve(const Handle& InHandle)
    {
        uint32_t SlotIndex = Handle::kInvalidIndex;
        if (!ResolveSlot(InHandle, SlotIndex))
        {
            return nullptr;
        }

        const SlotMeta& Slot = m_slots[SlotIndex];
        return &m_denseObjects[Slot.DenseIndex];
    }

    const TObject* Resolve(const Handle& InHandle) const
    {
        uint32_t SlotIndex = Handle::kInvalidIndex;
        if (!ResolveSlot(InHandle, SlotIndex))
        {
            return nullptr;
        }

        const SlotMeta& Slot = m_slots[SlotIndex];
        return &m_denseObjects[Slot.DenseIndex];
    }

    TObject* ResolveSlowById(const Uuid& Id)
    {
        auto It = m_idToSlot.find(Id);
        if (It == m_idToSlot.end())
        {
            return nullptr;
        }

        const uint32_t SlotIndex = It->second;
        if (SlotIndex >= m_slots.size())
        {
            return nullptr;
        }

        const SlotMeta& Slot = m_slots[SlotIndex];
        if (!Slot.Alive || Slot.DenseIndex >= m_denseObjects.size())
        {
            return nullptr;
        }

        return &m_denseObjects[Slot.DenseIndex];
    }

    const TObject* ResolveSlowById(const Uuid& Id) const
    {
        auto It = m_idToSlot.find(Id);
        if (It == m_idToSlot.end())
        {
            return nullptr;
        }

        const uint32_t SlotIndex = It->second;
        if (SlotIndex >= m_slots.size())
        {
            return nullptr;
        }

        const SlotMeta& Slot = m_slots[SlotIndex];
        if (!Slot.Alive || Slot.DenseIndex >= m_denseObjects.size())
        {
            return nullptr;
        }

        return &m_denseObjects[Slot.DenseIndex];
    }

    TExpected<Handle> HandleById(const Uuid& Id) const
    {
        auto It = m_idToSlot.find(Id);
        if (It == m_idToSlot.end())
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Runtime object not found"));
        }

        const uint32_t SlotIndex = It->second;
        if (SlotIndex >= m_slots.size())
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, "Runtime slot index out of range"));
        }

        const SlotMeta& Slot = m_slots[SlotIndex];
        if (!Slot.Alive)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Runtime object not alive"));
        }

        return MakeHandle(SlotIndex);
    }

    void PreTick(IWorld& WorldRef, const float DeltaSeconds)
    {
        if constexpr (kHasPreTickPhase)
        {
            for (TObject& Object : m_denseObjects)
            {
                InvokePreTick(Object, WorldRef, DeltaSeconds);
            }
        }
        else
        {
            (void)WorldRef;
            (void)DeltaSeconds;
        }
    }

    void Tick(IWorld& WorldRef, const float DeltaSeconds)
    {
        if constexpr (kHasTickPhase)
        {
            for (TObject& Object : m_denseObjects)
            {
                InvokeTick(Object, WorldRef, DeltaSeconds);
            }
        }
        else
        {
            (void)WorldRef;
            (void)DeltaSeconds;
        }
    }

    void FixedTick(IWorld& WorldRef, const float DeltaSeconds)
    {
        if constexpr (kHasFixedTickPhase)
        {
            for (TObject& Object : m_denseObjects)
            {
                InvokeFixedTick(Object, WorldRef, DeltaSeconds);
            }
        }
        else
        {
            (void)WorldRef;
            (void)DeltaSeconds;
        }
    }

    void LateTick(IWorld& WorldRef, const float DeltaSeconds)
    {
        if constexpr (kHasLateTickPhase)
        {
            for (TObject& Object : m_denseObjects)
            {
                InvokeLateTick(Object, WorldRef, DeltaSeconds);
            }
        }
        else
        {
            (void)WorldRef;
            (void)DeltaSeconds;
        }
    }

    void PostTick(IWorld& WorldRef, const float DeltaSeconds)
    {
        if constexpr (kHasPostTickPhase)
        {
            for (TObject& Object : m_denseObjects)
            {
                InvokePostTick(Object, WorldRef, DeltaSeconds);
            }
        }
        else
        {
            (void)WorldRef;
            (void)DeltaSeconds;
        }
    }

    void Clear(IWorld& WorldRef)
    {
        if constexpr (kHasOnDestroyPhase)
        {
            for (TObject& Object : m_denseObjects)
            {
                InvokeOnDestroy(Object, WorldRef);
            }
        }
        else
        {
            (void)WorldRef;
        }

        m_denseObjects.clear();
        m_denseSlotIndices.clear();
        m_idToSlot.clear();
        m_freeSlotIndices.clear();

        m_freeSlotIndices.reserve(m_slots.size());
        for (uint32_t SlotIndex = 0; SlotIndex < m_slots.size(); ++SlotIndex)
        {
            SlotMeta& Slot = m_slots[SlotIndex];
            Slot.Id = {};
            Slot.Alive = false;
            Slot.DenseIndex = Handle::kInvalidIndex;
            Slot.Generation = (Slot.Generation == std::numeric_limits<uint32_t>::max()) ? 1u : (Slot.Generation + 1u);
            if (Slot.Generation == 0u)
            {
                Slot.Generation = 1u;
            }
            m_freeSlotIndices.push_back(SlotIndex);
        }
    }

private:
    static void InvokeOnCreate(TObject& Object, IWorld& WorldRef)
    {
        if constexpr (HasOnCreateImpl<TObject>)
        {
            Object.OnCreateImpl(WorldRef);
        }
        else
        {
            (void)Object;
            (void)WorldRef;
        }
    }

    static void InvokeOnDestroy(TObject& Object, IWorld& WorldRef)
    {
        if constexpr (HasOnDestroyImpl<TObject>)
        {
            Object.OnDestroyImpl(WorldRef);
        }
        else
        {
            (void)Object;
            (void)WorldRef;
        }
    }

    static void InvokePreTick(TObject& Object, IWorld& WorldRef, const float DeltaSeconds)
    {
        if constexpr (HasPreTickImpl<TObject>)
        {
            Object.PreTickImpl(WorldRef, DeltaSeconds);
        }
        else
        {
            (void)Object;
            (void)WorldRef;
            (void)DeltaSeconds;
        }
    }

    static void InvokeTick(TObject& Object, IWorld& WorldRef, const float DeltaSeconds)
    {
        if constexpr (HasTickImpl<TObject>)
        {
            Object.TickImpl(WorldRef, DeltaSeconds);
        }
        else
        {
            (void)Object;
            (void)WorldRef;
            (void)DeltaSeconds;
        }
    }

    static void InvokeFixedTick(TObject& Object, IWorld& WorldRef, const float DeltaSeconds)
    {
        if constexpr (HasFixedTickImpl<TObject>)
        {
            Object.FixedTickImpl(WorldRef, DeltaSeconds);
        }
        else
        {
            (void)Object;
            (void)WorldRef;
            (void)DeltaSeconds;
        }
    }

    static void InvokeLateTick(TObject& Object, IWorld& WorldRef, const float DeltaSeconds)
    {
        if constexpr (HasLateTickImpl<TObject>)
        {
            Object.LateTickImpl(WorldRef, DeltaSeconds);
        }
        else
        {
            (void)Object;
            (void)WorldRef;
            (void)DeltaSeconds;
        }
    }

    static void InvokePostTick(TObject& Object, IWorld& WorldRef, const float DeltaSeconds)
    {
        if constexpr (HasPostTickImpl<TObject>)
        {
            Object.PostTickImpl(WorldRef, DeltaSeconds);
        }
        else
        {
            (void)Object;
            (void)WorldRef;
            (void)DeltaSeconds;
        }
    }

    struct SlotMeta
    {
        Uuid Id{};
        uint32_t Generation = 1;
        uint32_t DenseIndex = Handle::kInvalidIndex;
        bool Alive = false;
    };

    Handle MakeHandle(const uint32_t SlotIndex) const
    {
        const SlotMeta& Slot = m_slots[SlotIndex];
        return Handle{
            .Id = Slot.Id,
            .StorageToken = m_storageToken,
            .Index = SlotIndex,
            .Generation = Slot.Generation};
    }

    bool ResolveSlot(const Handle& InHandle, uint32_t& OutSlotIndex) const
    {
        if (InHandle.StorageToken != m_storageToken || InHandle.Index == Handle::kInvalidIndex)
        {
            return false;
        }
        if (InHandle.Index >= m_slots.size())
        {
            return false;
        }

        const SlotMeta& Slot = m_slots[InHandle.Index];
        if (!Slot.Alive || Slot.Generation != InHandle.Generation || Slot.Id != InHandle.Id)
        {
            return false;
        }

        OutSlotIndex = InHandle.Index;
        return true;
    }

    uint32_t AcquireSlot(const Uuid& Id)
    {
        uint32_t SlotIndex = Handle::kInvalidIndex;
        if (!m_freeSlotIndices.empty())
        {
            SlotIndex = m_freeSlotIndices.back();
            m_freeSlotIndices.pop_back();
        }
        else
        {
            SlotIndex = static_cast<uint32_t>(m_slots.size());
            m_slots.emplace_back();
        }

        SlotMeta& Slot = m_slots[SlotIndex];
        Slot.Id = Id;
        Slot.Alive = false;
        Slot.DenseIndex = Handle::kInvalidIndex;
        if (Slot.Generation == 0u)
        {
            Slot.Generation = 1u;
        }
        m_idToSlot[Id] = SlotIndex;
        return SlotIndex;
    }

    void RollbackCreate(const uint32_t SlotIndex)
    {
        if (SlotIndex >= m_slots.size())
        {
            return;
        }

        SlotMeta& Slot = m_slots[SlotIndex];
        m_idToSlot.erase(Slot.Id);
        Slot.Id = {};
        Slot.DenseIndex = Handle::kInvalidIndex;
        Slot.Alive = false;
        m_freeSlotIndices.push_back(SlotIndex);
    }

    void DestroyBySlot(IWorld& WorldRef, const uint32_t SlotIndex)
    {
        if (SlotIndex >= m_slots.size())
        {
            return;
        }

        SlotMeta& Slot = m_slots[SlotIndex];
        if (!Slot.Alive || Slot.DenseIndex == Handle::kInvalidIndex || Slot.DenseIndex >= m_denseObjects.size())
        {
            return;
        }

        const uint32_t DenseIndex = Slot.DenseIndex;
        const uint32_t LastDenseIndex = static_cast<uint32_t>(m_denseObjects.size() - 1u);

        if constexpr (kHasOnDestroyPhase)
        {
            InvokeOnDestroy(m_denseObjects[DenseIndex], WorldRef);
        }
        else
        {
            (void)WorldRef;
        }

        if (DenseIndex != LastDenseIndex)
        {
            std::swap(m_denseObjects[DenseIndex], m_denseObjects[LastDenseIndex]);

            const uint32_t MovedSlotIndex = m_denseSlotIndices[LastDenseIndex];
            m_denseSlotIndices[DenseIndex] = MovedSlotIndex;
            m_slots[MovedSlotIndex].DenseIndex = DenseIndex;
        }

        m_denseObjects.pop_back();
        m_denseSlotIndices.pop_back();

        m_idToSlot.erase(Slot.Id);
        Slot.Id = {};
        Slot.Alive = false;
        Slot.DenseIndex = Handle::kInvalidIndex;
        Slot.Generation = (Slot.Generation == std::numeric_limits<uint32_t>::max()) ? 1u : (Slot.Generation + 1u);
        if (Slot.Generation == 0u)
        {
            Slot.Generation = 1u;
        }
        m_freeSlotIndices.push_back(SlotIndex);
    }

    uint32_t m_storageToken = 1;
    std::vector<TObject> m_denseObjects{};
    std::vector<uint32_t> m_denseSlotIndices{};
    std::vector<SlotMeta> m_slots{};
    std::vector<uint32_t> m_freeSlotIndices{};
    std::unordered_map<Uuid, uint32_t, UuidHash> m_idToSlot{};
};

struct RuntimeNodeRecord final : NodeCRTP<RuntimeNodeRecord>
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::RuntimeNodeRecord";

    std::string Name{"Node"};
    TypeId Type{};
    bool Active = true;
    bool Replicated = false;
};

using RuntimeNodeHandle = TDenseRuntimeHandle<RuntimeNodeRecord>;

struct RuntimeComponentRecord final
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::RuntimeComponentRecord";
};

using RuntimeComponentHandle = TDenseRuntimeHandle<RuntimeComponentRecord>;

template<typename TObject>
RuntimeComponentHandle ToRuntimeComponentHandle(const TDenseRuntimeHandle<TObject>& InHandle)
{
    return RuntimeComponentHandle{
        .Id = InHandle.Id,
        .StorageToken = InHandle.StorageToken,
        .Index = InHandle.Index,
        .Generation = InHandle.Generation};
}

template<typename TObject>
TDenseRuntimeHandle<TObject> ToTypedRuntimeHandle(const RuntimeComponentHandle& InHandle)
{
    return TDenseRuntimeHandle<TObject>{
        .Id = InHandle.Id,
        .StorageToken = InHandle.StorageToken,
        .Index = InHandle.Index,
        .Generation = InHandle.Generation};
}

struct RuntimeNodeTransform
{
    Vec3 Position{};
    Quat Rotation = Quat::Identity();
    Vec3 Scale{1.0f, 1.0f, 1.0f};
};

/**
 * @brief World-owned dense hierarchy runtime for nodes.
 * @remarks
 * This is the first replacement slice for `Level` ownership semantics.
 * Node identity + hierarchy are centralized under `IWorld`.
 */
class WorldNodeRuntime final
{
public:
    using Handle = RuntimeNodeHandle;

    TExpected<Handle> CreateNode(IWorld& WorldRef, std::string Name, const TypeId& Type)
    {
        return CreateNodeWithId(WorldRef, NewUuid(), std::move(Name), Type);
    }

    TExpected<Handle> CreateNodeWithId(IWorld& WorldRef, const Uuid& Id, std::string Name, const TypeId& Type)
    {
        if (Type == TypeId{})
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Runtime node type is null"));
        }

        RuntimeNodeRecord Record{};
        Record.Name = std::move(Name);
        Record.Type = Type;

        auto HandleResult = m_nodes.CreateWithId(WorldRef, Id, std::move(Record));
        if (!HandleResult)
        {
            return std::unexpected(HandleResult.error());
        }

        const Handle CreatedHandle = *HandleResult;
        if (CreatedHandle.Index == Handle::kInvalidIndex)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, "Runtime node slot index is invalid"));
        }

        EnsureHierarchySlot(CreatedHandle.Index);
        HierarchyEntry& Entry = m_hierarchyBySlot[CreatedHandle.Index];
        Entry = {};
        Entry.Generation = CreatedHandle.Generation;
        Entry.Alive = true;

        AddRootIfMissing(CreatedHandle);
        return CreatedHandle;
    }

    Result DestroyNode(IWorld& WorldRef, const Handle NodeHandle)
    {
        if (NodeHandle.IsNull())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Runtime node handle is null"));
        }

        return DestroyNodeIterative(WorldRef, NodeHandle);
    }

    Result AttachChild(const Handle ParentHandle, const Handle ChildHandle)
    {
        if (ParentHandle.IsNull() || ChildHandle.IsNull())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Parent/child handle is null"));
        }
        if (ParentHandle == ChildHandle)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Node cannot be parent of itself"));
        }
        if (!m_nodes.Resolve(ParentHandle))
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Parent runtime node not found"));
        }
        if (!m_nodes.Resolve(ChildHandle))
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Child runtime node not found"));
        }

        HierarchyEntry* ParentState = EntryForHandle(ParentHandle);
        HierarchyEntry* ChildState = EntryForHandle(ChildHandle);
        if (!ParentState || !ChildState)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, "Runtime hierarchy state missing"));
        }

        if (!ChildState->Parent.IsNull())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Child already has a parent"));
        }

        for (Handle Cursor = ParentHandle; !Cursor.IsNull();)
        {
            if (Cursor == ChildHandle)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Hierarchy cycle detected"));
            }

            const HierarchyEntry* CursorState = EntryForHandle(Cursor);
            if (!CursorState)
            {
                break;
            }
            Cursor = CursorState->Parent;
        }

        auto& ParentChildren = ParentState->Children;
        if (std::find(ParentChildren.begin(), ParentChildren.end(), ChildHandle) == ParentChildren.end())
        {
            ParentChildren.push_back(ChildHandle);
        }

        ChildState->Parent = ParentHandle;
        RemoveRootIfPresent(ChildHandle);
        MarkSubtreeDirty(ChildHandle);
        return Ok();
    }

    Result DetachChild(const Handle ChildHandle)
    {
        if (ChildHandle.IsNull())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Child handle is null"));
        }
        if (!m_nodes.Resolve(ChildHandle))
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Child runtime node not found"));
        }

        HierarchyEntry* ChildState = EntryForHandle(ChildHandle);
        if (!ChildState)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, "Child hierarchy state missing"));
        }

        const Handle ParentHandle = ChildState->Parent;
        if (!ParentHandle.IsNull())
        {
            if (HierarchyEntry* ParentState = EntryForHandle(ParentHandle))
            {
                RemoveChildLink(*ParentState, ChildHandle);
            }
        }
        ChildState->Parent = {};
        AddRootIfMissing(ChildHandle);
        MarkSubtreeDirty(ChildHandle);
        return Ok();
    }

    [[nodiscard]] RuntimeNodeRecord* Resolve(const Handle NodeHandle)
    {
        return m_nodes.Resolve(NodeHandle);
    }

    [[nodiscard]] const RuntimeNodeRecord* Resolve(const Handle NodeHandle) const
    {
        return m_nodes.Resolve(NodeHandle);
    }

    [[nodiscard]] TExpected<Handle> HandleById(const Uuid& Id) const
    {
        return m_nodes.HandleById(Id);
    }

    [[nodiscard]] Handle Parent(const Handle ChildHandle) const
    {
        const HierarchyEntry* ChildState = EntryForHandle(ChildHandle);
        if (!ChildState)
        {
            return {};
        }

        const Handle ParentHandle = ChildState->Parent;
        return EntryForHandle(ParentHandle) ? ParentHandle : Handle{};
    }

    [[nodiscard]] std::vector<Handle> Children(const Handle ParentHandle) const
    {
        const HierarchyEntry* ParentState = EntryForHandle(ParentHandle);
        if (!ParentState)
        {
            return {};
        }

        std::vector<Handle> Result{};
        Result.reserve(ParentState->Children.size());
        ForEachChild(ParentHandle, [&](const Handle ChildHandle) {
            Result.push_back(ChildHandle);
        });
        return Result;
    }

    template<typename TVisitor>
        requires std::invocable<TVisitor, Handle>
    void ForEachChild(const Handle ParentHandle, TVisitor&& Visitor) const
    {
        const HierarchyEntry* ParentState = EntryForHandle(ParentHandle);
        if (!ParentState)
        {
            return;
        }

        for (const Handle ChildHandle : ParentState->Children)
        {
            if (EntryForHandle(ChildHandle))
            {
                Visitor(ChildHandle);
            }
        }
    }

    [[nodiscard]] const std::vector<Handle>& Roots() const
    {
        return m_roots;
    }

    [[nodiscard]] std::size_t Size() const
    {
        return m_nodes.Size();
    }

    bool SetLocalTransform(const Handle NodeHandle, const RuntimeNodeTransform& LocalTransform)
    {
        if (!m_nodes.Resolve(NodeHandle))
        {
            return false;
        }

        HierarchyEntry* Entry = EntryForHandle(NodeHandle);
        if (!Entry)
        {
            return false;
        }

        Entry->LocalTransform = NormalizeTransformRotation(LocalTransform);
        Entry->HasLocalTransform = true;
        MarkSubtreeDirty(NodeHandle);
        return true;
    }

    bool ClearLocalTransform(const Handle NodeHandle)
    {
        if (!m_nodes.Resolve(NodeHandle))
        {
            return false;
        }

        HierarchyEntry* Entry = EntryForHandle(NodeHandle);
        if (!Entry)
        {
            return false;
        }

        if (!Entry->HasLocalTransform)
        {
            return true;
        }

        Entry->LocalTransform = IdentityTransform();
        Entry->HasLocalTransform = false;
        MarkSubtreeDirty(NodeHandle);
        return true;
    }

    bool TryGetLocalTransform(const Handle NodeHandle, RuntimeNodeTransform& OutTransform) const
    {
        OutTransform = IdentityTransform();

        if (!m_nodes.Resolve(NodeHandle))
        {
            return false;
        }

        const HierarchyEntry* Entry = EntryForHandle(NodeHandle);
        if (!Entry || !Entry->HasLocalTransform)
        {
            return false;
        }

        OutTransform = Entry->LocalTransform;
        return true;
    }

    bool TryGetWorldTransform(const Handle NodeHandle, RuntimeNodeTransform& OutTransform)
    {
        OutTransform = IdentityTransform();
        bool HasTransform = false;
        if (!ComputeWorldTransform(NodeHandle, OutTransform, HasTransform))
        {
            return false;
        }
        return HasTransform;
    }

    bool TryGetParentWorldTransform(const Handle NodeHandle, RuntimeNodeTransform& OutTransform)
    {
        OutTransform = IdentityTransform();
        if (NodeHandle.IsNull() || !m_nodes.Resolve(NodeHandle))
        {
            return false;
        }

        const Handle ParentHandle = Parent(NodeHandle);
        if (ParentHandle.IsNull())
        {
            return false;
        }

        bool HasTransform = false;
        if (!ComputeWorldTransform(ParentHandle, OutTransform, HasTransform))
        {
            return false;
        }
        return HasTransform;
    }

    bool TrySetWorldTransform(const Handle NodeHandle, const RuntimeNodeTransform& WorldTransform)
    {
        if (NodeHandle.IsNull() || !m_nodes.Resolve(NodeHandle))
        {
            return false;
        }

        RuntimeNodeTransform ParentWorld = IdentityTransform();
        const bool HasParentWorld = TryGetParentWorldTransform(NodeHandle, ParentWorld);

        const RuntimeNodeTransform NormalizedWorld = NormalizeTransformRotation(WorldTransform);
        const RuntimeNodeTransform Local = HasParentWorld
            ? LocalTransformFromWorld(ParentWorld, NormalizedWorld)
            : NormalizedWorld;

        return SetLocalTransform(NodeHandle, Local);
    }

    void Clear(IWorld& WorldRef)
    {
        m_hierarchyBySlot.clear();
        m_roots.clear();
        m_nodes.Clear(WorldRef);
    }

private:
    struct HierarchyEntry
    {
        Handle Parent{};
        std::vector<Handle> Children{};
        RuntimeNodeTransform LocalTransform{};
        RuntimeNodeTransform CachedWorldTransform{};
        uint32_t Generation = 0;
        bool HasLocalTransform = false;
        bool CachedHasWorldTransform = false;
        bool Dirty = true;
        bool Alive = false;
    };

    static void RemoveChildLink(HierarchyEntry& ParentEntry, const Handle ChildHandle)
    {
        auto It = std::remove(ParentEntry.Children.begin(), ParentEntry.Children.end(), ChildHandle);
        if (It != ParentEntry.Children.end())
        {
            ParentEntry.Children.erase(It, ParentEntry.Children.end());
        }
    }

    void EnsureHierarchySlot(const uint32_t SlotIndex)
    {
        if (SlotIndex >= m_hierarchyBySlot.size())
        {
            m_hierarchyBySlot.resize(static_cast<std::size_t>(SlotIndex) + 1u);
        }
    }

    HierarchyEntry* EntryForHandle(const Handle NodeHandle)
    {
        if (NodeHandle.IsNull() || NodeHandle.Index == Handle::kInvalidIndex)
        {
            return nullptr;
        }
        if (NodeHandle.Index >= m_hierarchyBySlot.size())
        {
            return nullptr;
        }

        HierarchyEntry& Entry = m_hierarchyBySlot[NodeHandle.Index];
        if (!Entry.Alive || Entry.Generation != NodeHandle.Generation)
        {
            return nullptr;
        }
        return &Entry;
    }

    const HierarchyEntry* EntryForHandle(const Handle NodeHandle) const
    {
        if (NodeHandle.IsNull() || NodeHandle.Index == Handle::kInvalidIndex)
        {
            return nullptr;
        }
        if (NodeHandle.Index >= m_hierarchyBySlot.size())
        {
            return nullptr;
        }

        const HierarchyEntry& Entry = m_hierarchyBySlot[NodeHandle.Index];
        if (!Entry.Alive || Entry.Generation != NodeHandle.Generation)
        {
            return nullptr;
        }
        return &Entry;
    }

    void AddRootIfMissing(const Handle NodeHandle)
    {
        if (std::find(m_roots.begin(), m_roots.end(), NodeHandle) == m_roots.end())
        {
            m_roots.push_back(NodeHandle);
        }
    }

    void RemoveRootIfPresent(const Handle NodeHandle)
    {
        auto It = std::find(m_roots.begin(), m_roots.end(), NodeHandle);
        if (It != m_roots.end())
        {
            m_roots.erase(It);
        }
    }

    static RuntimeNodeTransform IdentityTransform()
    {
        return {};
    }

    static RuntimeNodeTransform NormalizeTransformRotation(const RuntimeNodeTransform& InTransform)
    {
        RuntimeNodeTransform Out = InTransform;
        Out.Rotation = NormalizeQuatOrIdentity(Out.Rotation);
        return Out;
    }

    static Quat NormalizeQuatOrIdentity(const Quat& Rotation)
    {
        Quat Out = Rotation;
        if (Out.squaredNorm() > static_cast<Quat::Scalar>(0))
        {
            Out.normalize();
        }
        else
        {
            Out = Quat::Identity();
        }
        return Out;
    }

    static Vec3 SafeScaleDivide(const Vec3& Numerator, const Vec3& Denominator)
    {
        constexpr Vec3::Scalar kMinScaleMagnitude = static_cast<Vec3::Scalar>(1.0e-6);
        Vec3 Out{};
        const auto DivideAxis = [&](const Vec3::Scalar Value, const Vec3::Scalar Divisor) -> Vec3::Scalar {
            if (std::abs(Divisor) <= kMinScaleMagnitude)
            {
                return static_cast<Vec3::Scalar>(0);
            }
            return Value / Divisor;
        };

        Out.x() = DivideAxis(Numerator.x(), Denominator.x());
        Out.y() = DivideAxis(Numerator.y(), Denominator.y());
        Out.z() = DivideAxis(Numerator.z(), Denominator.z());
        return Out;
    }

    static RuntimeNodeTransform ComposeTransform(const RuntimeNodeTransform& ParentWorld, const RuntimeNodeTransform& Local)
    {
        const RuntimeNodeTransform NormalizedParent = NormalizeTransformRotation(ParentWorld);
        const RuntimeNodeTransform NormalizedLocal = NormalizeTransformRotation(Local);

        RuntimeNodeTransform Out = IdentityTransform();
        Out.Position = NormalizedParent.Position
                     + (NormalizedParent.Rotation * NormalizedParent.Scale.cwiseProduct(NormalizedLocal.Position));
        Out.Rotation = NormalizeQuatOrIdentity(NormalizedParent.Rotation * NormalizedLocal.Rotation);
        Out.Scale = NormalizedParent.Scale.cwiseProduct(NormalizedLocal.Scale);
        return Out;
    }

    static RuntimeNodeTransform LocalTransformFromWorld(const RuntimeNodeTransform& ParentWorld, const RuntimeNodeTransform& World)
    {
        const RuntimeNodeTransform NormalizedParent = NormalizeTransformRotation(ParentWorld);
        const RuntimeNodeTransform NormalizedWorld = NormalizeTransformRotation(World);

        const Quat ParentInverse = NormalizedParent.Rotation.conjugate();
        const Vec3 ParentSpacePosition = ParentInverse * (NormalizedWorld.Position - NormalizedParent.Position);

        RuntimeNodeTransform Out = IdentityTransform();
        Out.Position = SafeScaleDivide(ParentSpacePosition, NormalizedParent.Scale);
        Out.Rotation = NormalizeQuatOrIdentity(ParentInverse * NormalizedWorld.Rotation);
        Out.Scale = SafeScaleDivide(NormalizedWorld.Scale, NormalizedParent.Scale);
        return Out;
    }

    void MarkSubtreeDirty(const Handle NodeHandle)
    {
        if (NodeHandle.IsNull())
        {
            return;
        }

        m_dirtyTraversalScratch.clear();
        m_dirtyTraversalScratch.push_back(NodeHandle);
        while (!m_dirtyTraversalScratch.empty())
        {
            const Handle Current = m_dirtyTraversalScratch.back();
            m_dirtyTraversalScratch.pop_back();

            HierarchyEntry* Entry = EntryForHandle(Current);
            if (!Entry)
            {
                continue;
            }

            Entry->Dirty = true;
            for (const Handle ChildHandle : Entry->Children)
            {
                m_dirtyTraversalScratch.push_back(ChildHandle);
            }
        }
    }

    bool ComputeWorldTransform(const Handle NodeHandle, RuntimeNodeTransform& OutTransform, bool& OutHasTransform)
    {
        OutTransform = IdentityTransform();
        OutHasTransform = false;

        if (NodeHandle.IsNull() || !m_nodes.Resolve(NodeHandle))
        {
            return false;
        }

        std::vector<Handle> Ancestry{};
        Ancestry.reserve(16);

        Handle Cursor = NodeHandle;
        std::size_t Depth = 0;
        const std::size_t MaxDepth = m_hierarchyBySlot.size() + 1u;
        while (true)
        {
            if (Depth++ > MaxDepth)
            {
                return false;
            }

            HierarchyEntry* Entry = EntryForHandle(Cursor);
            if (!Entry)
            {
                return false;
            }

            Ancestry.push_back(Cursor);
            if (!Entry->Dirty)
            {
                OutTransform = Entry->CachedWorldTransform;
                OutHasTransform = Entry->CachedHasWorldTransform;
                break;
            }

            if (Entry->Parent.IsNull())
            {
                OutTransform = IdentityTransform();
                OutHasTransform = false;
                break;
            }

            Cursor = Entry->Parent;
            if (!m_nodes.Resolve(Cursor))
            {
                return false;
            }
        }

        for (auto It = Ancestry.rbegin(); It != Ancestry.rend(); ++It)
        {
            HierarchyEntry* Entry = EntryForHandle(*It);
            if (!Entry)
            {
                return false;
            }

            if (!Entry->Dirty)
            {
                OutTransform = Entry->CachedWorldTransform;
                OutHasTransform = Entry->CachedHasWorldTransform;
                continue;
            }

            RuntimeNodeTransform ComputedWorld = IdentityTransform();
            bool ComputedHasWorldTransform = false;
            if (Entry->HasLocalTransform)
            {
                ComputedWorld = OutHasTransform ? ComposeTransform(OutTransform, Entry->LocalTransform) : Entry->LocalTransform;
                ComputedHasWorldTransform = true;
            }
            else if (OutHasTransform)
            {
                ComputedWorld = OutTransform;
                ComputedHasWorldTransform = true;
            }

            Entry->CachedWorldTransform = ComputedWorld;
            Entry->CachedHasWorldTransform = ComputedHasWorldTransform;
            Entry->Dirty = false;

            OutTransform = ComputedWorld;
            OutHasTransform = ComputedHasWorldTransform;
        }

        return true;
    }

    Result DestroyNodeIterative(IWorld& WorldRef, const Handle RootHandle)
    {
        if (RootHandle.IsNull() || !m_nodes.Resolve(RootHandle))
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Runtime node not found"));
        }

        m_destroyTraversalScratch.clear();
        m_destroyTraversalScratch.emplace_back(RootHandle, false);

        while (!m_destroyTraversalScratch.empty())
        {
            const auto [CurrentHandle, Expanded] = m_destroyTraversalScratch.back();
            m_destroyTraversalScratch.pop_back();

            if (!Expanded)
            {
                if (!m_nodes.Resolve(CurrentHandle))
                {
                    continue;
                }

                HierarchyEntry* Entry = EntryForHandle(CurrentHandle);
                if (!Entry)
                {
                    return std::unexpected(MakeError(EErrorCode::InternalError, "Runtime hierarchy state missing"));
                }

                m_destroyTraversalScratch.emplace_back(CurrentHandle, true);
                for (const Handle ChildHandle : Entry->Children)
                {
                    if (m_nodes.Resolve(ChildHandle))
                    {
                        m_destroyTraversalScratch.emplace_back(ChildHandle, false);
                    }
                }
                continue;
            }

            HierarchyEntry* Entry = EntryForHandle(CurrentHandle);
            if (!Entry)
            {
                continue;
            }

            const Handle ParentHandle = Entry->Parent;
            if (!ParentHandle.IsNull())
            {
                if (HierarchyEntry* ParentEntry = EntryForHandle(ParentHandle))
                {
                    RemoveChildLink(*ParentEntry, CurrentHandle);
                }
            }

            RemoveRootIfPresent(CurrentHandle);
            *Entry = HierarchyEntry{};

            if (!m_nodes.Destroy(WorldRef, CurrentHandle))
            {
                return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to destroy runtime node"));
            }
        }

        return Ok();
    }

    TDenseRuntimeStorage<RuntimeNodeRecord> m_nodes{};
    std::vector<HierarchyEntry> m_hierarchyBySlot{};
    std::vector<Handle> m_roots{};
    std::vector<Handle> m_dirtyTraversalScratch{};
    std::vector<std::pair<Handle, bool>> m_destroyTraversalScratch{};
};

/**
 * @brief World-owned typed storage registry for ECS runtime objects.
 * @remarks
 * Hot-path updates avoid virtual dispatch. Type erasure is reserved for cold
 * reflection/serialization style access.
 */
class WorldEcsRuntime final
{
public:
    class IErasedStorage
    {
    public:
        virtual ~IErasedStorage() = default;
        [[nodiscard]] virtual TypeId Type() const = 0;
        [[nodiscard]] virtual std::size_t Size() const = 0;
        [[nodiscard]] virtual void* ResolveRaw(const Uuid& Id) = 0;
        [[nodiscard]] virtual const void* ResolveRaw(const Uuid& Id) const = 0;
        virtual bool DestroyById(IWorld& WorldRef, const Uuid& Id) = 0;
    };

    template<RuntimeTickType TObject>
    TDenseRuntimeStorage<TObject>& Storage()
    {
        const TypeId& Type = StaticTypeId<TObject>();
        if (auto It = m_storages.find(Type); It != m_storages.end())
        {
            auto* Model = static_cast<TStorageModel<TObject>*>(It->second.get());
            return Model->TypedStorage;
        }

        const uint32_t StorageToken = AcquireStorageToken();
        auto Model = std::make_unique<TStorageModel<TObject>>(StorageToken);
        auto* ModelPtr = Model.get();
        auto* TypedStorage = &Model->TypedStorage;

        RegisterTickEntry<TObject>(TypedStorage);
        m_storages.emplace(Type, std::move(Model));
        m_storageByToken[ModelPtr->StorageToken()] = ModelPtr;
        return *TypedStorage;
    }

    template<RuntimeTickType TObject>
    TDenseRuntimeStorage<TObject>* FindStorage()
    {
        const TypeId& Type = StaticTypeId<TObject>();
        if (auto It = m_storages.find(Type); It != m_storages.end())
        {
            auto* Model = static_cast<TStorageModel<TObject>*>(It->second.get());
            return &Model->TypedStorage;
        }
        return nullptr;
    }

    template<RuntimeTickType TObject>
    const TDenseRuntimeStorage<TObject>* FindStorage() const
    {
        const TypeId& Type = StaticTypeId<TObject>();
        if (auto It = m_storages.find(Type); It != m_storages.end())
        {
            const auto* Model = static_cast<const TStorageModel<TObject>*>(It->second.get());
            return &Model->TypedStorage;
        }
        return nullptr;
    }

    [[nodiscard]] IErasedStorage* FindErased(const TypeId& Type)
    {
        if (auto It = m_storages.find(Type); It != m_storages.end())
        {
            return It->second.get();
        }
        return nullptr;
    }

    [[nodiscard]] const IErasedStorage* FindErased(const TypeId& Type) const
    {
        if (auto It = m_storages.find(Type); It != m_storages.end())
        {
            return It->second.get();
        }
        return nullptr;
    }

    [[nodiscard]] WorldNodeRuntime& Nodes()
    {
        return m_nodeRuntime;
    }

    [[nodiscard]] const WorldNodeRuntime& Nodes() const
    {
        return m_nodeRuntime;
    }

    template<RuntimeTickType TObject, typename... TArgs>
    TExpected<TDenseRuntimeHandle<TObject>> AddComponent(IWorld& WorldRef,
                                                         const RuntimeNodeHandle Owner,
                                                         TArgs&&... Args)
    {
        const TypeId& Type = StaticTypeId<TObject>();
        NodeComponentAttachment* Attachment = EnsureNodeAttachment(Owner);
        if (!Attachment)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Runtime owner node not found"));
        }
        if (FindNodeComponentIndex(*Attachment, Type).has_value())
        {
            return std::unexpected(MakeError(EErrorCode::AlreadyExists, "Runtime component already exists on node"));
        }

        auto& TypedStorage = Storage<TObject>();
        auto CreateResult = TypedStorage.Create(WorldRef, std::forward<TArgs>(Args)...);
        if (!CreateResult)
        {
            return std::unexpected(CreateResult.error());
        }

        Attachment->Components.push_back(NodeComponentLink{
            .Type = Type,
            .Handle = ToRuntimeComponentHandle(*CreateResult)});
        return *CreateResult;
    }

    template<RuntimeTickType TObject, typename... TArgs>
    TExpected<TDenseRuntimeHandle<TObject>> AddComponentWithId(IWorld& WorldRef,
                                                               const RuntimeNodeHandle Owner,
                                                               const Uuid& Id,
                                                               TArgs&&... Args)
    {
        const TypeId& Type = StaticTypeId<TObject>();
        NodeComponentAttachment* Attachment = EnsureNodeAttachment(Owner);
        if (!Attachment)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Runtime owner node not found"));
        }
        if (FindNodeComponentIndex(*Attachment, Type).has_value())
        {
            return std::unexpected(MakeError(EErrorCode::AlreadyExists, "Runtime component already exists on node"));
        }

        auto& TypedStorage = Storage<TObject>();
        auto CreateResult = TypedStorage.CreateWithId(WorldRef, Id, std::forward<TArgs>(Args)...);
        if (!CreateResult)
        {
            return std::unexpected(CreateResult.error());
        }

        Attachment->Components.push_back(NodeComponentLink{
            .Type = Type,
            .Handle = ToRuntimeComponentHandle(*CreateResult)});
        return *CreateResult;
    }

    template<RuntimeTickType TObject>
    TObject* Component(const RuntimeNodeHandle Owner)
    {
        const TypeId& Type = StaticTypeId<TObject>();
        NodeComponentAttachment* Attachment = FindNodeAttachment(Owner);
        if (!Attachment)
        {
            return nullptr;
        }

        const auto LinkIndex = FindNodeComponentIndex(*Attachment, Type);
        if (!LinkIndex.has_value())
        {
            return nullptr;
        }

        auto* TypedStorage = FindStorage<TObject>();
        if (!TypedStorage)
        {
            return nullptr;
        }

        const RuntimeComponentHandle GenericHandle = Attachment->Components[*LinkIndex].Handle;
        return TypedStorage->Resolve(ToTypedRuntimeHandle<TObject>(GenericHandle));
    }

    template<RuntimeTickType TObject>
    const TObject* Component(const RuntimeNodeHandle Owner) const
    {
        const TypeId& Type = StaticTypeId<TObject>();
        const NodeComponentAttachment* Attachment = FindNodeAttachment(Owner);
        if (!Attachment)
        {
            return nullptr;
        }

        const auto LinkIndex = FindNodeComponentIndex(*Attachment, Type);
        if (!LinkIndex.has_value())
        {
            return nullptr;
        }

        const auto* TypedStorage = FindStorage<TObject>();
        if (!TypedStorage)
        {
            return nullptr;
        }

        const RuntimeComponentHandle GenericHandle = Attachment->Components[*LinkIndex].Handle;
        return TypedStorage->Resolve(ToTypedRuntimeHandle<TObject>(GenericHandle));
    }

    template<RuntimeTickType TObject>
    bool RemoveComponent(IWorld& WorldRef, const RuntimeNodeHandle Owner)
    {
        const TypeId& Type = StaticTypeId<TObject>();
        NodeComponentAttachment* Attachment = FindNodeAttachment(Owner);
        if (!Attachment)
        {
            return false;
        }

        const auto LinkIndex = FindNodeComponentIndex(*Attachment, Type);
        if (!LinkIndex.has_value())
        {
            return false;
        }

        const RuntimeComponentHandle GenericHandle = Attachment->Components[*LinkIndex].Handle;
        bool Destroyed = false;
        if (auto* TypedStorage = FindStorage<TObject>())
        {
            Destroyed = TypedStorage->Destroy(WorldRef, ToTypedRuntimeHandle<TObject>(GenericHandle));
        }
        RemoveNodeComponentAt(*Attachment, *LinkIndex);
        return Destroyed;
    }

    TExpected<RuntimeComponentHandle> AddComponent(IWorld& WorldRef,
                                                   const RuntimeNodeHandle Owner,
                                                   const TypeId& Type)
    {
        return AddComponentWithId(WorldRef, Owner, Type, {});
    }

    TExpected<RuntimeComponentHandle> AddComponentWithId(IWorld& WorldRef,
                                                         const RuntimeNodeHandle Owner,
                                                         const TypeId& Type,
                                                         const Uuid& Id)
    {
        if (Type == TypeId{})
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Runtime component type is null"));
        }

        NodeComponentAttachment* Attachment = EnsureNodeAttachment(Owner);
        if (!Attachment)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Runtime owner node not found"));
        }
        if (FindNodeComponentIndex(*Attachment, Type).has_value())
        {
            return std::unexpected(MakeError(EErrorCode::AlreadyExists, "Runtime component already exists on node"));
        }

        IStorageModel* StorageModel = FindStorageModel(Type);
        if (!StorageModel)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Runtime storage for component type not found"));
        }

        auto CreateResult = StorageModel->CreateDefault(WorldRef, Id.is_nil() ? nullptr : &Id);
        if (!CreateResult)
        {
            return std::unexpected(CreateResult.error());
        }

        Attachment->Components.push_back(NodeComponentLink{
            .Type = Type,
            .Handle = *CreateResult});
        return *CreateResult;
    }

    Result RemoveComponent(IWorld& WorldRef,
                           const RuntimeNodeHandle Owner,
                           const TypeId& Type)
    {
        if (Type == TypeId{})
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Runtime component type is null"));
        }

        NodeComponentAttachment* Attachment = FindNodeAttachment(Owner);
        if (!Attachment)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Runtime owner node not found"));
        }

        const auto LinkIndex = FindNodeComponentIndex(*Attachment, Type);
        if (!LinkIndex.has_value())
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Runtime component not found on node"));
        }

        const RuntimeComponentHandle GenericHandle = Attachment->Components[*LinkIndex].Handle;
        bool Destroyed = false;
        if (IStorageModel* StorageModel = FindStorageModelByToken(GenericHandle.StorageToken))
        {
            Destroyed = StorageModel->DestroyByRuntimeHandle(WorldRef, GenericHandle);
        }
        else if (IStorageModel* TypeStorageModel = FindStorageModel(Type))
        {
            Destroyed = TypeStorageModel->DestroyByRuntimeHandle(WorldRef, GenericHandle);
        }

        RemoveNodeComponentAt(*Attachment, *LinkIndex);
        if (!GenericHandle.Id.is_nil())
        {
            ObjectRegistry::Instance().Unregister(GenericHandle.Id);
        }
        return Destroyed ? Ok() : std::unexpected(MakeError(EErrorCode::NotFound, "Runtime component not found"));
    }

    [[nodiscard]] bool HasComponent(const RuntimeNodeHandle Owner, const TypeId& Type) const
    {
        const NodeComponentAttachment* Attachment = FindNodeAttachment(Owner);
        return Attachment && FindNodeComponentIndex(*Attachment, Type).has_value();
    }

    [[nodiscard]] TExpected<RuntimeComponentHandle> ComponentHandle(const RuntimeNodeHandle Owner,
                                                                    const TypeId& Type) const
    {
        const NodeComponentAttachment* Attachment = FindNodeAttachment(Owner);
        if (!Attachment)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Runtime owner node not found"));
        }

        const auto LinkIndex = FindNodeComponentIndex(*Attachment, Type);
        if (!LinkIndex.has_value())
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Runtime component not found on node"));
        }

        return Attachment->Components[*LinkIndex].Handle;
    }

    [[nodiscard]] void* ResolveComponentRaw(const RuntimeComponentHandle Handle, const TypeId& Type)
    {
        if (Handle.IsNull() || Type == TypeId{})
        {
            return nullptr;
        }

        if (IStorageModel* StorageModel = FindStorageModelByToken(Handle.StorageToken))
        {
            if (StorageModel->Type() == Type)
            {
                return StorageModel->ResolveRawByRuntimeHandle(Handle);
            }
        }
        if (IStorageModel* TypeStorageModel = FindStorageModel(Type))
        {
            return TypeStorageModel->ResolveRawByRuntimeHandle(Handle);
        }
        return nullptr;
    }

    [[nodiscard]] const void* ResolveComponentRaw(const RuntimeComponentHandle Handle, const TypeId& Type) const
    {
        if (Handle.IsNull() || Type == TypeId{})
        {
            return nullptr;
        }

        if (const IStorageModel* StorageModel = FindStorageModelByToken(Handle.StorageToken))
        {
            if (StorageModel->Type() == Type)
            {
                return StorageModel->ResolveRawByRuntimeHandle(Handle);
            }
        }
        if (const IStorageModel* TypeStorageModel = FindStorageModel(Type))
        {
            return TypeStorageModel->ResolveRawByRuntimeHandle(Handle);
        }
        return nullptr;
    }

    Result DestroyRuntimeNode(IWorld& WorldRef, const RuntimeNodeHandle RootHandle)
    {
        if (RootHandle.IsNull())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Runtime node handle is null"));
        }
        if (!m_nodeRuntime.Resolve(RootHandle))
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Runtime node not found"));
        }

        m_componentDestroyScratch.clear();
        m_componentDestroyScratch.push_back(RootHandle);
        for (std::size_t Index = 0; Index < m_componentDestroyScratch.size(); ++Index)
        {
            const RuntimeNodeHandle Current = m_componentDestroyScratch[Index];
            if (!m_nodeRuntime.Resolve(Current))
            {
                continue;
            }

            m_nodeRuntime.ForEachChild(Current, [&](const RuntimeNodeHandle Child) {
                if (m_nodeRuntime.Resolve(Child))
                {
                    m_componentDestroyScratch.push_back(Child);
                }
            });
        }

        for (auto It = m_componentDestroyScratch.rbegin(); It != m_componentDestroyScratch.rend(); ++It)
        {
            RemoveAllComponentsOnNode(WorldRef, *It);
            ClearNodeAttachment(*It);
        }

        return m_nodeRuntime.DestroyNode(WorldRef, RootHandle);
    }

    void Tick(IWorld& WorldRef, const float DeltaSeconds)
    {
        for (const TickEntry& Entry : m_tickEntries)
        {
            if (Entry.PreTick)
            {
                Entry.PreTick(Entry.Storage, WorldRef, DeltaSeconds);
            }
            if (Entry.Tick)
            {
                Entry.Tick(Entry.Storage, WorldRef, DeltaSeconds);
            }
            if (Entry.PostTick)
            {
                Entry.PostTick(Entry.Storage, WorldRef, DeltaSeconds);
            }
        }
    }

    void FixedTick(IWorld& WorldRef, const float DeltaSeconds)
    {
        for (const TickEntry& Entry : m_tickEntries)
        {
            if (Entry.FixedTick)
            {
                Entry.FixedTick(Entry.Storage, WorldRef, DeltaSeconds);
            }
        }
    }

    void LateTick(IWorld& WorldRef, const float DeltaSeconds)
    {
        for (const TickEntry& Entry : m_tickEntries)
        {
            if (Entry.LateTick)
            {
                Entry.LateTick(Entry.Storage, WorldRef, DeltaSeconds);
            }
        }
    }

    void Clear(IWorld& WorldRef)
    {
        for (const NodeComponentAttachment& Attachment : m_nodeComponentsBySlot)
        {
            if (!Attachment.Alive)
            {
                continue;
            }

            for (const NodeComponentLink& Link : Attachment.Components)
            {
                if (!Link.Handle.Id.is_nil())
                {
                    ObjectRegistry::Instance().Unregister(Link.Handle.Id);
                }
            }
        }

        for (auto& [Type, Storage] : m_storages)
        {
            (void)Type;
            Storage->Clear(WorldRef);
        }
        m_nodeComponentsBySlot.clear();
        m_componentDestroyScratch.clear();
        m_nodeRuntime.Clear(WorldRef);
    }

private:
    class IStorageModel;

    struct NodeComponentLink
    {
        TypeId Type{};
        RuntimeComponentHandle Handle{};
    };

    struct NodeComponentAttachment
    {
        uint32_t Generation = 0;
        bool Alive = false;
        std::vector<NodeComponentLink> Components{};
    };

    [[nodiscard]] NodeComponentAttachment* EnsureNodeAttachment(const RuntimeNodeHandle Owner)
    {
        if (Owner.IsNull() || Owner.Index == RuntimeNodeHandle::kInvalidIndex || !m_nodeRuntime.Resolve(Owner))
        {
            return nullptr;
        }

        if (Owner.Index >= m_nodeComponentsBySlot.size())
        {
            m_nodeComponentsBySlot.resize(static_cast<std::size_t>(Owner.Index) + 1u);
        }

        NodeComponentAttachment& Attachment = m_nodeComponentsBySlot[Owner.Index];
        if (!Attachment.Alive || Attachment.Generation != Owner.Generation)
        {
            Attachment = NodeComponentAttachment{};
            Attachment.Generation = Owner.Generation;
            Attachment.Alive = true;
        }
        return &Attachment;
    }

    [[nodiscard]] const NodeComponentAttachment* FindNodeAttachment(const RuntimeNodeHandle Owner) const
    {
        if (Owner.IsNull() || Owner.Index == RuntimeNodeHandle::kInvalidIndex || !m_nodeRuntime.Resolve(Owner))
        {
            return nullptr;
        }
        if (Owner.Index >= m_nodeComponentsBySlot.size())
        {
            return nullptr;
        }

        const NodeComponentAttachment& Attachment = m_nodeComponentsBySlot[Owner.Index];
        if (!Attachment.Alive || Attachment.Generation != Owner.Generation)
        {
            return nullptr;
        }
        return &Attachment;
    }

    [[nodiscard]] NodeComponentAttachment* FindNodeAttachment(const RuntimeNodeHandle Owner)
    {
        return const_cast<NodeComponentAttachment*>(
            static_cast<const WorldEcsRuntime*>(this)->FindNodeAttachment(Owner));
    }

    [[nodiscard]] static std::optional<std::size_t> FindNodeComponentIndex(const NodeComponentAttachment& Attachment,
                                                                            const TypeId& Type)
    {
        for (std::size_t Index = 0; Index < Attachment.Components.size(); ++Index)
        {
            if (Attachment.Components[Index].Type == Type)
            {
                return Index;
            }
        }
        return std::nullopt;
    }

    static void RemoveNodeComponentAt(NodeComponentAttachment& Attachment, const std::size_t Index)
    {
        if (Index >= Attachment.Components.size())
        {
            return;
        }

        if (Index + 1u < Attachment.Components.size())
        {
            Attachment.Components[Index] = std::move(Attachment.Components.back());
        }
        Attachment.Components.pop_back();
    }

    void ClearNodeAttachment(const RuntimeNodeHandle Owner)
    {
        if (Owner.IsNull() || Owner.Index == RuntimeNodeHandle::kInvalidIndex)
        {
            return;
        }
        if (Owner.Index >= m_nodeComponentsBySlot.size())
        {
            return;
        }

        m_nodeComponentsBySlot[Owner.Index] = NodeComponentAttachment{};
    }

    void RemoveAllComponentsOnNode(IWorld& WorldRef, const RuntimeNodeHandle Owner)
    {
        NodeComponentAttachment* Attachment = FindNodeAttachment(Owner);
        if (!Attachment)
        {
            return;
        }

        for (const NodeComponentLink& Link : Attachment->Components)
        {
            if (IStorageModel* StorageModel = FindStorageModelByToken(Link.Handle.StorageToken))
            {
                (void)StorageModel->DestroyByRuntimeHandle(WorldRef, Link.Handle);
            }
            else if (IStorageModel* TypeStorageModel = FindStorageModel(Link.Type))
            {
                (void)TypeStorageModel->DestroyByRuntimeHandle(WorldRef, Link.Handle);
            }

            if (!Link.Handle.Id.is_nil())
            {
                ObjectRegistry::Instance().Unregister(Link.Handle.Id);
            }
        }
    }

    struct TickEntry
    {
        int Priority = 0;
        uint64_t Sequence = 0;
        void* Storage = nullptr;
        void (*PreTick)(void*, IWorld&, float) = nullptr;
        void (*Tick)(void*, IWorld&, float) = nullptr;
        void (*FixedTick)(void*, IWorld&, float) = nullptr;
        void (*LateTick)(void*, IWorld&, float) = nullptr;
        void (*PostTick)(void*, IWorld&, float) = nullptr;
    };

    class IStorageModel : public IErasedStorage
    {
    public:
        [[nodiscard]] virtual uint32_t StorageToken() const = 0;
        [[nodiscard]] virtual TExpected<RuntimeComponentHandle> CreateDefault(IWorld& WorldRef, const Uuid* ExplicitId) = 0;
        virtual bool DestroyByRuntimeHandle(IWorld& WorldRef, RuntimeComponentHandle Handle) = 0;
        [[nodiscard]] virtual void* ResolveRawByRuntimeHandle(RuntimeComponentHandle Handle) = 0;
        [[nodiscard]] virtual const void* ResolveRawByRuntimeHandle(RuntimeComponentHandle Handle) const = 0;
        virtual void Clear(IWorld& WorldRef) = 0;
    };

    template<RuntimeTickType TObject>
    class TStorageModel final : public IStorageModel
    {
    public:
        explicit TStorageModel(const uint32_t StorageToken)
            : TypedStorage(StorageToken)
        {
        }

        ~TStorageModel() override
        {
            ObjectRegistry::Instance().ReleaseRuntimePoolToken(TypedStorage.StorageToken());
        }

        [[nodiscard]] TypeId Type() const override
        {
            return StaticTypeId<TObject>();
        }

        [[nodiscard]] std::size_t Size() const override
        {
            return TypedStorage.Size();
        }

        [[nodiscard]] uint32_t StorageToken() const override
        {
            return TypedStorage.StorageToken();
        }

        [[nodiscard]] void* ResolveRaw(const Uuid& Id) override
        {
            return TypedStorage.ResolveSlowById(Id);
        }

        [[nodiscard]] const void* ResolveRaw(const Uuid& Id) const override
        {
            return TypedStorage.ResolveSlowById(Id);
        }

        bool DestroyById(IWorld& WorldRef, const Uuid& Id) override
        {
            return TypedStorage.DestroySlow(WorldRef, Id);
        }

        [[nodiscard]] TExpected<RuntimeComponentHandle> CreateDefault(IWorld& WorldRef, const Uuid* ExplicitId) override
        {
            if constexpr (!std::is_default_constructible_v<TObject>)
            {
                (void)WorldRef;
                (void)ExplicitId;
                return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                                 "Runtime component type is not default constructible"));
            }
            else
            {
                auto CreateResult = ExplicitId
                    ? TypedStorage.CreateWithId(WorldRef, *ExplicitId)
                    : TypedStorage.Create(WorldRef);
                if (!CreateResult)
                {
                    return std::unexpected(CreateResult.error());
                }
                return ToRuntimeComponentHandle(*CreateResult);
            }
        }

        bool DestroyByRuntimeHandle(IWorld& WorldRef, const RuntimeComponentHandle Handle) override
        {
            return TypedStorage.Destroy(WorldRef, ToTypedRuntimeHandle<TObject>(Handle));
        }

        [[nodiscard]] void* ResolveRawByRuntimeHandle(const RuntimeComponentHandle Handle) override
        {
            return TypedStorage.Resolve(ToTypedRuntimeHandle<TObject>(Handle));
        }

        [[nodiscard]] const void* ResolveRawByRuntimeHandle(const RuntimeComponentHandle Handle) const override
        {
            return TypedStorage.Resolve(ToTypedRuntimeHandle<TObject>(Handle));
        }

        void Clear(IWorld& WorldRef) override
        {
            TypedStorage.Clear(WorldRef);
        }

        TDenseRuntimeStorage<TObject> TypedStorage;
    };

    [[nodiscard]] IStorageModel* FindStorageModel(const TypeId& Type)
    {
        if (auto It = m_storages.find(Type); It != m_storages.end())
        {
            return It->second.get();
        }
        return nullptr;
    }

    [[nodiscard]] const IStorageModel* FindStorageModel(const TypeId& Type) const
    {
        if (auto It = m_storages.find(Type); It != m_storages.end())
        {
            return It->second.get();
        }
        return nullptr;
    }

    [[nodiscard]] IStorageModel* FindStorageModelByToken(const uint32_t StorageToken)
    {
        if (auto It = m_storageByToken.find(StorageToken); It != m_storageByToken.end())
        {
            return It->second;
        }
        return nullptr;
    }

    [[nodiscard]] const IStorageModel* FindStorageModelByToken(const uint32_t StorageToken) const
    {
        if (auto It = m_storageByToken.find(StorageToken); It != m_storageByToken.end())
        {
            return It->second;
        }
        return nullptr;
    }

    template<RuntimeTickType TObject>
    static void DispatchPreTick(void* StoragePtr, IWorld& WorldRef, const float DeltaSeconds)
    {
        auto* Storage = static_cast<TDenseRuntimeStorage<TObject>*>(StoragePtr);
        Storage->PreTick(WorldRef, DeltaSeconds);
    }

    template<RuntimeTickType TObject>
    static void DispatchTick(void* StoragePtr, IWorld& WorldRef, const float DeltaSeconds)
    {
        auto* Storage = static_cast<TDenseRuntimeStorage<TObject>*>(StoragePtr);
        Storage->Tick(WorldRef, DeltaSeconds);
    }

    template<RuntimeTickType TObject>
    static void DispatchFixedTick(void* StoragePtr, IWorld& WorldRef, const float DeltaSeconds)
    {
        auto* Storage = static_cast<TDenseRuntimeStorage<TObject>*>(StoragePtr);
        Storage->FixedTick(WorldRef, DeltaSeconds);
    }

    template<RuntimeTickType TObject>
    static void DispatchLateTick(void* StoragePtr, IWorld& WorldRef, const float DeltaSeconds)
    {
        auto* Storage = static_cast<TDenseRuntimeStorage<TObject>*>(StoragePtr);
        Storage->LateTick(WorldRef, DeltaSeconds);
    }

    template<RuntimeTickType TObject>
    static void DispatchPostTick(void* StoragePtr, IWorld& WorldRef, const float DeltaSeconds)
    {
        auto* Storage = static_cast<TDenseRuntimeStorage<TObject>*>(StoragePtr);
        Storage->PostTick(WorldRef, DeltaSeconds);
    }

    template<RuntimeTickType TObject>
    void RegisterTickEntry(TDenseRuntimeStorage<TObject>* Storage)
    {
        constexpr bool kHasAnyTickPhase =
            TDenseRuntimeStorage<TObject>::kHasPreTickPhase ||
            TDenseRuntimeStorage<TObject>::kHasTickPhase ||
            TDenseRuntimeStorage<TObject>::kHasFixedTickPhase ||
            TDenseRuntimeStorage<TObject>::kHasLateTickPhase ||
            TDenseRuntimeStorage<TObject>::kHasPostTickPhase;
        if constexpr (!kHasAnyTickPhase)
        {
            return;
        }

        const int Priority = RuntimeTickPriority<TObject>();
        const uint64_t Sequence = m_nextTickSequence++;

        TickEntry Entry{};
        Entry.Priority = Priority;
        Entry.Sequence = Sequence;
        Entry.Storage = Storage;
        if constexpr (TDenseRuntimeStorage<TObject>::kHasPreTickPhase)
        {
            Entry.PreTick = &DispatchPreTick<TObject>;
        }
        if constexpr (TDenseRuntimeStorage<TObject>::kHasTickPhase)
        {
            Entry.Tick = &DispatchTick<TObject>;
        }
        if constexpr (TDenseRuntimeStorage<TObject>::kHasFixedTickPhase)
        {
            Entry.FixedTick = &DispatchFixedTick<TObject>;
        }
        if constexpr (TDenseRuntimeStorage<TObject>::kHasLateTickPhase)
        {
            Entry.LateTick = &DispatchLateTick<TObject>;
        }
        if constexpr (TDenseRuntimeStorage<TObject>::kHasPostTickPhase)
        {
            Entry.PostTick = &DispatchPostTick<TObject>;
        }

        const auto It = std::lower_bound(m_tickEntries.begin(),
                                         m_tickEntries.end(),
                                         Entry,
                                         [](const TickEntry& Left, const TickEntry& Right) {
                                             if (Left.Priority != Right.Priority)
                                             {
                                                 return Left.Priority < Right.Priority;
                                             }
                                             return Left.Sequence < Right.Sequence;
                                         });
        m_tickEntries.insert(It, Entry);
    }

    uint32_t AcquireStorageToken()
    {
        const uint32_t Token = ObjectRegistry::Instance().AcquireRuntimePoolToken();
        if (Token == TDenseRuntimeHandle<RuntimeNodeRecord>::kInvalidStorageToken)
        {
            DEBUG_ASSERT(false, "Failed to acquire runtime storage token");
        }
        return Token;
    }

    std::unordered_map<TypeId, std::unique_ptr<IStorageModel>, UuidHash> m_storages{};
    std::unordered_map<uint32_t, IStorageModel*> m_storageByToken{};
    WorldNodeRuntime m_nodeRuntime{};
    std::vector<NodeComponentAttachment> m_nodeComponentsBySlot{};
    std::vector<RuntimeNodeHandle> m_componentDestroyScratch{};
    std::vector<TickEntry> m_tickEntries{};
    uint64_t m_nextTickSequence = 0;
};

} // namespace SnAPI::GameFramework
