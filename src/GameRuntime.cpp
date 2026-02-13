#include "GameRuntime.h"

#include "Assert.h"
#include "TypeRegistration.h"

#include <algorithm>
#include <cmath>
#include <mutex>

namespace SnAPI::GameFramework
{

Result GameRuntime::Init(const GameRuntimeSettings& Settings)
{
    Shutdown();

    m_settings = Settings;
    m_fixedAccumulator = 0.0f;

    if (m_settings.RegisterBuiltins)
    {
        EnsureBuiltinTypesRegistered();
    }

    std::string WorldName = m_settings.WorldName;
    if (WorldName.empty())
    {
        WorldName = "World";
    }
    m_world = std::make_unique<class World>(std::move(WorldName));

#if defined(SNAPI_GF_ENABLE_PHYSICS)
    if (m_settings.Physics)
    {
        auto InitPhysics = m_world->Physics().Initialize(*m_settings.Physics);
        if (!InitPhysics)
        {
            Shutdown();
            return std::unexpected(InitPhysics.error());
        }
    }
#endif

#if defined(SNAPI_GF_ENABLE_NETWORKING)
    if (m_settings.Networking)
    {
        auto InitNetwork = m_world->Networking().InitializeOwnedSession(*m_settings.Networking);
        if (!InitNetwork)
        {
            Shutdown();
            return std::unexpected(InitNetwork.error());
        }
    }
#endif

    return Ok();
}

void GameRuntime::Shutdown()
{
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    if (m_world)
    {
        m_world->Networking().ShutdownOwnedSession();
    }
#endif
#if defined(SNAPI_GF_ENABLE_PHYSICS)
    if (m_world)
    {
        m_world->Physics().Shutdown();
    }
#endif
    m_world.reset();
    m_fixedAccumulator = 0.0f;
}

bool GameRuntime::IsInitialized() const
{
    return static_cast<bool>(m_world);
}

void GameRuntime::Update(float DeltaSeconds)
{
    if (!m_world)
    {
        return;
    }

    const auto& TickSettings = m_settings.Tick;
    if (TickSettings.EnableFixedTick && TickSettings.FixedDeltaSeconds > 0.0f)
    {
        m_fixedAccumulator += std::max(0.0f, DeltaSeconds);
        const std::size_t MaxSteps = std::max<std::size_t>(1, TickSettings.MaxFixedStepsPerUpdate);

        std::size_t Steps = 0;
        while (m_fixedAccumulator >= TickSettings.FixedDeltaSeconds && Steps < MaxSteps)
        {
            m_world->FixedTick(TickSettings.FixedDeltaSeconds);
            m_fixedAccumulator -= TickSettings.FixedDeltaSeconds;
            ++Steps;
        }

        if (Steps == MaxSteps && m_fixedAccumulator >= TickSettings.FixedDeltaSeconds)
        {
            m_fixedAccumulator = std::fmod(m_fixedAccumulator, TickSettings.FixedDeltaSeconds);
        }
    }

    m_world->Tick(DeltaSeconds);

    if (TickSettings.EnableLateTick)
    {
        m_world->LateTick(DeltaSeconds);
    }

    if (TickSettings.EnableEndFrame)
    {
        m_world->EndFrame();
    }
}

World* GameRuntime::WorldPtr()
{
    return m_world.get();
}

const World* GameRuntime::WorldPtr() const
{
    return m_world.get();
}

World& GameRuntime::World()
{
    DEBUG_ASSERT(m_world != nullptr, "GameRuntime::World() called before Init()");
    return *m_world;
}

const World& GameRuntime::World() const
{
    DEBUG_ASSERT(m_world != nullptr, "GameRuntime::World() called before Init()");
    return *m_world;
}

const GameRuntimeSettings& GameRuntime::Settings() const
{
    return m_settings;
}

void GameRuntime::EnsureBuiltinTypesRegistered()
{
    static std::once_flag Once;
    std::call_once(Once, [] {
        RegisterBuiltinTypes();
    });
}

} // namespace SnAPI::GameFramework
