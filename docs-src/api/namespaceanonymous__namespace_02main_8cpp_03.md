# anonymous_namespace{main.cpp}

## Contents

- **Type:** anonymous_namespace{main.cpp}::BenchmarkOptions
- **Type:** anonymous_namespace{main.cpp}::ComponentSpec
- **Type:** anonymous_namespace{main.cpp}::RunResult

## Enumerations

<div class="snapi-api-card" markdown="1">
### `enum ETickMode`

**Values**

- `EcsOnly`
</div>

## Type Aliases

<div class="snapi-api-card" markdown="1">
### `using anonymous_namespace{main.cpp}::Clock = std::chrono::steady_clock`
</div>

## Functions

<div class="snapi-api-card" markdown="1">
### `NodeHandle anonymous_namespace{main.cpp}::FindNodeByName(TGraphLike &Graph, const std::string &Name)`

**Parameters**

- `Graph`: 
- `Name`:
</div>
<div class="snapi-api-card" markdown="1">
### `Level * anonymous_namespace{main.cpp}::FindGraphByName(Level &Graph, const std::string &Name)`

**Parameters**

- `Graph`: 
- `Name`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool anonymous_namespace{main.cpp}::ValidateDemoNode(Level &Graph, const std::string &NodeName, const std::string &TargetName, int ExpectedHealth, float ExpectedSpeed, const std::string &ExpectedTag, const Vec3 &ExpectedSpawn, int ExpectedScore, const std::string &ExpectedLabel, const Vec3 &ExpectedTint)`

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
### `double anonymous_namespace{main.cpp}::ToMilliseconds(const Clock::duration Duration)`

**Parameters**

- `Duration`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool anonymous_namespace{main.cpp}::AddComponentToNode(BaseNode &Node, std::string &OutError)`

**Parameters**

- `Node`: 
- `OutError`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector< ComponentSpec > anonymous_namespace{main.cpp}::BuildComponentSpecs()`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string anonymous_namespace{main.cpp}::JoinComponentNames(const std::vector< ComponentSpec > &Specs)`

**Parameters**

- `Specs`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::string anonymous_namespace{main.cpp}::UtcTimestampNow()`
</div>
<div class="snapi-api-card" markdown="1">
### `bool anonymous_namespace{main.cpp}::ParseUnsigned(const std::string &Value, std::uint64_t &Out)`

**Parameters**

- `Value`: 
- `Out`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool anonymous_namespace{main.cpp}::ParseFloat(const std::string &Value, float &Out)`

**Parameters**

- `Value`: 
- `Out`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::string_view anonymous_namespace{main.cpp}::TickModeName(const ETickMode Mode)`

**Parameters**

- `Mode`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool anonymous_namespace{main.cpp}::ParseTickMode(const std::string_view Value, ETickMode &OutMode)`

**Parameters**

- `Value`: 
- `OutMode`:
</div>
<div class="snapi-api-card" markdown="1">
### `void anonymous_namespace{main.cpp}::PrintUsage(const char *ProgramName)`

**Parameters**

- `ProgramName`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool anonymous_namespace{main.cpp}::ParseArgs(const int Argc, char **Argv, BenchmarkOptions &OutOptions)`

**Parameters**

- `Argc`: 
- `Argv`: 
- `OutOptions`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool anonymous_namespace{main.cpp}::WriteResultsFile(const BenchmarkOptions &Options, const std::filesystem::path &OutputPath, const ETickMode Mode, const RunResult &Results, const std::vector< ComponentSpec > &ComponentSpecs)`

**Parameters**

- `Options`: 
- `OutputPath`: 
- `Mode`: 
- `Results`: 
- `ComponentSpecs`:
</div>
<div class="snapi-api-card" markdown="1">
### `void anonymous_namespace{main.cpp}::ConfigureWorldForMode(World &WorldInstance)`

**Parameters**

- `WorldInstance`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool anonymous_namespace{main.cpp}::RunScenario(const BenchmarkOptions &Options, const std::vector< ComponentSpec > &ComponentSpecs, const std::filesystem::path &OutputPath, RunResult &OutResults)`

**Parameters**

- `Options`: 
- `ComponentSpecs`: 
- `OutputPath`: 
- `OutResults`:
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
