# SnAPI::GameFramework::Editor::IEditorCommand

Undoable editor command contract.

`IEditorCommand` is the unit of work stored by `EditorCommandService`. Implementations are expected to fully describe both the forward mutation and the reverse mutation for one editor action such as selection changes, hierarchy edits, or property adjustments.

Core semantics:
- `Execute()` applies the command's forward mutation.
- `Undo()` applies the reverse mutation.
- The same command instance is reused for execute/undo/redo cycles while it remains in history.

Ownership and lifetime:
- Commands are heap-allocated and owned by `EditorCommandService` after successful execution.
- A command may keep internal snapshots required for undo/redo, but should not assume external object pointers remain valid unless it re-resolves them safely.

Threading model:
- Main-thread only.

## Public Functions

<div class="snapi-api-card" markdown="1">
### `virtual SnAPI::GameFramework::Editor::IEditorCommand::~IEditorCommand()=default`
</div>
<div class="snapi-api-card" markdown="1">
### `virtual std::string_view SnAPI::GameFramework::Editor::IEditorCommand::Name() const =0`

Stable command name for diagnostics and UI.

**Returns:** Borrowed string view.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual Result SnAPI::GameFramework::Editor::IEditorCommand::Execute(EditorServiceContext &Context)=0`

Apply the command's forward mutation.

**Parameters**

- `Context`: Borrowed editor-service context.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual Result SnAPI::GameFramework::Editor::IEditorCommand::Undo(EditorServiceContext &Context)=0`

Apply the command's reverse mutation.

**Parameters**

- `Context`: Borrowed editor-service context.

**Returns:** Success or an error.
</div>
