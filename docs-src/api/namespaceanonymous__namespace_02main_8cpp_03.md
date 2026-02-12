# anonymous_namespace{main.cpp}

## Contents

- **Type:** anonymous_namespace{main.cpp}::Args
- **Type:** anonymous_namespace{main.cpp}::SessionListener
- **Type:** anonymous_namespace{main.cpp}::ReplicatedTransformComponent
- **Type:** anonymous_namespace{main.cpp}::MovingCube

## Type Aliases

<div class="snapi-api-card" markdown="1">
### `using anonymous_namespace{main.cpp}::Clock = std::chrono::steady_clock`
</div>

## Functions

<div class="snapi-api-card" markdown="1">
### `NodeHandle anonymous_namespace{main.cpp}::FindNodeByName(NodeGraph &Graph, const std::string &Name)`

**Parameters**

- `Graph`: 
- `Name`:
</div>
<div class="snapi-api-card" markdown="1">
### `NodeGraph * anonymous_namespace{main.cpp}::FindGraphByName(NodeGraph &Graph, const std::string &Name)`

**Parameters**

- `Graph`: 
- `Name`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool anonymous_namespace{main.cpp}::ValidateDemoNode(NodeGraph &Graph, const std::string &NodeName, const std::string &TargetName, int ExpectedHealth, float ExpectedSpeed, const std::string &ExpectedTag, const Vec3 &ExpectedSpawn, int ExpectedScore, const std::string &ExpectedLabel, const Vec3 &ExpectedTint)`

**Parameters**

- `Graph`: 
- `NodeName`: 
- `TargetName`: 
- `ExpectedHealth`: 
- `ExpectedSpeed`: 
- `ExpectedTag`: 
- `ExpectedSpawn`: 
- `ExpectedScore`: 
- `ExpectedLabel`: 
- `ExpectedTint`:
</div>
<div class="snapi-api-card" markdown="1">
### `void anonymous_namespace{main.cpp}::InitializeWorkingDirectory(const char *ExeArgv0)`

**Parameters**

- `ExeArgv0`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::string anonymous_namespace{main.cpp}::EndpointToString(const NetEndpoint &Endpoint)`

**Parameters**

- `Endpoint`:
</div>
<div class="snapi-api-card" markdown="1">
### `const char * anonymous_namespace{main.cpp}::DisconnectReasonToString(EDisconnectReason Reason)`

**Parameters**

- `Reason`:
</div>
<div class="snapi-api-card" markdown="1">
### `void anonymous_namespace{main.cpp}::PrintConnectionDump(const std::string &Label, const NetConnectionDump &Dump, float TimeSeconds)`

**Parameters**

- `Label`: 
- `Dump`: 
- `TimeSeconds`:
</div>
<div class="snapi-api-card" markdown="1">
### `void anonymous_namespace{main.cpp}::PrintUsage(const char *Exe)`

**Parameters**

- `Exe`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool anonymous_namespace{main.cpp}::ParseArgs(int argc, char **argv, Args &Out)`

**Parameters**

- `argc`: 
- `argv`: 
- `Out`:
</div>
<div class="snapi-api-card" markdown="1">
### `anonymous_namespace{main.cpp}::Field("Position", &ReplicatedTransformComponent::Position, EFieldFlagBits::Replication) .Field("Rotation"`
</div>
<div class="snapi-api-card" markdown="1">
### `EFieldFlagBits::Replication anonymous_namespace{main.cpp}::Field("Scale", &ReplicatedTransformComponent::Scale, EFieldFlagBits::Replication) .Constructor<>() .Register()))`
</div>
<div class="snapi-api-card" markdown="1">
### `void anonymous_namespace{main.cpp}::RegisterExampleTypes()`
</div>
<div class="snapi-api-card" markdown="1">
### `NetConfig anonymous_namespace{main.cpp}::MakeNetConfig()`
</div>
<div class="snapi-api-card" markdown="1">
### `std::shared_ptr< UdpTransportAsio > anonymous_namespace{main.cpp}::MakeUdpTransport(const NetEndpoint &Local)`

**Parameters**

- `Local`:
</div>
<div class="snapi-api-card" markdown="1">
### `void anonymous_namespace{main.cpp}::DrawCube(float Size)`

**Parameters**

- `Size`:
</div>
<div class="snapi-api-card" markdown="1">
### `void anonymous_namespace{main.cpp}::SetupCamera(int Width, int Height)`

**Parameters**

- `Width`: 
- `Height`:
</div>
<div class="snapi-api-card" markdown="1">
### `int anonymous_namespace{main.cpp}::RunServer(const Args &Parsed)`

**Parameters**

- `Parsed`:
</div>
<div class="snapi-api-card" markdown="1">
### `int anonymous_namespace{main.cpp}::RunClient(const Args &Parsed)`

**Parameters**

- `Parsed`:
</div>
<div class="snapi-api-card" markdown="1">
### `double anonymous_namespace{main.cpp}::ToMilliseconds(const Clock::duration &Duration)`

**Parameters**

- `Duration`:
</div>
<div class="snapi-api-card" markdown="1">
### `Level * anonymous_namespace{main.cpp}::FindLevelByName(World &WorldRef, const std::string &Name)`

**Parameters**

- `WorldRef`: 
- `Name`:
</div>
<div class="snapi-api-card" markdown="1">
### `NodeGraph * anonymous_namespace{main.cpp}::FindGraphByName(Level &LevelRef, const std::string &Name)`

**Parameters**

- `LevelRef`: 
- `Name`:
</div>
