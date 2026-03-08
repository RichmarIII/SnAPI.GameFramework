# SnAPI::GameFramework::WorldEcsRuntime::TickEntry

## Public Members

<div class="snapi-api-card" markdown="1">
### `int SnAPI::GameFramework::WorldEcsRuntime::TickEntry::Priority`
</div>
<div class="snapi-api-card" markdown="1">
### `uint64_t SnAPI::GameFramework::WorldEcsRuntime::TickEntry::Sequence`
</div>
<div class="snapi-api-card" markdown="1">
### `void* SnAPI::GameFramework::WorldEcsRuntime::TickEntry::Storage`
</div>
<div class="snapi-api-card" markdown="1">
### `void(* SnAPI::GameFramework::WorldEcsRuntime::TickEntry::PreTick) (void *, IWorld &, float))(void *, IWorld &, float)`
</div>
<div class="snapi-api-card" markdown="1">
### `void(* SnAPI::GameFramework::WorldEcsRuntime::TickEntry::Tick) (void *, IWorld &, float))(void *, IWorld &, float)`
</div>
<div class="snapi-api-card" markdown="1">
### `void(* SnAPI::GameFramework::WorldEcsRuntime::TickEntry::FixedTick) (void *, IWorld &, float))(void *, IWorld &, float)`
</div>
<div class="snapi-api-card" markdown="1">
### `void(* SnAPI::GameFramework::WorldEcsRuntime::TickEntry::LateTick) (void *, IWorld &, float))(void *, IWorld &, float)`
</div>
<div class="snapi-api-card" markdown="1">
### `void(* SnAPI::GameFramework::WorldEcsRuntime::TickEntry::PostTick) (void *, IWorld &, float))(void *, IWorld &, float)`
</div>
