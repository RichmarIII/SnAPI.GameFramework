# SnAPI::GameFramework::Level

Level implementation that is also a NodeGraph.

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::Level::kTypeName`

Stable type name for reflection.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::Level::Level()`

Construct a level with default name.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::Level::Level(std::string Name)`

Construct a level with a name.

**Parameters**

- `Name`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Level::Tick(float DeltaSeconds) override`

Per-frame tick.

**Parameters**

- `DeltaSeconds`: Time since last tick.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Level::FixedTick(float DeltaSeconds) override`

Fixed-step tick.

**Parameters**

- `DeltaSeconds`: Fixed time step.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Level::LateTick(float DeltaSeconds) override`

Late tick.

**Parameters**

- `DeltaSeconds`: Time since last tick.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Level::EndFrame() override`

End-of-frame processing.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< NodeHandle > SnAPI::GameFramework::Level::CreateGraph(std::string Name) override`

Create a child node graph in this level.

**Parameters**

- `Name`: 

**Returns:** Handle to the created graph or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpectedRef< NodeGraph > SnAPI::GameFramework::Level::Graph(NodeHandle Handle) override`

Access a child graph by handle.

**Parameters**

- `Handle`: 

**Returns:** Reference wrapper or error.
</div>
<div class="snapi-api-card" markdown="1">
### `NodeGraph & SnAPI::GameFramework::Level::RootGraph()`

Access the root graph.

**Returns:** Reference to the root graph.
</div>
<div class="snapi-api-card" markdown="1">
### `const NodeGraph & SnAPI::GameFramework::Level::RootGraph() const`

Access the root graph (const).

**Returns:** Const reference to the root graph.
</div>
