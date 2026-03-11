#pragma once

#include "Editor/EditorExport.h"
#include "Editor/IEditorService.h"

#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

namespace SnAPI::GameFramework::Editor
{

class SNAPI_GAMEFRAMEWORK_EDITOR_API IEditorCommand
{
public:
    virtual ~IEditorCommand() = default;
    [[nodiscard]] virtual std::string_view Name() const = 0;
    virtual Result Execute(EditorServiceContext& Context) = 0;
    virtual Result Undo(EditorServiceContext& Context) = 0;
};

class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorCommandService final : public IEditorService
{
public:
    [[nodiscard]] std::string_view Name() const override;
    [[nodiscard]] int Priority() const override;
    Result Initialize(EditorServiceContext& Context) override;
    void Shutdown(EditorServiceContext& Context) override;

    Result Execute(EditorServiceContext& Context, std::unique_ptr<IEditorCommand> Command);
    Result Undo(EditorServiceContext& Context);
    Result Redo(EditorServiceContext& Context);

    [[nodiscard]] bool CanUndo() const { return !m_undoStack.empty(); }
    [[nodiscard]] bool CanRedo() const { return !m_redoStack.empty(); }
    [[nodiscard]] std::size_t UndoCount() const { return m_undoStack.size(); }
    [[nodiscard]] std::size_t RedoCount() const { return m_redoStack.size(); }
    void ClearHistory();

private:
    std::vector<std::unique_ptr<IEditorCommand>> m_undoStack{};
    std::vector<std::unique_ptr<IEditorCommand>> m_redoStack{};
    std::size_t m_maxHistory = 256;
};

} // namespace SnAPI::GameFramework::Editor
