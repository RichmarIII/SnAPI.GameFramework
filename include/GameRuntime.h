#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "Expected.h"
#include "World.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Tick/lifecycle policy for `GameRuntime::Update`.
 * @remarks
 * `Update` always performs variable-step `World::Tick`.
 * Optional phases can be enabled for deterministic and post-frame work.
 */
struct GameRuntimeTickSettings
{
    bool EnableFixedTick = false; /**< @brief Execute fixed-step ticks from accumulator time. */
    float FixedDeltaSeconds = 1.0f / 60.0f; /**< @brief Fixed-step interval used when `EnableFixedTick` is true. */
    std::size_t MaxFixedStepsPerUpdate = 8; /**< @brief Safety cap to avoid spiral-of-death under long frames. */
    bool EnableLateTick = true; /**< @brief Execute `World::LateTick` each update. */
    bool EnableEndFrame = true; /**< @brief Execute `World::EndFrame` each update. */
    float MaxFpsWhenVSyncOff = 0.0f; /**< @brief Optional frame cap applied only while renderer VSync mode is `Off`; `<= 0` disables cap. */
};

#if defined(SNAPI_GF_ENABLE_NETWORKING)
using GameRuntimeNetworkingSettings = NetworkBootstrapSettings;
#endif
#if defined(SNAPI_GF_ENABLE_INPUT)
using GameRuntimeInputSettings = InputBootstrapSettings;
#endif
#if defined(SNAPI_GF_ENABLE_PHYSICS)
using GameRuntimePhysicsSettings = PhysicsBootstrapSettings;
#endif
#if defined(SNAPI_GF_ENABLE_RENDERER)
using GameRuntimeRendererSettings = RendererBootstrapSettings;
#endif

/**
 * @brief High-level runtime settings for bootstrap and update policy.
 */
struct GameRuntimeSettings
{
    std::string WorldName = "World"; /**< @brief Name assigned to the created world instance. */
    bool RegisterBuiltins = true; /**< @brief Register built-in reflection/serialization types once during init. */
    GameRuntimeTickSettings Tick{}; /**< @brief Tick/lifecycle policy for `Update`. */
#if defined(SNAPI_GF_ENABLE_INPUT)
    std::optional<GameRuntimeInputSettings> Input{}; /**< @brief Optional input bootstrap; nullopt keeps world input subsystem uninitialized. */
#endif
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    std::optional<GameRuntimeNetworkingSettings> Networking{}; /**< @brief Optional networking bootstrap; nullopt = offline/local runtime. */
#endif
#if defined(SNAPI_GF_ENABLE_PHYSICS)
    std::optional<GameRuntimePhysicsSettings> Physics{}; /**< @brief Optional physics bootstrap; nullopt = no world physics scene. */
#endif
#if defined(SNAPI_GF_ENABLE_RENDERER)
    std::optional<GameRuntimeRendererSettings> Renderer{}; /**< @brief Optional renderer bootstrap; nullopt = no world renderer backend. */
#endif
};

/**
 * @brief World runtime host that centralizes bootstrap and per-frame orchestration.
 * @remarks
 * Primary goal: remove boilerplate from apps/examples by providing:
 * - `Init(Settings)` for world + optional network/session setup
 * - `Update(DeltaSeconds)` for frame orchestration (pump/tick/end-frame)
 *
 * Ownership:
 * - owns `World`
 * - world-owned `InputSystem` owns input runtime/context when enabled
 * - world-owned `NetworkSystem` owns networking resources when enabled
 */
class GameRuntime final
{
public:
    /**
     * @brief Initialize runtime from settings.
     * @param Settings Runtime settings.
     * @return Success or error.
     * @remarks Calling `Init` resets previous runtime state first.
     */
    Result Init(const GameRuntimeSettings& Settings);

    /**
     * @brief Shutdown runtime and release world/network resources.
     */
    void Shutdown();

    /**
     * @brief Check if runtime currently owns a valid world.
     */
    bool IsInitialized() const;

    /**
     * @brief Run one application frame.
     * @param DeltaSeconds Frame delta time.
     * @remarks
     * Update order:
     * 1. optional fixed ticks (accumulator)
     * 2. variable tick
     * 3. optional late tick
     * 4. optional end-frame
     * 5. optional frame pacing (max-FPS cap while VSync is off)
     *
     * Input and network session pumping are handled by `World` lifecycle methods,
     * not by `GameRuntime`.
     */
    void Update(float DeltaSeconds);

    /**
     * @brief Get mutable world pointer.
     * @return World pointer or nullptr when not initialized.
     */
    class World* WorldPtr();
    /**
     * @brief Get const world pointer.
     * @return World pointer or nullptr when not initialized.
     */
    const class World* WorldPtr() const;

    /**
     * @brief Get mutable world reference.
     * @return World reference.
     * @remarks Debug-asserts when runtime is not initialized.
     */
    class World& World();
    /**
     * @brief Get const world reference.
     * @return World reference.
     * @remarks Debug-asserts when runtime is not initialized.
     */
    const class World& World() const;

    /**
     * @brief Access current runtime settings snapshot.
     */
    const GameRuntimeSettings& Settings() const;

private:
    using FrameClock = std::chrono::steady_clock;

    /**
     * @brief Apply end-of-frame pacing for max-FPS limiting.
     * @param FrameStart Runtime update start timestamp.
     * @remarks
     * Uses absolute deadline scheduling to keep frame spacing even when runtime
     * work cost varies slightly frame-to-frame.
     */
    void ApplyFramePacing(FrameClock::time_point FrameStart);

    /**
     * @brief Check whether frame pacing cap should run this frame.
     * @return True when `MaxFpsWhenVSyncOff` is configured and VSync is currently off.
     */
    bool ShouldCapFrameRate() const;

    static void EnsureBuiltinTypesRegistered();

    GameRuntimeSettings m_settings{}; /**< @brief Last initialization settings snapshot. */
    std::unique_ptr<class World> m_world{}; /**< @brief Owned runtime world instance. */
    float m_fixedAccumulator = 0.0f; /**< @brief Accumulated fixed-step time. */
    FrameClock::duration m_framePacerStep{}; /**< @brief Current pacing step duration derived from max-FPS setting. */
    FrameClock::time_point m_nextFrameDeadline{}; /**< @brief Next target frame-present deadline used by runtime frame pacer. */
    bool m_framePacerArmed = false; /**< @brief True once pacing deadline baseline has been initialized. */
};

} // namespace SnAPI::GameFramework
