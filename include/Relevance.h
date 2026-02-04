#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "Expected.h"
#include "IComponent.h"
#include "TypeName.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

class NodeGraph;

struct RelevanceContext
{
    NodeHandle Node;
    std::reference_wrapper<NodeGraph> Graph;
};

class RelevancePolicyRegistry
{
public:
    using EvaluateFn = bool(*)(const void* PolicyData, const RelevanceContext& Context);

    struct PolicyInfo
    {
        EvaluateFn Evaluate = nullptr;
    };

    template<typename PolicyT>
    static void Register()
    {
        const TypeId PolicyId = TypeIdFromName(TTypeNameV<PolicyT>);
        std::lock_guard<std::mutex> Lock(m_mutex);
        if (m_policies.find(PolicyId) != m_policies.end())
        {
            return;
        }
        m_policies.emplace(PolicyId, PolicyInfo{&EvaluateImpl<PolicyT>});
    }

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
    template<typename PolicyT>
    static bool EvaluateImpl(const void* PolicyData, const RelevanceContext& Context)
    {
        const auto* Typed = static_cast<const PolicyT*>(PolicyData);
        return Typed->Evaluate(Context);
    }

    static inline std::mutex m_mutex{};
    static inline std::unordered_map<TypeId, PolicyInfo, UuidHash> m_policies{};
};

class RelevanceComponent : public IComponent
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::RelevanceComponent";

    template<typename PolicyT>
    void Policy(PolicyT Policy)
    {
        RelevancePolicyRegistry::Register<PolicyT>();
        m_policyId = TypeIdFromName(TTypeNameV<PolicyT>);
        m_policyData = std::shared_ptr<void>(new PolicyT(std::move(Policy)), [](void* Ptr) { delete static_cast<PolicyT*>(Ptr); });
    }

    const TypeId& PolicyId() const
    {
        return m_policyId;
    }

    const std::shared_ptr<void>& PolicyData() const
    {
        return m_policyData;
    }

    bool Active() const
    {
        return m_active;
    }

    void Active(bool Active)
    {
        m_active = Active;
    }

    float LastScore() const
    {
        return m_lastScore;
    }

    void LastScore(float Score)
    {
        m_lastScore = Score;
    }

private:
    TypeId m_policyId{};
    std::shared_ptr<void> m_policyData{};
    bool m_active = true;
    float m_lastScore = 1.0f;
};

} // namespace SnAPI::GameFramework
