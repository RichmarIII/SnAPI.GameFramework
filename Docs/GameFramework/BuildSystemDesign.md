# SnAPI Game Build, Packaging, and Project Creation System

_Detailed Architecture and Implementation Design_

Version 0.9 • March 2026

> Purpose: define a production-grade editor-integrated build system that can create projects, compile game code modules, cook and package assets into `.snpak` output, and emit runnable builds for multiple platforms.

> Maintenance note: this document is a living design artifact. As implementation progresses, design changes, constraints, and clarified decisions should be updated here so the code and the architecture do not drift apart.


# Goals, Non-Goals, and Design Principles

The build system exists to make project creation, game module compilation, asset cooking, staging, and packaging feel like one coherent workflow rather than a set of disconnected workflows. A user should be able to create a project, author content, package it, select one or more targets, and end up with a reproducible runnable build.

The system must be production-oriented. That means deterministic manifests, actionable errors, resume-safe intermediate state, profile-driven settings, command-line automation, and enough structural flexibility that future console or mobile targets can fit without requiring architectural rework.

The system must also respect the existing SnAPI style: explicit over magical, predictable over clever, type-safe over stringly chaos, and modular enough that the editor can orchestrate the process while individual services remain testable.

> Design principle: the user-facing flow should feel simple, but the internal model must stay explicit. If a setting matters, it needs a named home in the project or profile schema rather than an implicit default hidden elsewhere.

- Goal: Create a first-class project, plugin, and module format plus scaffold generator.
- Goal: Package a subset of levels and assets, not only the entire project.
- Goal: Support include and exclude rules at directory, asset, asset-kind, and label/profile levels.
- Goal: Build both editor-hosted and runtime-hosted code paths from the same project modules.
- Goal: Emit staging directories with clear Bin/, Assets/, Config/, Metadata/, Logs/, and Symbols/ structure.
- Goal: Allow platform-specific settings without forking the project definition into madness.
- Non-Goal: Solve distributed build farms in V1.
- Non-Goal: Support arbitrary build systems other than CMake in V1.
- Non-Goal: Hide every build concept from users. Some controls are necessary; they should be presented clearly.


# Product Surface Overview

The system has three top-level product surfaces. The first is Project and Plugin Creation: a guided wizard that generates a new project or plugin descriptor, directory layout, starter source files, starter config, and starter build fragments. The second is Packaging: a tool window or modal launched from the editor, but backed by reusable build services and CLI commands. The third is Module Creation and Management: the authoring flow for adding new runtime, editor, shared, developer, or test modules to an existing project or plugin in a way that keeps the descriptor and CMake integration synchronized.

| Surface | Primary Users | Primary Outputs | Key Risks |
|---|---|---|---|
| Project / Plugin Creation | New project authors, plugin authors, team leads, template maintainers | Project or plugin root, .snproj.json or plugin descriptor, starter C++ module, config files | Bad defaults create permanent pain |
| Package | Content authors, gameplay programmers, release engineers | Cooked assets, .snpak bundles, binaries, manifests, reports | Non-determinism and bad diagnostics waste hours |
| CLI Build | CI, build scripts, local automation | Same outputs as package flow, plus machine-readable results | Feature skew versus editor path |
| Profile Management | Tech leads, build engineers | Reusable packaging targets and project defaults | Settings duplication or inheritance confusion |
| Module Creation | Gameplay programmers, engine programmers, plugin maintainers | New module folders, descriptor entries, CMake fragments, starter code | Descriptor/CMake skew and poor defaults |


# Core Concepts

The design introduces a small number of core concepts that must be used consistently across editor UI, CLI, manifests, and code services.

A Project is a source workspace rooted by a .snproj.json file. A Plugin is a separately authored extension unit that can contribute one or more modules, content, and build metadata to a host project. A Module is a code unit declared by the project or plugin and backed by a CMake fragment. A Build Profile is a named configuration describing target platform, build configuration, included content, excluded content, packaging options, execution environment, and platform overrides. A Build Request is one concrete invocation produced by the editor or CLI. A Build Graph is the executable DAG of stages and tasks derived from the Build Request.

- Project: persistent authored workspace
- Project Descriptor: canonical JSON file describing the project
- Plugin: reusable extension unit with modules and optional content
- Module: buildable code unit owned by the project or plugin
- Build Profile: reusable named package settings
- Build Request: concrete invocation with resolved settings
- Build Graph: staged DAG of work nodes
- Staging Directory: normalized output tree before optional archive or installer generation
- Cook Manifest: asset-level record of what was selected, why, and from which source hash
- Package Manifest: final output record of binaries, bundles, configs, versions, hashes, and platform metadata


# System Context

The build system sits between the editor, the project descriptor, the code compiler, and SnAPI.AssetPipeline. The editor is responsible for gathering user intent. The build services are responsible for turning intent into a Build Graph. The AssetPipeline is responsible for cooking assets into runtime payloads and bundles. The C++ toolchain is responsible for compiling and linking project modules against SnAPI.GameFramework and any enabled engine modules.

The runtime-facing side is already represented by GameProjectRuntime, which loads project metadata such as asset root and startup level into a GameRuntime session. The new design extends the authoring/build side so the same project descriptor can support both iterative editor workflows and final package generation.

```
Editor UI -> Build Request -> Build Planner -> Build Graph
                                      |-> Validation
                                      |-> Asset Selection
                                      |-> Cook Tasks (SnAPI.AssetPipeline)
                                      |-> Code Build Tasks (CMake + compiler)
                                      |-> Stage Tasks
                                      |-> Bundle Tasks (.snpak)
                                      |-> Manifest Tasks
                                      `-> Report Tasks
```


# Project Descriptor and Workspace Layout

The project descriptor should live at the project root as <ProjectName>.snproj.json or simply snproj.json. The file should be human-readable, diffable, and explicit. It should not try to behave like a database or accumulate unrelated state.

The descriptor must have a stable schema version and allow future migration. It should represent project identity, directories, modules, startup assets, build profiles, platform overrides, packaging defaults, and template provenance.

- The descriptor path is the authoritative project identity path.
- All relative paths are interpreted relative to the descriptor directory.
- The descriptor should be valid even if Intermediate/ and Saved/ are deleted.
- The descriptor must never store ephemeral machine-local state that belongs in Saved/ or Intermediate/.


# Recommended Project Root Layout

The project root layout should be intentionally simple and predictable. Stable layouts survive teams and automation.

| Path | Purpose |
|---|---|
| Assets/ | Source-authored assets and subfolders such as Levels/, Materials/, Audio/, UI/, Scripts/, Prefabs/. |
| Code/ | Project-owned C++ modules, CMake fragments, headers, source, and optional third-party code approved by project policy. |
| Config/ | Human-authored runtime and build config files. Environment-agnostic defaults only. |
| Build/ | Optional generated user-facing build presets or generated fragments that are safe to regenerate. |
| Intermediate/ | Regenerable outputs: generated CMake cache fragments, cook caches, object files, reflection outputs, temp manifests. |
| Saved/ | User-local or machine-local state: autosaves, logs, recent package destinations, build history, crash reports. |
| Binaries/ | Optional local development binary output for project modules if not kept in Intermediate/. |
| DerivedDataCache/ | Optional local/shared cache for cooked and derived asset content. |
| <Project>.snproj.json | Project descriptor |


# Project Descriptor Schema

The schema should be split into high-signal top-level blocks rather than a flat jungle of keys. Each block maps to one responsibility.

| Block | Responsibility |
|---|---|
| Format | Schema version, minimum supported tool version, migration markers |
| Project | Name, display name, company, project UUID, description |
| Paths | Asset root, code root, config root, intermediate root, saved root, output roots |
| Startup | Startup level asset id, default render settings asset id, default game class, default game mode class |
| Modules | Code module declarations and build metadata |
| Profiles | Named build/package profiles |
| Platforms | Platform-specific overrides and signing info |
| Packaging | Global package defaults such as compression, chunking, symbols, archive options |
| AssetRules | Global include/exclude and label-based selection rules |
| Templates | Project template provenance and update hints |

```
{
  "Format": {
    "SchemaVersion": 1,
    "MinimumToolVersion": "0.9.0"
  },
  "Project": {
    "Name": "MyGame",
    "DisplayName": "My Game",
    "ProjectId": "uuid-here"
  },
  "Paths": {
    "AssetRoot": "Assets",
    "CodeRoot": "Code",
    "ConfigRoot": "Config",
    "IntermediateRoot": "Intermediate",
    "SavedRoot": "Saved"
  },
  "Startup": {
    "StartupLevelAsset": "Levels/MainMenu.level",
    "DefaultGameClass": "MyGame::MyGame",
    "DefaultGameModeClass": "MyGame::MyGameMode"
  }
}
```


# Module Declaration Model

Each project module should be declared both in the descriptor and in a generated or user-maintained CMake fragment under Code/. The descriptor exists for editor tooling and high-level build planning. The CMake fragment exists for the native build toolchain. The two must stay synchronized through explicit generation rules.

| Field | Meaning |
|---|---|
| Name | Stable module name |
| Type | Runtime, Editor, Shared, Developer, Test, or Program |
| Root | Relative path under Code/ |
| PublicDependencies | Other project or engine modules exposed through public headers |
| PrivateDependencies | Modules used only internally |
| Platforms | Optional allow/deny filter |
| PreprocessorDefinitions | Module-local compile definitions |
| UseReflectionGen | Whether module participates in reflection generation pipeline |
| UseSWIG | Whether module contributes SWIG bindings |
| LoadInEditor | Whether editor executable should link or load the module |
| LoadInRuntime | Whether runtime executable should link or load the module |

The same declaration model should also back plugin-owned modules. V1 does not need a completely separate plugin build language. Plugin modules should use the same schema shape, generation rules, validation, and CMake orchestration as project modules so the system remains uniform.


# Project and Plugin Creation Wizard

The Project and Plugin Creation wizard should be launched from the editor welcome surface and from File > New Project or New Plugin. It should be fast, opinionated, and not ask thirty questions before the user even has a folder.

The wizard should follow a staged flow: Template, Identity, Location, Code Options, Startup Content, Platform Defaults, Review, Create.

1. Template step: choose Blank Game, Third-Person Starter, Empty Sandbox, Networked Prototype, Runtime Plugin, Editor Plugin, or custom organization templates.
2. Identity step: project name, display name, company, namespace root, optional project icon.
3. Location step: destination folder, project root preview, collision checks, disk availability warning.
4. Code step: create starter runtime module, optional editor module, starter IGame, starter IGameMode, optional LocalPlayer subclass, optional custom pawn.
5. Startup Content step: starter level choice, lighting preset, renderer settings asset, input mappings.
6. Platform Defaults step: initial supported targets and default package destinations.
7. Review step: show exact folders and files that will be created.
8. Create step: materialize files, validate, optionally open the project immediately.

The current V1 editor implementation exposes these authoring flows as dedicated modals backed by the shared creation services:

- `New Project...` covers project shape, identity, parent directory, namespace, starter runtime and editor module options, and startup level selection.
- `New Plugin...` covers plugin shape, identity, parent directory, description, asset participation, and starter runtime and editor module options.
- `Add Module...` covers project-versus-plugin target selection, descriptor path, module type, namespace, root override, dependency lists, platform filters, preprocessor definitions, reflection, SWIG, load flags, and optional runtime gameplay bootstrap generation.
- Every authored field in those flows is accompanied by a compact hover-help affordance so detailed guidance stays available without forcing fixed-width explanatory text into the form layout.

Those modals are intentionally service-driven. The editor shell gathers authored intent, but the filesystem and descriptor mutations still flow through `ProjectCreationService`, `PluginCreationService`, and `ModuleCreationService`.


# Generated Starter Files

A newly created project or plugin should include enough code to compile and launch or integrate meaningfully, but not so much placeholder architecture that it becomes future maintenance overhead.

| Generated File | Purpose |
|---|---|
| Code/CMakeLists.txt | Minimal checked-in bridge that includes generated project module wiring |
| Intermediate/Build/Generated/ProjectModules.cmake | Generated project module registration and editor/runtime linkage |
| Code/<Project>/CMakeLists.txt | Minimal module-root bridge that includes the module fragment |
| Code/<Project>/<Project>.CMakeLists.txt | Module-specific CMake fragment |
| Code/<Project>/include/<Project>/<Project>Game.h | Starter IGame implementation header |
| Code/<Project>/src/<Project>Game.cpp | Starter IGame implementation source |
| Code/<Project>/include/<Project>/<Project>GameMode.h | Starter IGameMode header |
| Code/<Project>/src/<Project>GameMode.cpp | Starter IGameMode source |
| Code/<Project>/include/<Project>/<Project>Module.h | Module registration or bootstrap declarations if needed |
| Code/<Project>/src/<Project>Module.cpp | Module bootstrap implementation |
| Code/<Project>Editor/CMakeLists.txt | Optional editor module-root bridge that includes the editor module fragment |
| Code/<Project>Editor/<Project>Editor.CMakeLists.txt | Optional editor module-specific CMake fragment |
| Code/<Project>Editor/include/<Project>Editor/<Project>EditorModule.h | Optional starter editor-module header |
| Code/<Project>Editor/src/<Project>EditorModule.cpp | Optional starter editor-module source |
| Config/DefaultGame.json | Default runtime/gameplay configuration |
| Assets/Levels/Main.level | Starter level source asset |
| <Project>.snproj.json | Project descriptor |

Module creation inside an existing project or plugin should also be first-class. Adding a module should update descriptor metadata, emit the module folder layout, emit a starter CMake fragment, and wire the generated root build include without asking users to hand-edit multiple disconnected files.

For runtime modules, the editor and CLI module-creation flows can also emit starter gameplay bootstrap files alongside the base module scaffold. That V1 bootstrap consists of:

- `<Module>Game.h/.cpp`
- `<Module>GameMode.h/.cpp`
- module source updates that expose the starter `IGame` and `IGameMode` entrypoints
- module CMake updates that compile those sources by default

```
class MyGame final : public SnAPI::GameFramework::IGame
{
public:
    std::string_view Name() const override { return "MyGame"; }
    Result Initialize(GameplayHost& Host) override;
    void Tick(GameplayHost& Host, float DeltaSeconds) override;
    void Shutdown(GameplayHost& Host) override;
};
```


# Editor Integration

The package flow should appear in the editor in three places: a toolbar or dropdown quick action, a Packaging surface, and a Recent Profiles submenu. The quick action is for common paths like Package > Windows > Shipping. The Packaging surface is for detailed selection, staging review, and validation. The Recent Profiles menu is for speed once the team settles on standard outputs.

The current V1 implementation uses a modal Packaging surface rather than a docked tool window. `EditorBuildService` routes editor packaging through the same request/planner/execution/history backend used by the CLI, and `File > Package Project...` opens a packaging modal with:

- an Overview tab for profile selection, plan/package actions, and last-build summary
- a Content tab for selected levels, explicit assets, include and exclude folders, label rules, asset-kind rules, dependency policy, and chunk strategy
- a Platform tab for target platform, build configuration, and execution environment selection
- an Output tab for output root, package-directory override, archive enablement, archive format, and archive file naming
- a History tab for refresh, retry, rebuild-all, and report comparison actions
- a Console tab for captured stdout, stderr, shared build events, and streamed configure/build output
- compact hover tooltips on the package fields so planner and packaging semantics remain discoverable in-editor
- typed selectors for constrained values such as target platform, configuration, dependency policy, chunk strategy, and archive format
- token-entry selectors with discovered-asset add affordances for selected levels, explicit assets, include/exclude folders, label rules, and asset-kind rules
- live draft refresh for resolved summaries and read-only previews without forcing full modal rebuilds on ordinary status or console updates
- shared backend execution so editor and CLI requests stay aligned, with editor-triggered planning and packaging dispatched onto a background worker thread to keep the shell responsive
- project-local build history loaded from `Saved/BuildHistory/`

The editor and CLI planning path should remain a hard parity point. The current V1 implementation keeps both surfaces on the same request/planner services, and parity coverage compares the serialized build graph for the same frozen request rather than trusting the two paths to stay aligned by convention.

Toolbar quick actions and Recent Profiles menus remain follow-on work on top of the same backend.

- Toolbar dropdown: Package Project, Create Project, Create Plugin, Add Module, Build Active Profile, Open Packaging Window
- Main menu: File > New Project, File > New Plugin, File > Open Project, File > Package, File > Build Profiles
- Context actions: right-click level asset -> Package With Level Included, mark as Startup Level, add to profile
- Build status area: current profile, last package result, warnings count, open logs/report


# Packaging Window Layout

The Packaging surface should support both a modal-first V1 presentation and a later dockable-window evolution without changing backend behavior. The current implementation uses a responsive modal with Overview, Content, Platform, Output, and History tabs. A later docked layout can grow around the same build/profile/history state once the editor shell needs a more persistent surface.

The surface must support saveable named profiles, profile diff against project defaults, and a dry-run preview. The current modal already exposes resolved profile selection, content-selection overrides, output overrides, plan/package actions, and history-backed retry/rebuild flows through `EditorBuildService`.

Asset-label filtering is currently exposed as include and exclude rule entry in the packaging surface. Standalone per-asset label authoring is not yet a separate editor workflow, so the packaging UI should be treated as the current V1 label-entry surface rather than a final authoring experience.

Free-form entry should only remain where the authored value is genuinely open-ended. For example, execution-environment overrides stay text-based because they may name host-local adapters or arbitrary `docker://...` images, and package-directory/archive-file overrides remain text because they are explicit user-authored names rather than enumerated choices.

| Panel | Main Controls |
|---|---|
| Overview | Profile selector, resolved profile summary, plan button, package button, last-build output summary |
| Build History | Recent build runs, refresh, retry selected build, rebuild all, compare against the latest build |
| Content Selection | Selected levels, explicit assets, include/exclude folders, asset labels, asset kinds, dependency policy, chunk strategy |
| Code Modules | Which project modules participate, build configuration, symbols, unity/LTO options where allowed |
| Platform Settings | Target platform, configuration, execution environment, and later signing/icon/platform metadata |
| Output | Output root, package directory override, archive options, and output naming |
| Validation | Missing startup level, unresolved assets, module compile issues, platform SDK status |


# User Experience Rules

The UI must never misrepresent what will happen. If the system will rebuild code, say that. If only assets will be recooked, say that. If a profile excludes a folder due to inheritance, show where the rule came from. Hidden behavior undermines trust.

- Every package action produces a build plan preview before execution, unless the user disables preview for trusted profiles.
- Validation errors block build start. Validation warnings do not block by default but are visible and recorded.
- Profiles always show inherited and overridden values distinctly.
- Paths are shown normalized and copyable.
- Recent package destinations are stored per project in Saved/, not in the descriptor.
- Every completed build writes a machine-readable report and a human-readable summary.


# Build Graph and Execution Model

Internally, the system should execute packaging through a Build Graph. This keeps packaging orchestration decomposed, testable, and maintainable instead of collapsing into a single monolithic command path. The graph should have named stages, typed nodes, explicit inputs and outputs, and cache keys.

A Build Graph node represents a task such as ValidateProject, ResolveAssetSelection, GenerateModuleCMake, ConfigureCMake, BuildCode, CookAssetBatch, WriteCookManifest, BundleSnpak, StageConfigs, WritePackageManifest, or ArchiveOutput.

The planner should emit nodes in deterministic stage order and derive the build-history and staging roots during planning so dry-run, CLI, editor preview, and later execution all describe the same filesystem intent. The initial stage root should live under `Saved/BuildHistory/<BuildId>/Stage/` unless a later profile/output setting explicitly redirects it.

| Stage | Typical Nodes |
|---|---|
| Preflight | LoadProject, LoadProfile, NormalizePaths, ResolvePlatformToolchain, ValidateSDK |
| Planning | ResolveBuildSettings, ResolveModuleSet, ResolveAssetSelection, ExpandDependencies |
| Code | GenerateCMake, ConfigureCMake, CompileTargets, LinkTargets, CopyBinaries, CopySymbols |
| Assets | EnumerateAssets, HashSources, CookAssets, BuildChunkLayout, WriteCookManifest, WriteSnpak |
| Staging | CreateStageTree, CopyConfigs, CopyMetadata, CopyAuxiliaryFiles, WriteBootstrapFiles |
| Finalize | WritePackageManifest, WriteBuildReport, OptionalArchive, OptionalSigning, CleanupTemp |

```
struct BuildGraphNode
{
    BuildNodeId Id;
    std::string Name;
    EBuildNodeType Type;
    std::vector<BuildNodeId> Dependencies;
    BuildInputs Inputs;
    BuildOutputs Outputs;
    BuildCacheKey CacheKey;
};
```


# Build Request Resolution

The system should resolve settings in a strict order so the same request always means the same thing.

1. Load project descriptor.
2. Select named profile or create an ephemeral profile from command-line/editor selections.
3. Apply platform-specific overrides.
4. Apply configuration-specific overrides such as Development, Shipping, Test.
5. Apply one-shot request overrides from the user action.
6. Normalize paths and expand environment variables where explicitly supported.
7. Freeze the resolved Build Request and compute a Request Hash.

One-shot request overrides should use the same append, replace, and explicit-clear semantics as build profiles so editor actions and CLI flags do not invent a separate override language. The initial request hash implementation should be a deterministic hash of the canonical frozen request JSON emitted by the build-request service, so identical requests produce identical hashes across local and CI execution.


# Build Configurations

The design should support at minimum Debug, Development, Test, and Shipping. Configuration affects both code build behavior and package composition. For example, Shipping may strip symbols from staged outputs, disable editor-only content, disable developer modules, and apply tighter package validation.

| Configuration | Intended Use | Typical Defaults |
|---|---|---|
| Debug | Deep local debugging | No optimization, symbols on, verbose logs, asserts on, loose validation |
| Development | Regular iteration | Moderate optimization, symbols on, logs on, editor-friendly assets |
| Test | QA and automation | Shipping-like behavior with diagnostics retained |
| Shipping | Final release build | High optimization, symbols externalized, editor assets excluded, package signing enabled where applicable |


# Asset Selection Model

Packaging must not be all-or-nothing. The selection model should combine explicit level selection, explicit asset inclusion, folder rules, label rules, kind rules, and dependency expansion policy.

The fundamental user-friendly entry point is level-driven packaging: select one or more levels, then include all hard dependencies, plus optional soft/reference-based dependencies according to profile rules.

Project startup defaults should participate in the same selection model. The current V1 implementation always seeds `Startup.DefaultRenderSettingsAssetId` into the package plan when it is configured, so packaged runtime boot does not carry a render-settings asset id that was never cooked.

- Primary selectors: StartupLevel, ExplicitLevels, ExplicitAssets
- Secondary selectors: IncludeFolders, IncludeAssetLabels, IncludeAssetKinds
- Exclusion selectors: ExcludeFolders, ExcludeAssets, ExcludeAssetLabels, ExcludeAssetKinds
- Dependency policy: HardOnly, HardAndSoft, HardSoftAndEditorPreview, CustomResolver
- Conflict rule: explicit include beats inherited exclude only if profile option AllowExplicitOverrideExcludes is true

V1 selection currently treats level selection, explicit assets, include folders, and include asset kinds as additive seed selectors. A kind rule does not replace selected levels; it extends the selected set before dependency expansion and exclusion filtering run.


# Level-Driven Packaging

For game teams, levels are the natural packaging unit. A profile should therefore allow users to select levels as first-class entries rather than hunting asset references manually. The planner then walks dependencies to determine the cook set.

| Selection Option | Behavior |
|---|---|
| Selected Levels Only | Cook only the explicitly chosen levels and their hard dependencies |
| Selected Levels + Startup | Always include StartupLevelAsset in addition to explicit levels |
| Selected Levels + Global Shared | Include assets tagged as GlobalShared or profile-defined always-cook labels |
| Whole Project | Cook every cookable asset under AssetRoot except excluded assets |


# Asset Cooking Pipeline

Asset cooking should be delegated to SnAPI.AssetPipeline through a Build Service adapter rather than reimplemented inside the package window. The build system's job is to select, schedule, cache, and stage. AssetPipeline's job is to transform source assets into runtime payloads.

1. Enumerate candidate assets from selection rules.
2. Resolve dependencies and construct final cook set.
3. Filter out editor-only or platform-incompatible assets.
4. Compute asset source hashes plus relevant cook settings hash.
5. Skip assets with valid cached cooked outputs when allowed.
6. Cook dirty assets in batches grouped by asset kind or pipeline affinity.
7. Write or update Cook Manifest with source hash, settings hash, output payload metadata, and dependency summary.
8. Bundle cooked payloads into one or more .snpak files according to chunk rules.

The current implementation already backs `ResolveAssetSelection`, `EnumerateAssets`, `CookAssets`, `WriteCookManifest`, and `WriteSnpak` with `AssetCookServiceAdapter` over `SnAPI.AssetPipeline`. The V1 backend currently supports selected levels, explicit assets, include folders, include asset kinds, exclude folders, exclude asset kinds, startup-level fallback, cook-manifest emission, source-content hashes, settings hashes, staged `.snpak` output, and explicit asset-selection provenance.

The editor packaging surface now opts into the real asset-cook adapter by default. Placeholder asset outputs remain only as an explicit fallback path for dry-run or intentionally reduced execution modes such as CLI `--skip-assets`.

Selection artifacts now record included assets, excluded assets, per-asset provenance entries, deterministic chunk ownership, and whether a selected source is cookable or should be staged verbatim. This keeps the selection result explainable instead of collapsing everything into a flat selected set. Selected content is now expanded through semantic asset dependencies reported by `SnAPI.AssetPipeline`, while GameFramework still owns the packaging policy that decides whether required, optional, or auxiliary dependencies should be included for a given build request.

The current dependency-expansion path uses import-only source analysis rather than a full cook when it needs dependency metadata for selection. This avoids selection-time failures caused by cook-only requirements on assets that are merely being analyzed for references.

Auxiliary package sources that are not cookable `IAsset` inputs, such as standalone shader sources or other raw files under `Assets/`, are still first-class package candidates. When a folder rule or kind rule selects those files, the package flow preserves their authored logical path under staged `Assets/` and copies them verbatim alongside generated `.snpak` bundles instead of forcing them through the cook pipeline.


# Asset Chunking and Snpak Layout

V1 uses a simple chunk model while leaving room for more advanced patch-friendly or streaming-friendly chunking later. The current backend supports:

- `Monolithic`: one `Primary` bundle containing all selected cooked assets.
- `SharedPlusPerLevel`: one `Shared` bundle for non-level content plus one bundle per selected level asset.
- `PerLabel`: chunking by the top-level authored asset folder as a practical V1 grouping rule.

`CustomGraph` remains a future extension point and currently falls back to the monolithic bundle path until a true authored chunk graph is implemented.

| Chunk Strategy | Description | Best For |
|---|---|---|
| Monolithic | Everything into one primary `.snpak` | Small projects and simplicity |
| Shared + PerLevel | Shared assets in one pack, each selected level in its own pack | Faster iteration and selective DLC later |
| PerLabel | Chunk by top-level authored content group in V1 | Larger projects that want stable content grouping before full label metadata lands |
| Custom Graph | User-defined chunking rules through profile schema | Future advanced teams and patch/DLC workflows |


# Code Compilation Strategy

Code compilation should be based on generated or maintained CMake integration under Code/. The package system should not attempt to become a second build system. It should orchestrate CMake rather than replace it with ad hoc shell orchestration.

Each project module should produce targets that can be used by the editor and runtime host. For V1 the simplest durable strategy is static or shared linkage into host executables depending on platform and configuration. Dynamic plugin loading can be added later, but the build metadata should already be structured so that future change is possible.

- The editor build path must be able to compile project runtime modules and editor-only modules.
- The packaged runtime build path must compile only runtime-relevant modules for the target platform.
- The same orchestration path must support plugin modules and newly added project modules without inventing a second workflow.
- Generated CMake should include engine dependency wiring, project include directories, module sources, and optional reflection generation participation.
- The system should support project-level presets for compiler, generator, toolchain file, and build directory layout.
- The engine root should accept an optional `SNAPI_PROJECT_ROOT_DIR` cache path so the same engine build can integrate one external project workspace at configure time.
- The initial configure/build contract is explicit: `cmake -S <EngineRoot> -B <ProjectIntermediateBuildDir> -DSNAPI_PROJECT_ROOT_DIR=<ProjectRoot>` followed by `cmake --build <ProjectIntermediateBuildDir> --target SnAPI.GameFramework.Runtime`.
- When callers do not provide an explicit engine-source override, `<EngineRoot>` should resolve from the compiled SnAPI.GameFramework source tree rather than the host process working directory.
- Build nodes should treat project build-file regeneration, CMake configure, and CMake build as separate reportable steps so failures are localized and cacheable.

The current editor packaging flow now opts into the real code-build adapter by default, so successful package runs produce discovered runtime binaries rather than placeholder marker files unless callers explicitly request the reduced placeholder path through CLI skip flags or execution options.


# Containerized Toolchains and Stable Build Environments

To make multi-platform builds reproducible and less dependent on the current developer workstation, the build system should support containerized toolchain environments as a first-class concept. The preferred execution model is to resolve supported targets against versioned Docker or OCI images that contain the known-good compiler, SDK, sysroot, generator, and auxiliary packaging tools for that target.

This is not only a CI concern. The editor and CLI should both be able to resolve the same profile into the same containerized build environment so a local package run and a CI package run are operating against the same target definition rather than whatever happens to be installed on one machine.

- Build profiles should be able to name an execution environment or toolchain image.
- Build requests should freeze the resolved container image, toolchain version, and relevant mounted paths as part of the request hash.
- Code build, cook, and packaging nodes should be able to execute either on the host or inside a container, depending on platform support and profile policy.
- Reports and manifests should record the container image or host toolchain details used for the build.
- Cached outputs must include the execution environment fingerprint so host-built and container-built outputs do not collide incorrectly.
- Platforms that cannot legally or practically be fully containerized should still use the same abstraction, with a host-resolved platform toolchain adapter standing in for the container image.

> Design principle: platform support should be defined by stable target environments, not by the accidental state of one developer workstation.


# Recommended Linking Model for V1

Use one host executable per mode and link project modules into that mode-specific executable. In practice this means the editor executable links project runtime modules and project editor modules, while the runtime executable links project runtime modules only. This keeps startup and debugging straightforward and avoids solving runtime module discovery, ABI boundaries, and hot-load edge cases all at once.

> Future-proofing note: even if V1 links modules statically or directly, keep module metadata rich enough that a later shift to shared objects or loadable bundles does not require descriptor surgery.

| Mode | Links |
|---|---|
| Editor | SnAPI.GameFramework + engine/editor modules + project runtime modules + project editor modules |
| Runtime | SnAPI.GameFramework + engine runtime modules + project runtime modules |
| Dedicated Server (future) | SnAPI.GameFramework server subset + project runtime/server modules |


# Generated CMake Integration

The project descriptor is not a build file. It feeds generated build fragments. The system should generate a stable top-level project include file under `Intermediate/Build/Generated/` and a checked-in minimal root include under `Code/` that references generated module wiring. Individual module roots should also contain a minimal `CMakeLists.txt` wrapper so `add_subdirectory()` can consume the tool-generated module fragment without renaming that fragment into the user-owned root file.

Generation should be idempotent and comment-stamped so users know which files are safe to edit and which are tool-owned.

The engine-side root `CMakeLists.txt` should treat `SNAPI_PROJECT_ROOT_DIR` as the explicit integration point for an external project workspace. When that cache path is provided and `<ProjectRoot>/Code/CMakeLists.txt` exists, the engine configure step should add that project code root as a subdirectory so generated project module wiring can link into `SnAPI.GameFramework.Editor` and `SnAPI.GameFramework.Runtime` without manual engine edits.

```
# Auto-generated by SnAPI Build System. Safe to regenerate.
add_subdirectory(${PROJECT_SOURCE_DIR}/Code/MyGame)
add_subdirectory(${PROJECT_SOURCE_DIR}/Code/MyGameEditor)
target_link_libraries(SnAPI.GameFramework.Editor PRIVATE MyGame MyGameEditor)
target_link_libraries(SnAPI.GameFramework.Runtime PRIVATE MyGame)
```

```
# Minimal bridge kept in Code/CMakeLists.txt
set(SNAPI_PROJECT_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/..")
include("${SNAPI_PROJECT_ROOT_DIR}/Intermediate/Build/Generated/ProjectModules.cmake" OPTIONAL)
unset(SNAPI_PROJECT_ROOT_DIR)
```

```
# Engine-side configure/build shape
cmake -S /SnAPI/Engine -B /Projects/MyGame/Intermediate/Build/Linux/Development/host-local \
    -DSNAPI_PROJECT_ROOT_DIR=/Projects/MyGame
cmake --build /Projects/MyGame/Intermediate/Build/Linux/Development/host-local \
    --target SnAPI.GameFramework.Runtime
```

Code build trees should be isolated by platform, configuration, and execution environment so incompatible CMake caches do not overwrite one another. The current V1 layout is:

```
Intermediate/Build/<Platform>/<Configuration>/<EnvironmentKey>/
```

The generated project-module bridge remains at `Intermediate/Build/Generated/ProjectModules.cmake` because it is descriptor-driven rather than compiler-cache state.

When a profile resolves to a containerized execution environment, the system should wrap that same configure/build shape in the chosen Docker or OCI runtime rather than invent a separate build protocol for containers.


# Reflection and Generated Code Participation

Some modules will participate in the reflection pipeline. The build system must understand that reflection generation is a codegen stage that happens before compile, and that generated outputs belong under Intermediate/Generated/<Module>/ rather than polluting source directories.

- Module metadata includes UseReflectionGen and generated include path settings.
- Generated reflection outputs are treated as node inputs for compile tasks.
- Build reports must show reflection generation separately from C++ compilation so failures are localized.


# Staging Directory Layout

The staging layout is the canonical pre-archive package tree. It must be predictable across platforms even if the final archive/container differs.

| Stage Path | Contents |
|---|---|
| Bin/ | Executables, copied non-system shared libraries, helper programs, bootstrap launchers |
| Assets/ | .snpak bundles, optional loose cooked assets for dev configurations |
| Config/ | Copied authored config files plus generated resolved runtime configuration and platform overrides |
| Metadata/ | Package manifest, build report summary, version file, chunk map, license notices if any |
| Logs/ | Build logs copied or summarized for local builds when enabled |
| Symbols/ | PDB/dSYM/debug symbol outputs when staged externally |
| Prereqs/ | Optional redistributables or installer dependencies in future |

The current staging implementation already creates the canonical tree, stages runtime binaries, stages `.snpak` bundles, copies selected auxiliary asset files under `Assets/` while preserving their authored relative hierarchy, copies authored `Config/` content when present, and writes `ResolvedRuntimeConfig.json` so packaged output can carry the resolved startup/bootstrap view explicitly.

Packaged runtime systems that need authored-asset types now resolve them through runtime asset-manager loading paths rather than assuming loose authored source files are present beside the package. This allows packaged Conduit class and graph refs to resolve from staged `.snpak` output instead of requiring the original authored source tree at runtime, while preserving `LoadAsset()` as the source-authored loading path.

On Linux, the current packaging path also copies non-system ELF shared-library dependencies into `Bin/` and rewrites packaged runtime search paths to `$ORIGIN`, so packaged binaries resolve sibling libraries from the staged output instead of falling back to the per-project `Intermediate/Build/...` tree.


# Package Manifest

Every build should emit a package manifest in JSON. This is the authoritative record of what the package contains and why. Without it, differences between builds become difficult to diagnose and explain.

| Manifest Field | Purpose |
|---|---|
| BuildId | Unique ID for this build invocation |
| ProjectId | Stable project identity |
| ProjectName | Human-readable project name |
| ProfileName | Resolved build profile |
| TargetPlatform | Platform identifier |
| Configuration | Debug/Development/Test/Shipping |
| ToolVersions | Engine/build tool versions |
| SourceRevision | Optional source control commit or working-tree fingerprint |
| IncludedLevels | Selected level asset IDs |
| OutputFiles | Relative paths, sizes, and hashes |
| SnpakFiles | Bundle names, chunk identifiers, contained asset counts |
| Modules | Participating runtime module names and authored linkage metadata |
| Warnings | Recorded non-fatal build warnings |

The current implementation emits `PackageManifest.json` from the staged filesystem tree through `PackageManifestService`. It enumerates staged files deterministically, records file sizes and hashes, captures staged `.snpak` bundle metadata, records participating runtime modules, and emits `StageFileHashes.json` alongside the package manifest for automation-friendly diffing.


# Validation System

Validation should be its own subsystem. It is not just a pre-build checklist. It should run in the editor continuously for active profiles where possible and again as a frozen snapshot before build execution.

- Project descriptor validation: schema, missing required fields, invalid relative paths
- Asset validation: missing startup level, unresolved asset refs, unsupported asset kinds for target
- Module validation: missing module root, missing CMake fragment, invalid dependency names
- Toolchain validation: compiler/generator availability, platform SDK presence, signing tool availability
- Profile validation: contradictory rules, missing output root, invalid archive format for target

The current V1 resolved-descriptor validation treats a missing non-URI startup level source asset as a blocking error. A descriptor that names `Levels/StarterLevel.level` or any other project-relative startup asset must resolve to a real file on disk before validation, planning, package execution, or retry flows are allowed to continue.

| Severity | Behavior |
|---|---|
| Error | Build blocked until resolved |
| Warning | Build allowed but warning visible in plan and report |
| Info | Non-blocking observation or suggestion |


# Diagnostics and Reporting

The build output should be split into structured events, per-stage logs, and human summaries. Users need scrolling logs during execution, but automation needs structured output. Both matter.

- Emit structured event stream with timestamps, stage IDs, node IDs, severity, message, and optional payload.
- Persist `BuildRequest.json` and `BuildPlan.json` into the build history folder before node execution begins so dry-run, CLI, editor preview, and post-failure inspection all reference the same frozen inputs.
- Write per-stage log files under Saved/BuildHistory/<BuildId>/.
- Write BuildReport.json with timing, cache hits, outputs, warnings, and failures.
- Write BuildSummary.txt or .md for quick human reading.
- Promote successful staged packages through `PackageOutputService` into a user-facing output root, and record the resolved output root, copied package directory, and optional archive path in `BuildReport.json`.
- Expose a read-side build history service that can enumerate prior runs, load `BuildReport.json`, and compare outputs or node outcomes between two builds.
- In the editor, provide expandable failure details with direct links to the offending asset, file, or profile setting.

The current implementation already emits structured execution events, writes `BuildRequest.json`, `BuildPlan.json`, `BuildReport.json`, `BuildSummary.md`, and per-stage log files, and keeps incomplete history entries visible so failed or interrupted runs remain inspectable.

The current editor implementation also preserves the active packaging session's `stdout` and `stderr` in the Packaging surface's `Console` tab. Shared build events and streamed CMake configure/build output are appended into the same rolling console buffer so one plan or package action can be inspected as one coherent session transcript. After execution completes, the editor also folds warning and error diagnostics from the structured build report and per-stage log files into the same console surface so users do not have to manually open `CodeBuild.*.log` or other stage logs just to see the blocking failure lines. Planning and package execution now run on a background worker thread from the editor shell, while the modal polls completion on the main thread and keeps the console/status view live. The editor retains a larger bounded raw transcript internally, but the modal renders a normalized recent tail window so extremely large build logs do not exhaust the UI packet glyph budget or expand into large blank regions. The shared `UIScrollContainer` now also supports pinned end-following and explicit descendant alignment requests so log-heavy surfaces can keep the newest output visible without hard-coding console-specific scrolling behavior into the editor shell.

The code-build adapter should always emit an explicit bounded parallel job count for `cmake --build` rather than a bare `--parallel` flag. Leaving generator concurrency unconstrained can make editor-triggered builds far more aggressive than intended on desktop workstations and can destabilize the host during large engine rebuilds.


# Incremental Build and Caching

Iteration speed matters enough that caching is not optional. Code and asset stages should both participate in invalidation and reuse.

1. Compute a request-level hash for the fully resolved build request.
2. Compute a module compile fingerprint from sources, public headers, generated code, compile definitions, toolchain version, and configuration.
3. Compute an asset cook fingerprint from source asset content, relevant import settings, pipeline version, target platform, and dependency summary.
4. Skip nodes with matching cache keys and existing verified outputs unless ForceRebuild or ForceRecook is set.
5. Cache keys and outputs should be inspectable in reports so debugging cache weirdness is not mystical.

The current implementation keeps the persistent node cache focused on cooked-asset and staged-output reuse, and those cache entries are validated against the current node inputs before restore. Code-build nodes intentionally rely on the underlying CMake build tree's own incremental behavior instead of cross-build artifact restore so engine/runtime source edits cannot be masked by an overly coarse package-level cache hit.

The current implementation backs this with `BuildCacheService`, which persists cache metadata and artifact paths under `Intermediate/BuildCache/<Platform>/<Configuration>/`. Cacheable build nodes can restore previously materialized outputs on a later identical request, and retry flows can reload both the frozen `BuildRequest.json` and the prior `BuildReport.json` from history so successful nodes are resumed by default even when the retry gets a new `BuildId`. A retry can still opt out of that behavior and force a full rebuild through `--rebuild-all`.


# Determinism and Reproducibility

Perfect bitwise reproducibility across every host may be unrealistic in early stages, but deterministic intent and inspectable differences are absolutely achievable. The design should therefore aim for stable ordering, normalized paths, sorted manifests, and fixed timestamp policy where reasonable.

- Sort asset enumeration and manifest output deterministically.
- Normalize all stored paths to a canonical relative representation when written to manifests.
- Avoid embedding wall-clock timestamps inside content bundles unless explicitly requested.
- Record host/tool versions and environment fingerprints in metadata so differences are explainable.


# Platform Abstraction

The system should define a platform abstraction layer around packaging, not around the entire engine. Platform packaging differs mainly in toolchain, output layout, signing, metadata, archive/container rules, and file naming. The core graph stays the same.

| Platform Hook Area | Examples |
|---|---|
| Toolchain Resolution | Docker/OCI image, compiler, generator, SDK, sysroot, toolchain file |
| Binary Naming | .exe vs no extension, library suffixes |
| Config Transform | Platform-specific runtime settings emitted into Config/ |
| Asset Format Overrides | Texture compression or shader formats later |
| Signing/Stamping | Windows signing, mobile provisioning, console package signing later |
| Final Container | Loose staged folder, zip archive, installer, store package |


# Build Profiles

Build profiles are the reusable human-facing abstraction over raw build requests. A profile should be named, diffable, inheritable, and exportable.

- Each project can define multiple named profiles such as WindowsDevelopment, WindowsShipping, LinuxShipping, DemoBuild, QARegression.
- Profiles may inherit from another profile, but inheritance depth should be limited so resolution remains understandable.
- The resolved profile shown in the UI must make inherited values visible.
- Profiles should also be able to select a containerized execution environment so the same profile means the same build environment in both local and CI execution.


# Profile Inheritance Rules

Profile inheritance should use predictable merge semantics.

| Value Type | Merge Rule |
|---|---|
| Scalar | Child overrides parent |
| Object | Recursive merge by key unless object is marked ReplaceEntireObject |
| Array of paths/rules | Append then normalize and deduplicate unless marked Replace |
| Explicit null | Clears inherited scalar or object when allowed |

For descriptor authoring, list fields may be written as a plain JSON array for the common append-and-deduplicate case, or as an object such as `{ "Values": [...], "Replace": true }` when the child profile needs to replace the inherited list explicitly. Explicit `null` clears inherited scalar values, and object sections such as `Archive` may additionally opt into `ReplaceEntireObject` when a child profile needs a clean replacement baseline before applying overrides.


# Output Naming Convention

Packages should follow a predictable naming pattern. This matters for humans, automation, and reducing ambiguity between similar output folders.

```
<ProjectName>_<ProfileName>_<Platform>_<Configuration>_<BuildId>/
<ProjectName>_<Platform>_<Configuration>.zip
```

The current implementation derives the copied package-directory name from project name, resolved profile name or `AdHoc`, target platform, configuration, and `BuildId`. The optional archive name currently defaults to `<ProjectName>_<Platform>_<Configuration>.zip`.


# Project Runtime Boot Flow

Packaging and project creation are not separate from runtime boot. The generated project metadata must line up with how GameProjectRuntime already expects project fields such as AssetRoot, StartupLevelAsset, and DefaultRenderSettingsAssetId.

1. Open project descriptor.
2. Resolve asset root and project root.
3. Create project-scoped AssetManager.
4. Mount one or more .snpak bundles from staged Assets/.
5. Load startup level asset.
6. Apply default render settings asset if configured.
7. Instantiate runtime and gameplay host using generated default game class and game mode class as configured.

The current V1 runtime bootstrap defers gameplay-host autostart until after the startup level and optional project default render settings have loaded. That keeps auto-created local players and initial possession from being created twice during project bootstrap.

For renderer-facing runtime assets, the current V1 implementation now treats meshes and materials as shared runtime objects loaded through `GetRuntimeShared(...)`. Static and skeletal mesh runtime objects keep both the shared vertex stream source and the baked material-instance refs together so mesh components can bind the singleton runtime mesh, resolve singleton material instances for each slot, and then layer any per-component material overrides on top without reloading authored assets or deserializing cooked payloads on the hot path.

For packaged builds, the runtime executable should not require a `.snproj.json` argument at launch time. The packaged runtime bootstrap contract is:

- the staged output writes `Config/ResolvedRuntimeConfig.json`
- the packaged runtime auto-discovers that file relative to `Bin/` when launched without explicit arguments
- runtime bootstrap resolves `Assets/` relative to the staged package root, mounts packaged `.snpak` bundles, and uses the staged bootstrap config instead of an authored project descriptor
- explicit bootstrap or project-descriptor paths remain supported for development/runtime launches outside packaged output


# Editor and Runtime Module Participation

The project module model should distinguish runtime-only modules from editor-only modules. The project wizard should optionally generate both.

| Module Type | Linked Into Editor | Linked Into Runtime | Typical Uses |
|---|---|---|---|
| Runtime | Yes | Yes | Gameplay, systems, assets, net code |
| Editor | Yes | No | Custom inspectors, build panels, import tools, editor viewport helpers |
| Shared | Yes | Yes | Pure utility libraries with no editor dependency |
| Developer | Optional | No | Internal tooling, tests, diagnostics |


# CLI and Automation

Everything the editor can do for project creation and packaging should map to CLI commands. The editor should basically be a rich front-end over the same build services, not a separate species.

- CLI should emit both human-readable console output and optional JSON event stream.
- Exit codes must distinguish validation failure, build failure, internal error, and cancelled build.
- CLI should support dry-run and plan-only modes.
- CLI should be able to select or override the execution environment, including named Docker/OCI toolchain images, without introducing a separate code path from the editor.
- The current implementation exposes a standalone `SnAPI.GameFramework.Build` target backed by `BuildCliService`.
- The current implementation supports `create-project`, `create-plugin`, `add-module`, `validate`, `package`, `retry`, and `history`.
- The current implementation supports plan-only package execution, final copied package output, optional zip archive emission, ad hoc request overrides, JSON output, JSON event capture, build-history enumeration, build-history comparison, cooperative cancellation recording, and retry from a previously frozen build request.
- The current implementation resumes successful nodes from the source build report by default during `retry`, and supports `--rebuild-all` when the caller wants a clean retry without prior-node reuse.

```
snapi build create-project --name MyGame --dest /Projects/MyGame
snapi build create-plugin --name Inventory --dest /Projects/Inventory
snapi build add-module --project /Projects/MyGame/project.snproj.json --name Combat --type Runtime
snapi build package --project /Projects/MyGame/project.snproj.json --profile WindowsShipping
snapi build package --project /Projects/MyGame/project.snproj.json --profile LinuxShipping --container ghcr.io/snapi/toolchains/linux-clang:1.0
snapi build package --project /Projects/MyGame/project.snproj.json --platform Windows --config Development --levels Levels/Main.level --archive
snapi build validate --project /Projects/MyGame/project.snproj.json --profile WindowsShipping
snapi build retry --project /Projects/MyGame/project.snproj.json --from-build-id 20260322-140500-main
snapi build retry --project /Projects/MyGame/project.snproj.json --from-build-id 20260322-140500-main --rebuild-all
snapi build history list --project /Projects/MyGame/project.snproj.json --json
snapi build history compare --project /Projects/MyGame/project.snproj.json --left 20260322-140500-main --right 20260322-141200-main
```


# Automation-Friendly Output

Build reports should include enough machine-readable detail that CI can archive them, compare them, and gate on them.

| Output | Why It Exists |
|---|---|
| BuildReport.json | Complete structured result |
| PackageManifest.json | Contents of the final output package |
| CookManifest.json | What assets were selected, cooked, skipped, and bundled |
| BuildSummary.md | Readable summary artifact for CI job logs or release notes |
| StageFileHashes.json | Stage-root relative size/hash list for diff/comparison support |


# Security and Trust Boundaries

A build system runs code, reads descriptors, and writes outputs. It therefore needs some baseline security hygiene even in local-only contexts.

- Project descriptors should not allow arbitrary shell command injection as a normal configuration mechanism.
- Path traversal outside the project root should be rejected unless explicitly allowed for approved external asset mounts.
- Signing secrets or certificates must not be stored in the project descriptor; instead reference secure local/CI secret stores.
- Generated files should be clearly stamped to reduce accidental manual edits to tool-owned files.


# Versioning and Migration

The descriptor schema must carry a SchemaVersion. The build tool must support explicit migration steps and dry-run migrations. Migration complexity should be designed for up front rather than added reactively later.

1. Load descriptor and inspect SchemaVersion.
2. If old but supported, apply migration steps in memory and offer to write upgraded descriptor.
3. If too old or too new, produce a clear compatibility error.
4. Record TemplateVersion and CreatedWithToolVersion so starter templates can evolve in a controlled way.


# Failure Handling and Recovery

Builds fail. The system should fail in a way that preserves evidence and makes rerun cheap.

- Every build gets a BuildId and dedicated history folder under Saved/BuildHistory/.
- Partial stage outputs are either marked incomplete or written to temp paths then atomically promoted.
- Cancelled builds remain browsable in history with cancellation reason and finished nodes.
- The package window should offer Rebuild Failed Nodes, Rebuild All, and Open Last Successful Output.

The current backend already supports retrying a prior run from its stored `BuildRequest.json`, which keeps retry behavior aligned with the original frozen request rather than re-resolving a potentially changed live descriptor or profile. It also reloads the prior `BuildReport.json` so successful nodes are resumed by default into the new history and stage roots, while `--rebuild-all` disables that reuse when a full rebuild is preferred.


# Testing Strategy

The build system is one of those areas where tiny mistakes waste days. Test coverage therefore needs breadth.

| Test Layer | Examples |
|---|---|
| Unit | Path normalization, profile merge, selection rule evaluation, manifest writing |
| Component | Build planner to graph generation, project creation file emission, validation services |
| Integration | End-to-end cook/package of a sample project for Windows and Linux dev targets |
| Golden File | Descriptor generation, manifest generation, starter CMake output |
| Performance | Incremental package after one asset edit, after one code edit, after profile-only edit |
| Failure Injection | Missing SDK, compile failure, cook failure, locked output file, invalid descriptor |


# Open Questions and Design Tradeoffs

Some areas should remain explicitly marked as tradeoffs rather than pretending the answer is already obvious.

| Question | Current Recommendation | Future Option |
|---|---|---|
| Static versus dynamic project modules | Use direct linkage in V1 | Move to loadable runtime modules later if needed |
| Single .snpak or chunked packs | Support both monolithic and shared+per-level in V1 | Advanced patch/DLC chunk graphs later |
| Generated versus user-authored root CMake | Hybrid: tool-generated include plus editable module fragments | Full generation or full manual later if needed |
| Shared derived-data cache | Local first | Network/shared cache when team scale demands it |
| Host toolchains versus containerized toolchains | Prefer versioned Docker/OCI images where supported | Fall back to host adapters only for restricted platforms |


# Implementation Plan

The design is intentionally partitioned into ordered milestones. This reduces the odds of a giant branch that changes everything and proves nothing.

1. Milestone 1: Add descriptor models, JSON serialization, schema validation, migration hooks, and workspace path helpers.
2. Milestone 2: Implement project creation, plugin creation, and module creation services plus starter templates.
3. Milestone 3: Implement Build Profile model, inheritance resolver, execution-environment resolution, Build Request freezing, and validation service.
4. Milestone 4: Implement Build Graph core types, node scheduling, event emission, and report writing.
5. Milestone 5: Integrate asset selection and AssetPipeline cook/bundle nodes.
6. Milestone 6: Integrate CMake generation/configure/build nodes for project and plugin modules, including containerized toolchain execution.
7. Milestone 7: Implement staging, manifests, archive/sign hooks, and editor UI shell.
8. Milestone 8: Add CLI parity, build history, retry/resume support, and tests.


# Acceptance Criteria

A feature this central needs hard acceptance criteria, not informal expectations.

- New project creation succeeds from the editor and CLI and produces a compileable starter project.
- A starter project can be opened, compiled, and run in editor and runtime modes.
- Packaging can include one or more selected levels and exclude folders by rule.
- Asset cooking writes a Cook Manifest and final .snpak bundle(s).
- Runtime package stage contains Bin/, Assets/, Config/, Metadata/ at minimum.
- A package manifest is emitted and lists built binaries and bundles with hashes.
- A second package run after no content changes reuses caches and reports cache hits.
- Validation blocks bad descriptors and missing startup levels before build execution.
- CLI and editor build results match for the same frozen Build Request.


# Appendix A: Proposed .snproj.json Schema

The following extended sample illustrates the intended shape. It is not a requirement that every project fill every field on day one, but the schema should be designed to grow into this shape rather than needing a rewrite later.

```
{
  "Format": { "SchemaVersion": 1, "MinimumToolVersion": "0.9.0" },
  "Project": {
    "Name": "MyGame",
    "DisplayName": "My Game",
    "Company": "My Studio",
    "ProjectId": "6e8d0f01-0d6c-4fb7-95c7-4d70b5450001",
    "Description": "Starter SnAPI game project"
  },
  "Paths": {
    "AssetRoot": "Assets",
    "CodeRoot": "Code",
    "ConfigRoot": "Config",
    "IntermediateRoot": "Intermediate",
    "SavedRoot": "Saved"
  },
  "Startup": {
    "StartupLevelAsset": "Levels/Main.level",
    "DefaultRenderSettingsAssetId": "Render/DefaultRenderSettings.asset",
    "DefaultGameClass": "MyGame::MyGame",
    "DefaultGameModeClass": "MyGame::MyGameMode"
  },
  "Modules": [
    {
      "Name": "MyGame",
      "Type": "Runtime",
      "Root": "Code/MyGame",
      "PublicDependencies": ["SnAPI.GameFramework"],
      "PrivateDependencies": ["SnAPI.AssetPipeline"],
      "LoadInEditor": true,
      "LoadInRuntime": true,
      "UseReflectionGen": true
    },
    {
      "Name": "MyGameEditor",
      "Type": "Editor",
      "Root": "Code/MyGameEditor",
      "PublicDependencies": [],
      "PrivateDependencies": ["MyGame", "SnAPI.GameFramework"],
      "LoadInEditor": true,
      "LoadInRuntime": false
    }
  ],
  "Profiles": {
    "WindowsDevelopment": {
      "Platform": "Windows",
      "Configuration": "Development",
      "SelectedLevels": ["Levels/Main.level"],
      "IncludeFolders": ["Assets/Shared"],
      "ExcludeFolders": ["Assets/EditorOnly"],
      "ChunkStrategy": "SharedPlusPerLevel",
      "Archive": { "Enabled": false }
    }
  }
}
```


# Appendix B: Proposed Starter Module CMake Fragment

This is the kind of generated starter fragment the project wizard should emit.

```
add_library(MyGame
    src/MyGame.cpp
    src/MyGameMode.cpp
)

target_include_directories(MyGame PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(MyGame PUBLIC
    SnAPI.GameFramework
)
```


# Appendix C: Build History Layout

Build history should be per-project and easy to inspect manually.

```
Saved/
  BuildHistory/
    <BuildId>/
      BuildRequest.json
      BuildPlan.json
      BuildReport.json
      BuildSummary.md
      Stage/
        <Project>_<Profile>_<Platform>_<Configuration>/
          Metadata/
            PackageManifest.json
            StageFileHashes.json
      Logs/
        Preflight.log
        Planning.log
        Code.log
        Assets.log
        Staging.log
        Finalize.log
      Manifests/
        CookManifest.json
```


# Appendix D: Suggested Editor Services

The system maps naturally to a service-oriented editor/runtime architecture.

| Service | Responsibility |
|---|---|
| ProjectCreationService | Wizard orchestration and file generation |
| PluginCreationService | Wizard orchestration and file generation for plugin workspaces and starter plugin modules |
| ModuleCreationService | Add new project or plugin modules through explicit project/plugin entrypoints, emit starter module source, and regenerate descriptor/CMake integration |
| ProjectDescriptorService | Load/save/migrate/validate descriptor |
| PluginDescriptorService | Load/save/validate plugin descriptors and resolve plugin-relative workspace paths |
| BuildProfileService | Manage named profiles and resolution |
| BuildRequestService | Freeze project descriptors, named profiles, and one-shot overrides into deterministic resolved build requests |
| BuildPlannerService | Resolve Build Request and generate Build Graph |
| BuildCacheService | Persist cacheable node metadata and restore previously materialized outputs for identical frozen requests |
| BuildExecutionService | Schedule and execute graph nodes, emit structured events, and write request/plan/report/history artifacts |
| PackageOutputService | Promote staged outputs into final copied package directories and optional archive files |
| AssetCookServiceAdapter | Bridge to SnAPI.AssetPipeline cook APIs |
| CodeBuildServiceAdapter | Bridge to project build-file generation, execution-environment resolution, and CMake configure/build toolchain |
| PackageManifestService | Enumerate staged package contents, hash staged files, and emit `PackageManifest.json` plus `StageFileHashes.json` |
| PackageReportService | Write reports, manifests, summaries, history |
| BuildHistoryService | List prior runs, load build reports, compare results, and surface incomplete history entries |
| BuildCliService | Parse CLI commands and dispatch them onto the shared project/build services without a separate backend path |
| EditorBuildService | Build editor-facing requests from the active project, execute package/retry flows, and expose project-local build history without shelling through the CLI |


# Appendix E: Suggested C++ Type Skeletons

These skeletons are intended to guide implementation toward the right shape.

```
struct BuildRequest;
struct BuildProfile;
struct ResolvedBuildRequest;
struct BuildValidationIssue;
struct BuildGraph;
struct BuildGraphNode;
struct BuildCliResult;
struct CookManifest;
struct PackageManifest;

class IBuildNodeExecutor;
class BuildCliService;
class BuildRequestService;
class BuildPlannerService;
class BuildExecutionService;
class ModuleCreationService;
class PluginCreationService;
class PluginDescriptorService;
class ProjectCreationService;
class ProjectDescriptorService;
```


# Detailed Validation Rules

The validation service should expose rule IDs so issues can be suppressed, promoted, or waived in a controlled way. Example rule IDs: Project.MissingStartupLevel, Paths.AssetRootMissing, Modules.UnknownDependency, Platforms.Windows.MissingSdk, Packaging.OutputInsideProjectSource.

Suppression should be allowed only for warnings and info by default, and should live in project-local config or user-local Saved/ preferences depending on whether the suppression is meant to be shared.


# Detailed Build Events

Structured build events should include event kind, timestamp, stage, node ID, severity, user-facing text, machine payload, and optional correlation ID. This allows both rich UI progress and automation consumers.

Event kinds should include BuildStarted, BuildPlanReady, ValidationIssueRaised, NodeQueued, NodeStarted, NodeProgress, NodeCacheHit, NodeFinished, NodeFailed, BuildCancelled, BuildFinished.


# Build Cancellation Semantics

Cancellation should be cooperative. Nodes should receive a cancellation token and periodically check it. Some nodes, like final bundle writes, may need to reach a safe point before aborting to avoid corrupt outputs.

The report must distinguish Cancelled by User, Cancelled by Dependency Failure, and Cancelled by Shutdown.

The current implementation performs cancellation checks between planned nodes, records a structured cancellation reason in `BuildReport.json`, emits `BuildCancelled`, and keeps the remaining unstarted nodes visible in the report as cancelled/skipped records rather than silently truncating execution history.


# Temporary and Promoted Outputs

Nodes that write durable outputs should write to temp paths first where practical, then atomically rename or promote. This prevents partially written packages from being mistaken for valid outputs after crashes or cancellation.

Stage promotion should be directory-scoped at the last reasonable moment to avoid long periods with missing outputs.


# Profile UX Details

Profiles should support duplicate, rename, export, import, compare against another profile, and reset-to-inherited actions.

The compare view should show changed keys grouped by section, not a raw JSON diff unless the user explicitly asks for it.


# Asset Rule Precedence

A final resolved asset selection should be explainable. For any asset in or out of the build, the UI should be able to say why: explicitly selected, dependency of selected level, included by folder rule, excluded by label rule, filtered by platform, filtered as editor-only, and so on.

The current V1 backend does track provenance tags on inclusion and exclusion decisions. `ResolveAssetSelection` writes included assets, excluded assets, and ordered per-asset provenance entries so later history, reports, and editor surfaces can explain why a concrete asset was kept or removed. Dependency-driven expansion now participates in that same provenance model, and future work should extend it further for more advanced asset-label metadata and richer custom resolvers.


# Potential Future Features

The design leaves room for downloadable content chunks, patch manifests, remote/shared derived data cache, console package generators, runtime-loadable project modules, package diff tools, and content-addressed bundle storage.

Nothing in V1 should preclude those directions, but V1 should not try to implement them all at once.
