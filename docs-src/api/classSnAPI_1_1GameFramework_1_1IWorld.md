# SnAPI::GameFramework::IWorld

Root runtime container contract for gameplay sessions.

## Public Functions

<div class="snapi-api-card" markdown="1">
### `virtual SnAPI::GameFramework::IWorld::~IWorld()=default`

Virtual destructor.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IWorld::Tick(float DeltaSeconds)=0`

Per-frame tick.

**Parameters**

- `DeltaSeconds`: Time since last tick.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IWorld::FixedTick(float DeltaSeconds)=0`

Fixed-step tick.

**Parameters**

- `DeltaSeconds`: Fixed time step.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IWorld::LateTick(float DeltaSeconds)=0`

Late tick.

**Parameters**

- `DeltaSeconds`: Time since last tick.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IWorld::EndFrame()=0`

End-of-frame processing.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual TExpected< NodeHandle > SnAPI::GameFramework::IWorld::CreateLevel(std::string Name)=0`

Create a level as a child node.

**Parameters**

- `Name`: Level name.

**Returns:** Handle to the created level or error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual TExpectedRef< Level > SnAPI::GameFramework::IWorld::LevelRef(NodeHandle Handle)=0`

Access a level by handle.

**Parameters**

- `Handle`: Level handle.

**Returns:** Reference wrapper or error.
</div>
