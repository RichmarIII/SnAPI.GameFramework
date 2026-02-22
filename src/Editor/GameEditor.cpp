#include "Editor/GameEditor.h"

#include "Editor/EditorCoreServices.h"
#include "Editor/EditorWorld.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace SnAPI::GameFramework::Editor
{

Result GameEditor::Initialize(const GameEditorSettings& Settings)
{
    Shutdown();
    m_settings = Settings;

    if (auto InitRuntime = InitializeRuntime(m_settings); !InitRuntime)
    {
        return InitRuntime;
    }

    if (auto InitEditor = InitializeEditorModules(); !InitEditor)
    {
        Shutdown();
        return InitEditor;
    }

    m_initialized = true;
    return Ok();
}

void GameEditor::Shutdown()
{
    if (m_runtime.IsInitialized())
    {
        ShutdownEditorModules();
    }

    m_runtime.Shutdown();
    m_initialized = false;
}

bool GameEditor::IsInitialized() const
{
    return m_initialized && m_runtime.IsInitialized();
}

bool GameEditor::Update(const float DeltaSeconds)
{
    if (!IsInitialized())
    {
        return false;
    }

    TickServices(DeltaSeconds);

    return m_runtime.Update(DeltaSeconds);
}

GameRuntime& GameEditor::Runtime()
{
    return m_runtime;
}

const GameRuntime& GameEditor::Runtime() const
{
    return m_runtime;
}

const GameEditorSettings& GameEditor::Settings() const
{
    return m_settings;
}

Result GameEditor::RegisterService(std::unique_ptr<IEditorService> Service)
{
    if (!Service)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Service instance must not be null"));
    }

    const std::type_index ServiceType = std::type_index(typeid(*Service));
    if (m_serviceIndexByType.contains(ServiceType))
    {
        return Ok();
    }

    ServiceEntry Entry{};
    Entry.Type = ServiceType;
    Entry.Instance = std::move(Service);
    Entry.Initialized = false;

    const std::size_t NewIndex = m_services.size();
    m_services.emplace_back(std::move(Entry));
    m_serviceIndexByType.emplace(ServiceType, NewIndex);

    if (!IsInitialized())
    {
        return Ok();
    }

    if (const Result InitResult = InitializeServices(); !InitResult)
    {
        m_serviceIndexByType.erase(ServiceType);
        m_services.pop_back();
        (void)BuildServiceOrder();
        return InitResult;
    }

    return Ok();
}

Result GameEditor::UnregisterService(const std::type_index& ServiceType)
{
    const auto ServiceIt = m_serviceIndexByType.find(ServiceType);
    if (ServiceIt == m_serviceIndexByType.end())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Editor service was not found"));
    }

    const std::size_t ServiceCount = m_services.size();
    std::vector<bool> RemoveMask(ServiceCount, false);
    std::vector<std::size_t> PendingIndices{ServiceIt->second};
    while (!PendingIndices.empty())
    {
        const std::size_t Current = PendingIndices.back();
        PendingIndices.pop_back();
        if (Current >= ServiceCount || RemoveMask[Current])
        {
            continue;
        }

        RemoveMask[Current] = true;
        const std::type_index CurrentType = m_services[Current].Type;

        for (std::size_t Candidate = 0; Candidate < ServiceCount; ++Candidate)
        {
            if (RemoveMask[Candidate] || !m_services[Candidate].Instance)
            {
                continue;
            }

            const auto Dependencies = m_services[Candidate].Instance->Dependencies();
            if (std::find(Dependencies.begin(), Dependencies.end(), CurrentType) == Dependencies.end())
            {
                continue;
            }

            PendingIndices.push_back(Candidate);
        }
    }

    if (IsInitialized())
    {
        EditorServiceContext Context(*this);
        for (auto ReverseIt = m_serviceOrder.rbegin(); ReverseIt != m_serviceOrder.rend(); ++ReverseIt)
        {
            const std::size_t ServiceIndex = *ReverseIt;
            if (ServiceIndex >= ServiceCount || !RemoveMask[ServiceIndex])
            {
                continue;
            }

            ServiceEntry& Entry = m_services[ServiceIndex];
            if (Entry.Initialized && Entry.Instance)
            {
                Entry.Instance->Shutdown(Context);
                Entry.Initialized = false;
            }
        }
    }

    std::vector<ServiceEntry> Remaining{};
    Remaining.reserve(ServiceCount);
    for (std::size_t Index = 0; Index < ServiceCount; ++Index)
    {
        if (!RemoveMask[Index])
        {
            Remaining.emplace_back(std::move(m_services[Index]));
        }
    }

    m_services = std::move(Remaining);
    RebuildServiceIndexByType();
    m_serviceOrder.clear();

    if (const Result RebuildOrderResult = BuildServiceOrder(); !RebuildOrderResult)
    {
        return RebuildOrderResult;
    }

    if (IsInitialized())
    {
        if (const Result InitResult = InitializeServices(); !InitResult)
        {
            return InitResult;
        }
    }

    return Ok();
}

Result GameEditor::InitializeRuntime(const GameEditorSettings& Settings)
{
    GameRuntimeSettings EffectiveSettings = Settings.Runtime;

    if (!EffectiveSettings.WorldFactory)
    {
        EffectiveSettings.WorldFactory = [](std::string Name) -> std::unique_ptr<SnAPI::GameFramework::World> {
            return std::make_unique<EditorWorld>(std::move(Name));
        };
    }

#if defined(SNAPI_GF_ENABLE_INPUT) && defined(SNAPI_GF_ENABLE_UI)
    // Editor UX requires pointer/keyboard routing into UI by default.
    if (EffectiveSettings.UI.has_value() && !EffectiveSettings.Input.has_value())
    {
        EffectiveSettings.Input = GameRuntimeInputSettings{};
    }
#endif

    return m_runtime.Init(EffectiveSettings);
}

Result GameEditor::InitializeEditorModules()
{
    EnsureDefaultServicesRegistered();
    return InitializeServices();
}

void GameEditor::ShutdownEditorModules()
{
    ShutdownServices();
}

void GameEditor::EnsureDefaultServicesRegistered()
{
    if (m_defaultServicesRegistered)
    {
        return;
    }

#if defined(SNAPI_GF_ENABLE_RENDERER) && defined(SNAPI_GF_ENABLE_UI)
    (void)RegisterService<EditorCommandService>();
    (void)RegisterService<EditorThemeService>();
    (void)RegisterService<EditorSceneService>();
    (void)RegisterService<EditorRootViewportService>();
    (void)RegisterService<EditorSelectionService>();
    (void)RegisterService<EditorLayoutService>();
    (void)RegisterService<EditorGameViewportOverlayService>();
    (void)RegisterService<EditorSelectionInteractionService>();
    (void)RegisterService<EditorTransformInteractionService>();
#endif

    m_defaultServicesRegistered = true;
}

Result GameEditor::BuildServiceOrder()
{
    m_serviceOrder.clear();
    if (m_services.empty())
    {
        return Ok();
    }

    const std::size_t ServiceCount = m_services.size();
    std::vector<bool> Resolved(ServiceCount, false);
    m_serviceOrder.reserve(ServiceCount);

    for (std::size_t ResolveCount = 0; ResolveCount < ServiceCount; ++ResolveCount)
    {
        std::size_t BestIndex = ServiceCount;
        int BestPriority = 0;

        for (std::size_t Index = 0; Index < ServiceCount; ++Index)
        {
            if (Resolved[Index] || !m_services[Index].Instance)
            {
                continue;
            }

            const auto Dependencies = m_services[Index].Instance->Dependencies();
            bool DependenciesReady = true;
            for (const std::type_index& DependencyType : Dependencies)
            {
                const auto DependencyIt = m_serviceIndexByType.find(DependencyType);
                if (DependencyIt == m_serviceIndexByType.end())
                {
                    const std::string Message = std::string("Service '") +
                                                std::string(m_services[Index].Instance->Name()) +
                                                "' is missing dependency '" + std::string(DependencyType.name()) + "'";
                    return std::unexpected(MakeError(EErrorCode::NotFound, Message));
                }

                if (!Resolved[DependencyIt->second])
                {
                    DependenciesReady = false;
                    break;
                }
            }

            if (!DependenciesReady)
            {
                continue;
            }

            const int CandidatePriority = m_services[Index].Instance->Priority();
            if (BestIndex == ServiceCount || CandidatePriority < BestPriority ||
                (CandidatePriority == BestPriority && Index < BestIndex))
            {
                BestIndex = Index;
                BestPriority = CandidatePriority;
            }
        }

        if (BestIndex == ServiceCount)
        {
            return std::unexpected(
                MakeError(EErrorCode::InternalError, "Circular or unsatisfied editor service dependencies"));
        }

        Resolved[BestIndex] = true;
        m_serviceOrder.push_back(BestIndex);
    }

    return Ok();
}

Result GameEditor::InitializeServices()
{
    if (const Result OrderResult = BuildServiceOrder(); !OrderResult)
    {
        return OrderResult;
    }

    EditorServiceContext Context(*this);
    std::vector<std::size_t> InitializedOrder{};
    InitializedOrder.reserve(m_serviceOrder.size());

    for (const std::size_t ServiceIndex : m_serviceOrder)
    {
        ServiceEntry& Entry = m_services[ServiceIndex];
        if (!Entry.Instance || Entry.Initialized)
        {
            continue;
        }

        if (const Result InitResult = Entry.Instance->Initialize(Context); !InitResult)
        {
            for (auto ReverseIt = InitializedOrder.rbegin(); ReverseIt != InitializedOrder.rend(); ++ReverseIt)
            {
                ServiceEntry& InitializedEntry = m_services[*ReverseIt];
                if (InitializedEntry.Instance && InitializedEntry.Initialized)
                {
                    InitializedEntry.Instance->Shutdown(Context);
                    InitializedEntry.Initialized = false;
                }
            }
            return InitResult;
        }

        Entry.Initialized = true;
        InitializedOrder.push_back(ServiceIndex);
    }

    return Ok();
}

void GameEditor::TickServices(const float DeltaSeconds)
{
    EditorServiceContext Context(*this);
    for (const std::size_t ServiceIndex : m_serviceOrder)
    {
        ServiceEntry& Entry = m_services[ServiceIndex];
        if (!Entry.Instance || !Entry.Initialized)
        {
            continue;
        }

        Entry.Instance->Tick(Context, DeltaSeconds);
    }
}

void GameEditor::ShutdownServices()
{
    EditorServiceContext Context(*this);
    for (auto ReverseIt = m_serviceOrder.rbegin(); ReverseIt != m_serviceOrder.rend(); ++ReverseIt)
    {
        ServiceEntry& Entry = m_services[*ReverseIt];
        if (!Entry.Instance || !Entry.Initialized)
        {
            continue;
        }

        Entry.Instance->Shutdown(Context);
        Entry.Initialized = false;
    }
}

void GameEditor::RebuildServiceIndexByType()
{
    m_serviceIndexByType.clear();
    for (std::size_t Index = 0; Index < m_services.size(); ++Index)
    {
        m_serviceIndexByType.emplace(m_services[Index].Type, Index);
    }
}

GameRuntime& GameEditor::RuntimeForServices()
{
    return m_runtime;
}

const GameRuntime& GameEditor::RuntimeForServices() const
{
    return m_runtime;
}

IEditorService* GameEditor::ResolveServiceForContext(const std::type_index& Type)
{
    const auto It = m_serviceIndexByType.find(Type);
    if (It == m_serviceIndexByType.end())
    {
        return nullptr;
    }

    return m_services[It->second].Instance.get();
}

const IEditorService* GameEditor::ResolveServiceForContext(const std::type_index& Type) const
{
    const auto It = m_serviceIndexByType.find(Type);
    if (It == m_serviceIndexByType.end())
    {
        return nullptr;
    }

    return m_services[It->second].Instance.get();
}

} // namespace SnAPI::GameFramework::Editor
