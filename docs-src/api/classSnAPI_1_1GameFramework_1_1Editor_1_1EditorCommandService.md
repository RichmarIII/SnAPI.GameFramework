# SnAPI::GameFramework::Editor::EditorCommandService

Central undo and redo service for editor mutations.

`EditorCommandService` owns the command-history stacks used by the editor shell. It executes commands immediately, pushes successful commands onto the undo stack, clears redo history on new forward execution, and replays the same command instances when users request undo or redo.

Core semantics:
- A command enters history only if `Execute()` succeeds.
- `Undo()` pops from the undo stack and pushes onto the redo stack only if reversal succeeds.
- `Redo()` re-executes the command and moves it back to the undo stack only if replay succeeds.
- Oldest undo entries are discarded when history reaches `m_maxHistory`.

Threading model:
- Main-thread only.

## Private Members

<div class="snapi-api-card" markdown="1">
### `std::vector<std::unique_ptr<IEditorCommand> > SnAPI::GameFramework::Editor::EditorCommandService::m_undoStack`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<std::unique_ptr<IEditorCommand> > SnAPI::GameFramework::Editor::EditorCommandService::m_redoStack`
</div>
<div class="snapi-api-card" markdown="1">
### `std::size_t SnAPI::GameFramework::Editor::EditorCommandService::m_maxHistory`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `std::string_view SnAPI::GameFramework::Editor::EditorCommandService::Name() const override`

Service name used for diagnostics.
</div>
<div class="snapi-api-card" markdown="1">
### `int SnAPI::GameFramework::Editor::EditorCommandService::Priority() const override`

Priority hint for service initialization.

**Returns:** A very low value so command history is ready before higher-level editor services begin using it.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorCommandService::Initialize(EditorServiceContext &Context) override`

Reset command history for a fresh editor session.

**Parameters**

- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorCommandService::Shutdown(EditorServiceContext &Context) override`

Clear command history during shutdown.

**Parameters**

- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorCommandService::Execute(EditorServiceContext &Context, std::unique_ptr< IEditorCommand > Command)`

Execute and record a command.

**Parameters**

- `Context`: Borrowed editor-service context.
- `Command`: Owning pointer to the command to execute.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorCommandService::Undo(EditorServiceContext &Context)`

Undo the most recently executed command.

**Parameters**

- `Context`: Borrowed editor-service context.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorCommandService::Redo(EditorServiceContext &Context)`

Redo the most recently undone command.

**Parameters**

- `Context`: Borrowed editor-service context.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorCommandService::CanUndo() const`

Query whether an undo operation is currently available.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorCommandService::CanRedo() const`

Query whether a redo operation is currently available.
</div>
<div class="snapi-api-card" markdown="1">
### `std::size_t SnAPI::GameFramework::Editor::EditorCommandService::UndoCount() const`

Current undo-stack depth.
</div>
<div class="snapi-api-card" markdown="1">
### `std::size_t SnAPI::GameFramework::Editor::EditorCommandService::RedoCount() const`

Current redo-stack depth.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorCommandService::ClearHistory()`

Drop both undo and redo history stacks.
</div>
