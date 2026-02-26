#include "InputIntentComponent.h"

#include <cmath>

namespace SnAPI::GameFramework
{
namespace
{
[[nodiscard]] bool IsFiniteFloat(const float Value)
{
    return std::isfinite(Value);
}

[[nodiscard]] bool IsFiniteVec3(const Vec3& Value)
{
    return std::isfinite(Value.x()) && std::isfinite(Value.y()) && std::isfinite(Value.z());
}
} // namespace

void InputIntentComponent::SetMoveWorldInput(const Vec3& Input)
{
    m_moveWorldInput = IsFiniteVec3(Input) ? Input : Vec3::Zero();
}

void InputIntentComponent::AddMoveWorldInput(const Vec3& Input)
{
    if (!IsFiniteVec3(Input))
    {
        return;
    }

    m_moveWorldInput += Input;
}

const Vec3& InputIntentComponent::MoveWorldInput() const
{
    return m_moveWorldInput;
}

void InputIntentComponent::ClearMoveWorldInput()
{
    m_moveWorldInput = Vec3::Zero();
}

void InputIntentComponent::QueueJump()
{
    m_jumpRequested = true;
}

void InputIntentComponent::SetJumpRequested(const bool Requested)
{
    m_jumpRequested = Requested;
}

bool InputIntentComponent::JumpRequested() const
{
    return m_jumpRequested;
}

bool InputIntentComponent::ConsumeJumpRequested()
{
    const bool Requested = m_jumpRequested;
    m_jumpRequested = false;
    return Requested;
}

void InputIntentComponent::SetLookInput(const float YawDeltaDegrees, const float PitchDeltaDegrees)
{
    m_lookYawDeltaDegrees = IsFiniteFloat(YawDeltaDegrees) ? YawDeltaDegrees : 0.0f;
    m_lookPitchDeltaDegrees = IsFiniteFloat(PitchDeltaDegrees) ? PitchDeltaDegrees : 0.0f;
}

void InputIntentComponent::AddLookInput(const float YawDeltaDegrees, const float PitchDeltaDegrees)
{
    if (!IsFiniteFloat(YawDeltaDegrees) || !IsFiniteFloat(PitchDeltaDegrees))
    {
        return;
    }

    m_lookYawDeltaDegrees += YawDeltaDegrees;
    m_lookPitchDeltaDegrees += PitchDeltaDegrees;
}

void InputIntentComponent::ConsumeLookInput(float& OutYawDeltaDegrees, float& OutPitchDeltaDegrees)
{
    OutYawDeltaDegrees = m_lookYawDeltaDegrees;
    OutPitchDeltaDegrees = m_lookPitchDeltaDegrees;
    m_lookYawDeltaDegrees = 0.0f;
    m_lookPitchDeltaDegrees = 0.0f;
}

void InputIntentComponent::ClearLookInput()
{
    m_lookYawDeltaDegrees = 0.0f;
    m_lookPitchDeltaDegrees = 0.0f;
}

void InputIntentComponent::ResetIntents()
{
    m_moveWorldInput = Vec3::Zero();
    m_jumpRequested = false;
    m_lookYawDeltaDegrees = 0.0f;
    m_lookPitchDeltaDegrees = 0.0f;
}

} // namespace SnAPI::GameFramework

