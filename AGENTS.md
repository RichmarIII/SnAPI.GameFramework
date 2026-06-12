# SnAPI.GameFramework Agent Guide

This file is the short entry point for AI-assisted work in this repo. Keep it
brief: architecture, validation rules, and long-running plans live in `docs/`.

## Source Of Truth

- Architecture context router: `docs/ARCHITECTURE.md`
- Runtime/core architecture: `docs/architecture/RUNTIME_CORE.md`
- Asset pipeline architecture: `docs/architecture/ASSET_PIPELINE.md`
- Editor and UI architecture: `docs/architecture/EDITOR_AND_UI.md`
- Rendering integration architecture: `docs/architecture/RENDERING.md`
- Conduit and scripting architecture: `docs/architecture/CONDUIT_AND_SCRIPTING.md`
- Project/build/package architecture: `docs/architecture/PROJECTS_BUILD_AND_PACKAGING.md`
- Build and dependency architecture: `docs/architecture/BUILD_FLAGS_AND_DEPENDENCIES.md`
- Testing and diagnostics architecture: `docs/architecture/TESTING_VALIDATION_AND_DIAGNOSTICS.md`
- Code style and source layout: `docs/CODING_STYLE.md`
- Code documentation rules: `docs/CODE_DOCUMENTATION.md`
- Mandatory code-change workflow: `docs/WORKFLOW.md`
- Build and test commands: `docs/VALIDATION.md`
- Known cleanup and debt queue: `docs/TECH_DEBT.md`
- Execution plan inventory: `docs/exec-plans/`

Read `docs/ARCHITECTURE.md` before making architectural changes, then read only
the focused docs it routes you to. Read any relevant active plan before
continuing planned work after a context reset.

## Repository Map

- Durable engineering docs: `docs/`
- Generated MkDocs site output: `docs/site/`
- Authored MkDocs inputs: `docs-src/`
- Historical/domain design notes: `Docs/GameFramework/`
- Target compiled modules: `Modules/`
- Current public headers awaiting module-layout migration: `include/`
- Current implementation sources awaiting module-layout migration: `src/`
- Samples and integration hosts: `examples/`
- Unit and regression tests: `tests/`
- Build and support tools: `tools/`
- Benchmarks: `benchmarks/`

## Working Rules

- For any code or feature change, create or update an active plan in
  `docs/exec-plans/active/` before editing source code.
- If no relevant active plan exists yet, run a design/onboarding discussion with
  the user before writing the plan unless the user explicitly asks to skip it.
- Code and feature work must happen on a git branch tied to the active plan;
  write the branch name in the plan before implementation starts.
- When a plan is complete, merge its branch back into `main`, then move the plan
  from `docs/exec-plans/active/` to `docs/exec-plans/completed/`.
- Target source layout is `Modules/`. Every compiled module owns its own
  `Public/`, `Private/`, `Dependencies.cmake`, and `CMakeLists.txt` contract.
- Current `include/` and `src/` paths are legacy implementation facts, not the
  desired future layout. Do not add new public surface there unless an active
  migration plan explicitly scopes it.
- Preserve public gameplay/runtime contracts unless architecture docs explicitly
  call for a migration.
- Prefer public APIs in examples and tests; do not move application logic into
  framework internals to make an example pass.
- Follow `docs/CODING_STYLE.md` before broad source edits, especially naming,
  one-primary-type-per-file rules, helper placement, and include boundaries.
- For behavior changes, add or update focused tests in `tests/`.
- For architecture-affecting changes, update the relevant doc in the same patch.
- For public API, tutorial, or generated-doc changes, follow
  `docs/CODE_DOCUMENTATION.md`.
- Do not vendor dependencies by cloning external repositories into this tree or
  copying ad hoc third-party drops. New dependency mechanisms require explicit
  user approval and must be recorded in the active plan.
- Do not treat generated build output or generated docs as source. Avoid manual
  edits under `build*/`, `docs/site/`, and generated `docs-src/api/` output.

## Expected Validation

Use the smallest validation that proves the change, then broaden when touching
shared behavior. The canonical command list is in `docs/VALIDATION.md`.
