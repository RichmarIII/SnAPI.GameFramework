# SnAPI::GameFramework::Editor::anonymous_namespace{EditorCoreServices.cpp}::SelectNodeCommand

## Private Members

<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::Editor::anonymous_namespace{EditorCoreServices.cpp}::SelectNodeCommand::m_previous`
</div>
<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::Editor::anonymous_namespace{EditorCoreServices.cpp}::SelectNodeCommand::m_next`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::Editor::anonymous_namespace{EditorCoreServices.cpp}::SelectNodeCommand::SelectNodeCommand(const NodeHandle &Previous, const NodeHandle &Next)`

**Parameters**

- `Previous`: 
- `Next`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::string_view SnAPI::GameFramework::Editor::anonymous_namespace{EditorCoreServices.cpp}::SelectNodeCommand::Name() const override`

Stable command name for diagnostics and UI.

**Returns:** Borrowed string view.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::anonymous_namespace{EditorCoreServices.cpp}::SelectNodeCommand::Execute(EditorServiceContext &Context) override`

Apply the command's forward mutation.

**Parameters**

- `Context`: Borrowed editor-service context.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::anonymous_namespace{EditorCoreServices.cpp}::SelectNodeCommand::Undo(EditorServiceContext &Context) override`

Apply the command's reverse mutation.

**Parameters**

- `Context`: Borrowed editor-service context.

**Returns:** Success or an error.
</div>
