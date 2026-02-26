#pragma once

#include "BaseComponent.h"
#include "Math.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Shared intent bus for pawn-style movement and view input.
 * @remarks
 * Producers (for example `InputComponent`) write intents here, while
 * consumers (for example movement/camera controllers) pull or consume them.
 *
 * This component is intentionally transient runtime state and is not replicated.
 */
class InputIntentComponent : public BaseComponent, public ComponentCRTP<InputIntentComponent>
{
public:
    /** @brief Stable type name for reflection/serialization registration. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::InputIntentComponent";

    /** @brief Replace current world-space movement intent. */
    void SetMoveWorldInput(const Vec3& Input);
    /** @brief Add world-space movement intent. */
    void AddMoveWorldInput(const Vec3& Input);
    /** @brief Access current world-space movement intent. */
    const Vec3& MoveWorldInput() const;
    /** @brief Clear movement intent to zero. */
    void ClearMoveWorldInput();

    /** @brief Latch a jump request until consumed by a consumer. */
    void QueueJump();
    /** @brief Overwrite jump-request state. */
    void SetJumpRequested(bool Requested);
    /** @brief Read jump-request state without clearing. */
    bool JumpRequested() const;
    /** @brief Read and clear jump-request state. */
    bool ConsumeJumpRequested();

    /** @brief Replace current look delta intent (degrees). */
    void SetLookInput(float YawDeltaDegrees, float PitchDeltaDegrees);
    /** @brief Accumulate look delta intent (degrees). */
    void AddLookInput(float YawDeltaDegrees, float PitchDeltaDegrees);
    /** @brief Read and clear look delta intent (degrees). */
    void ConsumeLookInput(float& OutYawDeltaDegrees, float& OutPitchDeltaDegrees);
    /** @brief Clear look intent to zero. */
    void ClearLookInput();

    /** @brief Clear all intents. */
    void ResetIntents();

private:
    Vec3 m_moveWorldInput = Vec3::Zero();
    bool m_jumpRequested = false;
    float m_lookYawDeltaDegrees = 0.0f;
    float m_lookPitchDeltaDegrees = 0.0f;
};

} // namespace SnAPI::GameFramework

