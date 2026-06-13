#pragma once

#include "Editor/EditorExport.h"
#include "Editor/IEditorService.h"
#include "Serialization.h"
#include "World.h"

#include <cstdint>
#include <optional>

namespace SnAPI::GameFramework::Editor
{

class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorPieService final : public IEditorService
{
public:
    enum class EState : std::uint8_t
    {
        Stopped = 0,
        Playing,
        Paused,
    };

    [[nodiscard]] std::string_view Name() const override;
    Result Initialize(EditorServiceContext& Context) override;
    void Shutdown(EditorServiceContext& Context) override;

    Result Play(EditorServiceContext& Context);
    Result Pause(EditorServiceContext& Context);
    Result Stop(EditorServiceContext& Context);

    [[nodiscard]] EState State() const { return m_state; }
    [[nodiscard]] bool IsPlaying() const { return m_state == EState::Playing; }
    [[nodiscard]] bool IsPaused() const { return m_state == EState::Paused; }
    [[nodiscard]] bool IsSessionActive() const { return m_state != EState::Stopped; }

private:
    Result StartSession(EditorServiceContext& Context);
    Result ResumeSession(EditorServiceContext& Context);
    Result StopSession(EditorServiceContext& Context);
    [[nodiscard]] static WorldExecutionProfile PausedExecutionProfile();

    EState m_state = EState::Stopped;
    std::optional<WorldPayload> m_editorSnapshot{};
    EWorldKind m_editorWorldKind = EWorldKind::Editor;
    WorldExecutionProfile m_editorExecutionProfile{};
};

} // namespace SnAPI::GameFramework::Editor
