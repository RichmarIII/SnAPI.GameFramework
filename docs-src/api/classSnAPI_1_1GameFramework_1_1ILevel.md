# SnAPI::GameFramework::ILevel

Interface for level containers.

## Public Functions

<div class="snapi-api-card" markdown="1">
### `virtual SnAPI::GameFramework::ILevel::~ILevel()=default`

Virtual destructor.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::ILevel::Tick(float DeltaSeconds)=0`

Per-frame tick.

**Parameters**

- `DeltaSeconds`: Time since last tick.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::ILevel::FixedTick(float DeltaSeconds)=0`

Fixed-step tick.

**Parameters**

- `DeltaSeconds`: Fixed time step.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::ILevel::LateTick(float DeltaSeconds)=0`

Late tick.

**Parameters**

- `DeltaSeconds`: Time since last tick.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::ILevel::EndFrame()=0`

End-of-frame processing.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual TExpected< NodeHandle > SnAPI::GameFramework::ILevel::CreateGraph(std::string Name)=0`

Create a child node graph.

**Parameters**

- `Name`: Graph name.

**Returns:** Handle to the created graph or error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual TExpectedRef< NodeGraph > SnAPI::GameFramework::ILevel::Graph(NodeHandle Handle)=0`

Access a child graph by handle.

**Parameters**

- `Handle`: Graph handle.

**Returns:** Reference wrapper or error.
</div>
