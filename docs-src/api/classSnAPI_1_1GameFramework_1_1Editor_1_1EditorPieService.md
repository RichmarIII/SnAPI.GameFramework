# SnAPI::GameFramework::Editor::EditorPieService

Service that manages Play-In-Editor world session lifecycle.

`EditorPieService` snapshots the current editor world, rehydrates that snapshot into a PIE-flavored world instance, optionally starts gameplay, and restores the original editor snapshot when PIE stops.

Core semantics:
- `Play()` starts a fresh session from the current editor world or resumes a paused one.
- `Pause()` swaps the world to a paused execution profile that disables gameplay, physics, audio, and networking pumps.
- `Stop()` restores the serialized editor snapshot and original world kind/profile.
- PIE loads regenerate object ids so the running play session is isolated from the editor snapshot.

Ownership and lifetime:
- The service owns only the serialized editor snapshot; the runtime world remains owned by `GameRuntime`.

## Public Types

<div class="snapi-api-card" markdown="1">
### `enum EState`

High-level PIE session state.

**Values**

- `Stopped`: No PIE session is active and the editor world is live.
- `Playing`: PIE world is active and using the normal PIE execution profile.
- `Paused`: PIE world is active but running the paused execution profile.
</div>

## Private Members

<div class="snapi-api-card" markdown="1">
### `EState SnAPI::GameFramework::Editor::EditorPieService::m_state`
</div>
<div class="snapi-api-card" markdown="1">
### `std::optional<WorldPayload> SnAPI::GameFramework::Editor::EditorPieService::m_editorSnapshot`
</div>
<div class="snapi-api-card" markdown="1">
### `EWorldKind SnAPI::GameFramework::Editor::EditorPieService::m_editorWorldKind`
</div>
<div class="snapi-api-card" markdown="1">
### `WorldExecutionProfile SnAPI::GameFramework::Editor::EditorPieService::m_editorExecutionProfile`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `std::string_view SnAPI::GameFramework::Editor::EditorPieService::Name() const override`

Service name used for diagnostics.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorPieService::Initialize(EditorServiceContext &Context) override`

Reset PIE state for a fresh editor session.

**Parameters**

- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorPieService::Shutdown(EditorServiceContext &Context) override`

Stop any active PIE session and drop the stored snapshot.

**Parameters**

- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorPieService::Play(EditorServiceContext &Context)`

Start or resume PIE.

**Parameters**

- `Context`: 

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorPieService::Pause(EditorServiceContext &Context)`

Pause an active PIE session.

**Parameters**

- `Context`: 

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorPieService::Stop(EditorServiceContext &Context)`

Stop PIE and restore the editor snapshot.

**Parameters**

- `Context`: 

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `EState SnAPI::GameFramework::Editor::EditorPieService::State() const`

Current PIE state.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorPieService::IsPlaying() const`

Query whether PIE is actively playing.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorPieService::IsPaused() const`

Query whether PIE is currently paused.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorPieService::IsSessionActive() const`

Query whether any PIE session is currently active.
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorPieService::StartSession(EditorServiceContext &Context)`

**Parameters**

- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorPieService::ResumeSession(EditorServiceContext &Context)`

**Parameters**

- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorPieService::StopSession(EditorServiceContext &Context)`

**Parameters**

- `Context`:
</div>

## Private Static Func

<div class="snapi-api-card" markdown="1">
### `WorldExecutionProfile SnAPI::GameFramework::Editor::EditorPieService::PausedExecutionProfile()`
</div>
