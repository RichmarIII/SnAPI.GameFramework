#include "Editor/EditorCommandService.h"

namespace SnAPI::GameFramework::Editor
{
std::string_view EditorCommandService::Name() const
{
    return "EditorCommandService";
}

int EditorCommandService::Priority() const
{
    return -1000;
}

Result EditorCommandService::Initialize(EditorServiceContext& Context)
{
    (void)Context;
    ClearHistory();
    return Ok();
}

void EditorCommandService::Shutdown(EditorServiceContext& Context)
{
    (void)Context;
    ClearHistory();
}

Result EditorCommandService::Execute(EditorServiceContext& Context, std::unique_ptr<IEditorCommand> Command)
{
    if (!Command)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Editor command must not be null"));
    }

    if (const Result ExecuteResult = Command->Execute(Context); !ExecuteResult)
    {
        return ExecuteResult;
    }

    m_redoStack.clear();
    if (m_undoStack.size() >= m_maxHistory)
    {
        m_undoStack.erase(m_undoStack.begin());
    }
    m_undoStack.emplace_back(std::move(Command));
    return Ok();
}

Result EditorCommandService::Undo(EditorServiceContext& Context)
{
    if (m_undoStack.empty())
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "No editor command available to undo"));
    }

    std::unique_ptr<IEditorCommand> Command = std::move(m_undoStack.back());
    m_undoStack.pop_back();

    if (const Result UndoResult = Command->Undo(Context); !UndoResult)
    {
        m_undoStack.emplace_back(std::move(Command));
        return UndoResult;
    }

    m_redoStack.emplace_back(std::move(Command));
    return Ok();
}

Result EditorCommandService::Redo(EditorServiceContext& Context)
{
    if (m_redoStack.empty())
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "No editor command available to redo"));
    }

    std::unique_ptr<IEditorCommand> Command = std::move(m_redoStack.back());
    m_redoStack.pop_back();

    if (const Result RedoResult = Command->Execute(Context); !RedoResult)
    {
        m_redoStack.emplace_back(std::move(Command));
        return RedoResult;
    }

    m_undoStack.emplace_back(std::move(Command));
    return Ok();
}

void EditorCommandService::ClearHistory()
{
    m_undoStack.clear();
    m_redoStack.clear();
}

 
} // namespace SnAPI::GameFramework::Editor
