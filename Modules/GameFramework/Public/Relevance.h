#pragma once

#include <functional>
#include "GameThreading.h"
#include <memory>
#include <mutex>
#include <unordered_map>

#include "Expected.h"
#include "BaseComponent.h"
#include "StaticTypeId.h"
#include "TypeName.h"
#include "Uuid.h"
#include "ReflectionAnnotations.h"

namespace SnAPI::GameFramework
{

class Level;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Inputs provided to a relevance-policy evaluation.
 *
 * Relevance policies are intentionally evaluated against a narrow context rather than
 * against the full world API. This keeps the policy contract cheap to pass around and
 * makes the decision inputs explicit.
 *
 * Lifetime and ownership:
 * - `Node` is a value handle copy.
 * - `Graph` is a borrowed reference to the owning level and must not be retained past
 *   the evaluation call.
 *
 * Threading:
 * - Main-thread only unless the owning level explicitly guarantees otherwise.
 */
struct RelevanceContext
{
    NodeHandle Node; /**< @brief Handle of the node currently being tested for relevance. */
    std::reference_wrapper<Level> Graph; /**< @brief Borrowed owning level used for neighborhood or graph-aware decisions. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Process-wide registry that binds reflected relevance-policy types to evaluation callbacks.
 *
 * `RelevanceComponent` stores policy state in type-erased form. The registry supplies
 * the code path that turns that erased payload back into "call `PolicyT::Evaluate(...)`".
 * This keeps runtime storage compact while still allowing arbitrary policy structs to be
 * registered lazily on first use.
 *
 * Core semantics:
 * - Registration is keyed by reflected `TypeId`.
 * - Duplicate registration of the same type is ignored.
 * - The registry does not own policy instances; it only owns dispatch metadata.
 * - `Find()` returns metadata only when the policy type has already been registered.
 *
 * Threading:
 * - Not generally thread-safe.
 * - Internal `GameMutex` use provides affinity validation, not real mutual exclusion.
 * - Register and lookup on the game thread or provide external synchronization.
 *
 * @see RelevanceComponent
 */
class RelevancePolicyRegistry
{
public:
    /**
     * @brief Type-erased function signature used to evaluate one policy instance.
     * @param PolicyData Borrowed pointer to the stored policy object. The pointee must
     *        be of the same concrete type that was passed to `Register<PolicyT>()`.
     * @param Context Borrowed evaluation inputs for the current node.
     * @return `true` when the node should be treated as relevant/active.
     */
    using EvaluateFn = bool(*)(const void* PolicyData, const RelevanceContext& Context);

    /**
     * @brief Dispatch metadata recorded for a registered policy type.
     *
     * The registry is intentionally minimal today: evaluation is the only required
     * behavior. Additional policy-side metadata can be added here later without
     * changing the component storage format.
     */
    struct PolicyInfo
    {
        EvaluateFn Evaluate = nullptr; /**< @brief Type-erased evaluation entry point for the policy type. */
    };

    /**
     * @brief Register a policy type and its type-erased evaluation trampoline.
     * @tparam PolicyT Policy type (must implement Evaluate).
     *
     * `PolicyT` is expected to provide `bool Evaluate(const RelevanceContext&) const`
     * or another compatible callable member used by `EvaluateImpl`.
     *
     * Registration is idempotent. Re-registering an already-known type leaves the
     * original metadata in place.
     *
     * @note This function does not create or own policy instances.
     */
    template<typename PolicyT>
    static void Register()
    {
        const TypeId PolicyId = StaticTypeId<PolicyT>();
        GameLockGuard Lock(m_mutex);
        if (m_policies.find(PolicyId) != m_policies.end())
        {
            return;
        }
        m_policies.emplace(PolicyId, PolicyInfo{&EvaluateImpl<PolicyT>});
    }

    /**
     * @brief Look up dispatch metadata for a previously registered policy type.
     * @param PolicyId Reflected policy type id.
     * @return Pointer to registry-owned metadata, or `nullptr` when the type has not
     *         been registered.
     *
     * The returned pointer is borrowed and remains valid until static shutdown.
     */
    static const PolicyInfo* Find(const TypeId& PolicyId)
    {
        GameLockGuard Lock(m_mutex);
        auto It = m_policies.find(PolicyId);
        if (It == m_policies.end())
        {
            return nullptr;
        }
        return &It->second;
    }

private:
    /**
     * @brief Type-specific trampoline used by the registry to erase policy storage.
     * @tparam PolicyT Policy type.
     * @param PolicyData Borrowed pointer to a stored `PolicyT` instance.
     * @param Context Borrowed evaluation context.
     * @return `true` when `PolicyT::Evaluate(Context)` reports the node as relevant.
     */
    template<typename PolicyT>
    static bool EvaluateImpl(const void* PolicyData, const RelevanceContext& Context)
    {
        const auto* Typed = static_cast<const PolicyT*>(PolicyData);
        return Typed->Evaluate(Context);
    }

    static inline GameMutex m_mutex{}; /**< @brief Protects policy map. */
    static inline std::unordered_map<TypeId, PolicyInfo, UuidHash> m_policies{}; /**< @brief Policy map by TypeId. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Component that stores per-node relevance policy state and the latest evaluation result.
 *
 * A `RelevanceComponent` turns arbitrary policy data into something the level can
 * evaluate uniformly. The component owns an erased policy payload plus two cached
 * outputs:
 * - whether the node is currently considered active/relevant
 * - the last score produced by the broader relevance pass
 *
 * Why it exists:
 * - policy structs stay plain data types instead of polymorphic heap hierarchies
 * - node storage can keep one uniform component type
 * - evaluation code can dispatch through `RelevancePolicyRegistry`
 *
 * Ownership and lifetime:
 * - The component owns the current policy payload through `std::shared_ptr<void>`.
 * - Replacing the policy releases the previous payload when no longer referenced.
 * - Returned policy data from `PolicyData()` is borrowed and type-erased.
 *
 * Threading:
 * - Main-thread only.
 * - Mutating the policy while a relevance pass is in progress is not supported.
 *
 * Invariants:
 * - `m_policyId` is meaningful only when `m_policyData` holds a matching payload.
 * - `Active()` and `LastScore()` are cache fields; they do not trigger evaluation.
 *
 * @see RelevancePolicyRegistry
 */
SnType()
class RelevanceComponent : public BaseComponent, public ComponentCRTP<RelevanceComponent>
{
public:
    /** @brief Stable type name for reflection. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::RelevanceComponent";

    /**
     * @brief Replace the stored relevance policy payload with a new concrete policy value.
     * @tparam PolicyT Policy type.
     * @param Policy Policy value to copy or move into component-owned storage.
     *
     * Semantics:
     * - Ensures `PolicyT` is registered in `RelevancePolicyRegistry`.
     * - Replaces any previously stored policy object.
     * - Updates `PolicyId()` to the reflected id for `PolicyT`.
     *
     * Ownership:
     * - Ownership of the stored instance transfers into the component's internal
     *   shared payload.
     */
    template<typename PolicyT>
    void Policy(PolicyT Policy)
    {
        RelevancePolicyRegistry::Register<PolicyT>();
        m_policyId = StaticTypeId<PolicyT>();
        m_policyData = std::shared_ptr<void>(new PolicyT(std::move(Policy)), [](void* Ptr) { delete static_cast<PolicyT*>(Ptr); });
    }

    /**
     * @brief Get the reflected type id of the currently stored policy payload.
     * @return Borrowed reference to the stored policy type id.
     *
     * Returns the nil/default `TypeId` when no policy has been configured yet.
     */
    const TypeId& PolicyId() const
    {
        return m_policyId;
    }

    /**
     * @brief Access the owned, type-erased policy payload.
     * @return Borrowed reference to the internal shared payload.
     *
     * The pointer is intentionally type-erased. Callers are expected to pair this with
     * `PolicyId()` and `RelevancePolicyRegistry::Find()` rather than static-casting it
     * blindly.
     */
    const std::shared_ptr<void>& PolicyData() const
    {
        return m_policyData;
    }

    /**
     * @brief Read the most recently applied relevance-active flag.
     * @return `true` when the last relevance pass marked this node active.
     */
    bool Active() const
    {
        return m_active;
    }

    /**
     * @brief Store the most recently computed relevance-active flag.
     * @param Active New cached active state.
     *
     * This is a passive cache write. It does not itself evaluate the policy.
     */
    void Active(bool Active)
    {
        m_active = Active;
    }

    /**
     * @brief Read the last score written by the relevance system.
     * @return Cached score value.
     */
    float LastScore() const
    {
        return m_lastScore;
    }

    /**
     * @brief Store the score produced by the latest relevance evaluation.
     * @param Score Cached score value.
     */
    void LastScore(float Score)
    {
        m_lastScore = Score;
    }

private:
    TypeId m_policyId{}; /**< @brief Reflected type id of current policy object. */
    std::shared_ptr<void> m_policyData{}; /**< @brief Owned type-erased policy instance payload. */
    bool m_active = true; /**< @brief Last computed relevance active state applied to node gating. */
    float m_lastScore = 1.0f; /**< @brief Last computed score used for diagnostics/future prioritization. */
};

} // namespace SnAPI::GameFramework
