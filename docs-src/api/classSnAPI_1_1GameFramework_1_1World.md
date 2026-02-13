# SnAPI::GameFramework::World

Concrete world root that owns levels and subsystems.

Responsibility boundaries:
- world controls frame lifecycle and end-of-frame flush
- levels are represented as child nodes/graphs under the world
- nodes/components can query world context through `Owner()->World()`

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::World::kTypeName`

Stable type name for reflection.
</div>

## Private Members

<div class="snapi-api-card" markdown="1">
### `JobSystem SnAPI::GameFramework::World::m_jobSystem`

World-scoped job dispatch facade for framework/runtime tasks.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::World::World()`

Construct a world with default name.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::World::World(std::string Name)`

Construct a world with a name.

**Parameters**

- `Name`:
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::World::~World() override`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::World::World(const World &)=delete`
</div>
<div class="snapi-api-card" markdown="1">
### `World & SnAPI::GameFramework::World::operator=(const World &)=delete`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::World::World(World &&) noexcept=default`
</div>
<div class="snapi-api-card" markdown="1">
### `World & SnAPI::GameFramework::World::operator=(World &&) noexcept=default`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::World::Tick(float DeltaSeconds) override`

Per-frame tick.

**Parameters**

- `DeltaSeconds`: Time since last tick.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::World::FixedTick(float DeltaSeconds) override`

Fixed-step tick.

**Parameters**

- `DeltaSeconds`: Fixed time step.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::World::LateTick(float DeltaSeconds) override`

Late tick.

**Parameters**

- `DeltaSeconds`: Time since last tick.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::World::EndFrame() override`

End-of-frame processing.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< NodeHandle > SnAPI::GameFramework::World::CreateLevel(std::string Name) override`

Create a level as a child node.

**Parameters**

- `Name`: 

**Returns:** Handle to the created level or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpectedRef< Level > SnAPI::GameFramework::World::LevelRef(NodeHandle Handle) override`

Access a level by handle.

**Parameters**

- `Handle`: 

**Returns:** Reference wrapper or error.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector< NodeHandle > SnAPI::GameFramework::World::Levels() const`

Get all level handles.

**Returns:** Vector of level handles.
</div>
<div class="snapi-api-card" markdown="1">
### `JobSystem & SnAPI::GameFramework::World::Jobs()`

Access the job system for parallel internal tasks.

**Returns:** Reference to JobSystem.
</div>
