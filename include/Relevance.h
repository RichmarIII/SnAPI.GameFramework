#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "Expected.h"
#include "IComponent.h"
#include "StaticTypeId.h"
#include "TypeName.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

class NodeGraph;

/**
 * @brief Context passed to relevance policy evaluation.
 * @remarks Provides access to the node handle and owning graph.
 */
struct RelevanceContext
{
    NodeHandle Node; /**< @brief Node being evaluated. */
    std::reference_wrapper<NodeGraph> Graph; /**< @brief Owning graph. */
};

/**
 * @brief Registry for relevance policy types.
 * @remarks
 * Static process-wide registry that binds policy type ids to evaluate callbacks.
 * Relevance components store policy data + type id, while NodeGraph executes callbacks
 * during relevance evaluation passes.
 */
class RelevancePolicyRegistry
{
public:
    /**
     * @brief Signature for relevance evaluation callbacks.
     * @param PolicyData Pointer to policy instance.
     * @param Context Evaluation context.
     * @return True if the node is relevant/active.
     */
    using EvaluateFn = bool(*)(const void* PolicyData, const RelevanceContext& Context);

    /**
     * @brief Stored policy metadata.
     */
    struct PolicyInfo
    {
        EvaluateFn Evaluate = nullptr; /**< @brief Evaluation callback. */
    };

    /**
     * @brief Register a policy type.
     * @tparam PolicyT Policy type (must implement Evaluate).
     * @remarks Duplicate registrations are ignored to keep registration idempotent.
     */
    template<typename PolicyT>
    static void Register()
    {
        const TypeId PolicyId = StaticTypeId<PolicyT>();
        std::lock_guard<std::mutex> Lock(m_mutex);
        if (m_policies.find(PolicyId) != m_policies.end())
        {
            return;
        }
        m_policies.emplace(PolicyId, PolicyInfo{&EvaluateImpl<PolicyT>});
    }

    /**
     * @brief Find policy metadata by TypeId.
     * @param PolicyId Policy type id.
     * @return Pointer to PolicyInfo or nullptr.
     */
    static const PolicyInfo* Find(const TypeId& PolicyId)
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        auto It = m_policies.find(PolicyId);
        if (It == m_policies.end())
        {
            return nullptr;
        }
        return &It->second;
    }

private:
    /**
     * @brief Internal evaluation wrapper for PolicyT.
     * @tparam PolicyT Policy type.
     * @param PolicyData Pointer to policy instance.
     * @param Context Evaluation context.
     * @return True if relevant.
     */
    template<typename PolicyT>
    static bool EvaluateImpl(const void* PolicyData, const RelevanceContext& Context)
    {
        const auto* Typed = static_cast<const PolicyT*>(PolicyData);
        return Typed->Evaluate(Context);
    }

    static inline std::mutex m_mutex{}; /**< @brief Protects policy map. */
    static inline std::unordered_map<TypeId, PolicyInfo, UuidHash> m_policies{}; /**< @brief Policy map by TypeId. */
};

/**
 * @brief Component that drives relevance evaluation for a node.
 * @remarks
 * Holds type-erased policy instance and latest evaluation outputs.
 * NodeGraph relevance pass reads this component to decide node activation.
 */
class RelevanceComponent : public IComponent
{
public:
    /** @brief Stable type name for reflection. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::RelevanceComponent";

    /**
     * @brief Set the relevance policy for this component.
     * @tparam PolicyT Policy type.
     * @param Policy Policy instance to store.
     * @remarks Registers policy type metadata on first use and replaces existing policy instance.
     */
    template<typename PolicyT>
    void Policy(PolicyT Policy)
    {
        RelevancePolicyRegistry::Register<PolicyT>();
        m_policyId = StaticTypeId<PolicyT>();
        m_policyData = std::shared_ptr<void>(new PolicyT(std::move(Policy)), [](void* Ptr) { delete static_cast<PolicyT*>(Ptr); });
    }

    /**
     * @brief Get the policy type id.
     * @return TypeId of the policy.
     */
    const TypeId& PolicyId() const
    {
        return m_policyId;
    }

    /**
     * @brief Get the stored policy instance.
     * @return Shared pointer to the policy data.
     * @remarks The stored pointer is type-erased.
     */
    const std::shared_ptr<void>& PolicyData() const
    {
        return m_policyData;
    }

    /**
     * @brief Get the active state computed by relevance.
     * @return True if relevant.
     */
    bool Active() const
    {
        return m_active;
    }

    /**
     * @brief Set the active state computed by relevance.
     * @param Active New active state.
     */
    void Active(bool Active)
    {
        m_active = Active;
    }

    /**
     * @brief Get the last computed relevance score.
     * @return Score value.
     */
    float LastScore() const
    {
        return m_lastScore;
    }

    /**
     * @brief Set the last computed relevance score.
     * @param Score Score value.
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
