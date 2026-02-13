#include "PhysicsSystem.h"

#if defined(SNAPI_GF_ENABLE_PHYSICS)

#include <algorithm>
#include <array>
#include <utility>

namespace SnAPI::GameFramework
{

PhysicsSystem::PhysicsSystem(PhysicsSystem&& Other) noexcept
{
    std::lock_guard<std::mutex> Lock(Other.m_mutex);
    m_scene = std::move(Other.m_scene);
    m_settings = std::move(Other.m_settings);
    m_pendingEvents = std::move(Other.m_pendingEvents);
    m_eventListeners = std::move(Other.m_eventListeners);
    m_nextEventListenerToken = Other.m_nextEventListenerToken;
    m_bodySleepListeners = std::move(Other.m_bodySleepListeners);
    m_bodySleepListenerTokensByBody = std::move(Other.m_bodySleepListenerTokensByBody);
    m_nextBodySleepListenerToken = Other.m_nextBodySleepListenerToken;
}

PhysicsSystem& PhysicsSystem::operator=(PhysicsSystem&& Other) noexcept
{
    if (this == &Other)
    {
        return *this;
    }

    std::scoped_lock Lock(m_mutex, Other.m_mutex);
    m_scene = std::move(Other.m_scene);
    m_settings = std::move(Other.m_settings);
    m_pendingEvents = std::move(Other.m_pendingEvents);
    m_eventListeners = std::move(Other.m_eventListeners);
    m_nextEventListenerToken = Other.m_nextEventListenerToken;
    m_bodySleepListeners = std::move(Other.m_bodySleepListeners);
    m_bodySleepListenerTokensByBody = std::move(Other.m_bodySleepListenerTokensByBody);
    m_nextBodySleepListenerToken = Other.m_nextBodySleepListenerToken;
    return *this;
}

Error PhysicsSystem::MapPhysicsError(const SnAPI::Physics::Error& ErrorValue)
{
    switch (ErrorValue.Code)
    {
    case SnAPI::Physics::EErrorCode::InvalidArgument:
        return MakeError(EErrorCode::InvalidArgument, ErrorValue.Message);
    case SnAPI::Physics::EErrorCode::NotFound:
        return MakeError(EErrorCode::NotFound, ErrorValue.Message);
    case SnAPI::Physics::EErrorCode::AlreadyExists:
        return MakeError(EErrorCode::AlreadyExists, ErrorValue.Message);
    case SnAPI::Physics::EErrorCode::NotInitialized:
        return MakeError(EErrorCode::NotReady, ErrorValue.Message);
    case SnAPI::Physics::EErrorCode::NotSupported:
        return MakeError(EErrorCode::NotReady, ErrorValue.Message);
    case SnAPI::Physics::EErrorCode::BackendError:
    case SnAPI::Physics::EErrorCode::InternalError:
    default:
        return MakeError(EErrorCode::InternalError, ErrorValue.Message);
    }
}

Result PhysicsSystem::Initialize(const PhysicsBootstrapSettings& Settings)
{
    std::lock_guard<std::mutex> Lock(m_mutex);

    m_scene.reset();
    m_settings = Settings;
    m_pendingEvents.clear();
    m_eventListeners.clear();
    m_nextEventListenerToken = 1;
    m_bodySleepListeners.clear();
    m_bodySleepListenerTokensByBody.clear();
    m_nextBodySleepListenerToken = 1;

    auto RegisterResult = SnAPI::Physics::RegisterJoltBackend(m_runtime.Registry());
    if (!RegisterResult && RegisterResult.error().Code != SnAPI::Physics::EErrorCode::AlreadyExists)
    {
        return std::unexpected(MapPhysicsError(RegisterResult.error()));
    }

    auto SceneResult = m_runtime.CreateScene(m_settings.Scene, m_settings.Routing, m_settings.Couplings);
    if (!SceneResult)
    {
        return std::unexpected(MapPhysicsError(SceneResult.error()));
    }

    m_scene = std::move(SceneResult.value());
    return Ok();
}

void PhysicsSystem::Shutdown()
{
    std::lock_guard<std::mutex> Lock(m_mutex);
    m_scene.reset();
    m_pendingEvents.clear();
    m_eventListeners.clear();
    m_nextEventListenerToken = 1;
    m_bodySleepListeners.clear();
    m_bodySleepListenerTokensByBody.clear();
    m_nextBodySleepListenerToken = 1;
}

bool PhysicsSystem::IsInitialized() const
{
    std::lock_guard<std::mutex> Lock(m_mutex);
    return static_cast<bool>(m_scene);
}

Result PhysicsSystem::Step(float DeltaSeconds)
{
    if (DeltaSeconds <= 0.0f)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "DeltaSeconds must be > 0"));
    }

    std::vector<SnAPI::Physics::PhysicsEvent> StepEvents{};
    std::vector<PhysicsEventListener> Listeners{};
    struct PendingBodySleepDispatch
    {
        std::size_t EventIndex = 0;
        BodySleepListener Listener{};
    };
    std::vector<PendingBodySleepDispatch> BodySleepDispatches{};
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        if (!m_scene)
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "Physics scene is not initialized"));
        }

        auto SimulateResult = m_scene->Simulate(DeltaSeconds);
        if (!SimulateResult)
        {
            return std::unexpected(MapPhysicsError(SimulateResult.error()));
        }

        auto FetchResult = m_scene->FetchResults();
        if (!FetchResult)
        {
            return std::unexpected(MapPhysicsError(FetchResult.error()));
        }

        std::array<SnAPI::Physics::PhysicsEvent, 256> Buffer{};
        while (true)
        {
            const std::uint32_t Count = m_scene->DrainEvents(std::span<SnAPI::Physics::PhysicsEvent>(Buffer.data(), Buffer.size()));
            if (Count == 0)
            {
                break;
            }

            const std::size_t Begin = StepEvents.size();
            StepEvents.resize(Begin + Count);
            for (std::uint32_t Index = 0; Index < Count; ++Index)
            {
                StepEvents[Begin + Index] = Buffer[Index];
            }
        }

        m_pendingEvents.insert(m_pendingEvents.end(), StepEvents.begin(), StepEvents.end());
        Listeners.reserve(m_eventListeners.size());
        for (const auto& [Token, Listener] : m_eventListeners)
        {
            (void)Token;
            Listeners.push_back(Listener);
        }

        const auto QueueBodySleepDispatches = [this, &BodySleepDispatches](const std::size_t EventIndex,
                                                                           const SnAPI::Physics::BodyHandle BodyHandle) {
            if (!BodyHandle.IsValid())
            {
                return;
            }

            const auto TokensIt = m_bodySleepListenerTokensByBody.find(BodyHandle.Value());
            if (TokensIt == m_bodySleepListenerTokensByBody.end())
            {
                return;
            }

            const std::vector<BodySleepListenerToken>& Tokens = TokensIt->second;
            for (const BodySleepListenerToken Token : Tokens)
            {
                const auto ListenerIt = m_bodySleepListeners.find(Token);
                if (ListenerIt == m_bodySleepListeners.end() || !ListenerIt->second.Listener)
                {
                    continue;
                }

                BodySleepDispatches.push_back(PendingBodySleepDispatch{EventIndex, ListenerIt->second.Listener});
            }
        };

        for (std::size_t EventIndex = 0; EventIndex < StepEvents.size(); ++EventIndex)
        {
            const SnAPI::Physics::PhysicsEvent& Event = StepEvents[EventIndex];
            if (Event.Type != SnAPI::Physics::EPhysicsEventType::BodySleep
                && Event.Type != SnAPI::Physics::EPhysicsEventType::BodyWake)
            {
                continue;
            }

            QueueBodySleepDispatches(EventIndex, Event.BodyA);
            if (Event.BodyB.IsValid() && Event.BodyB != Event.BodyA)
            {
                QueueBodySleepDispatches(EventIndex, Event.BodyB);
            }
        }
    }

    for (const auto& Event : StepEvents)
    {
        for (const auto& Listener : Listeners)
        {
            Listener(Event);
        }
    }

    for (const PendingBodySleepDispatch& Dispatch : BodySleepDispatches)
    {
        Dispatch.Listener(StepEvents[Dispatch.EventIndex]);
    }

    return Ok();
}

std::uint32_t PhysicsSystem::DrainEvents(std::span<SnAPI::Physics::PhysicsEvent> OutEvents)
{
    std::lock_guard<std::mutex> Lock(m_mutex);
    const std::size_t Count = std::min<std::size_t>(OutEvents.size(), m_pendingEvents.size());
    for (std::size_t Index = 0; Index < Count; ++Index)
    {
        OutEvents[Index] = m_pendingEvents[Index];
    }

    if (Count > 0)
    {
        m_pendingEvents.erase(m_pendingEvents.begin(), m_pendingEvents.begin() + static_cast<std::size_t>(Count));
    }

    return static_cast<std::uint32_t>(Count);
}

PhysicsSystem::PhysicsEventListenerToken PhysicsSystem::AddEventListener(PhysicsEventListener Listener)
{
    std::lock_guard<std::mutex> Lock(m_mutex);
    const PhysicsEventListenerToken Token = m_nextEventListenerToken++;
    m_eventListeners.emplace(Token, std::move(Listener));
    return Token;
}

bool PhysicsSystem::RemoveEventListener(const PhysicsEventListenerToken Token)
{
    std::lock_guard<std::mutex> Lock(m_mutex);
    return m_eventListeners.erase(Token) > 0;
}

PhysicsSystem::BodySleepListenerToken PhysicsSystem::AddBodySleepListener(const SnAPI::Physics::BodyHandle BodyHandle,
                                                                          BodySleepListener Listener)
{
    if (!BodyHandle.IsValid() || !Listener)
    {
        return 0;
    }

    std::lock_guard<std::mutex> Lock(m_mutex);
    const BodySleepListenerToken Token = m_nextBodySleepListenerToken++;
    m_bodySleepListeners.emplace(Token, BodySleepListenerEntry{BodyHandle.Value(), std::move(Listener)});
    m_bodySleepListenerTokensByBody[BodyHandle.Value()].push_back(Token);
    return Token;
}

bool PhysicsSystem::RemoveBodySleepListener(const BodySleepListenerToken Token)
{
    std::lock_guard<std::mutex> Lock(m_mutex);
    const auto EntryIt = m_bodySleepListeners.find(Token);
    if (EntryIt == m_bodySleepListeners.end())
    {
        return false;
    }

    const std::uint64_t BodyHandleValue = EntryIt->second.BodyHandleValue;
    m_bodySleepListeners.erase(EntryIt);

    const auto TokensIt = m_bodySleepListenerTokensByBody.find(BodyHandleValue);
    if (TokensIt != m_bodySleepListenerTokensByBody.end())
    {
        auto& Tokens = TokensIt->second;
        Tokens.erase(std::remove(Tokens.begin(), Tokens.end(), Token), Tokens.end());
        if (Tokens.empty())
        {
            m_bodySleepListenerTokensByBody.erase(TokensIt);
        }
    }

    return true;
}

SnAPI::Physics::IPhysicsScene* PhysicsSystem::Scene()
{
    std::lock_guard<std::mutex> Lock(m_mutex);
    return m_scene.get();
}

const SnAPI::Physics::IPhysicsScene* PhysicsSystem::Scene() const
{
    std::lock_guard<std::mutex> Lock(m_mutex);
    return m_scene.get();
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_PHYSICS
