# Development Workflow

This workflow is mandatory for AI-assisted code and feature changes.

## Code Change Lifecycle

For every code or feature change:

1. If no relevant active plan exists, run a design/onboarding conversation before
   creating one. Clarify the feature goals, requirements, constraints, public API
   shape, GameFramework/lower-level module boundaries, validation strategy,
   risks, milestones, and dependency choices. Treat that discussion as the source
   material for the plan.
2. Create or update an execution plan in `docs/exec-plans/active/` before editing
   source code.
3. Create or switch to a git branch for that plan.
4. Record the branch name in the plan before implementation starts.
5. Keep the plan updated as facts change, especially scope, touched files,
   validation, and completion status.
6. Implement the change on the plan branch.
7. Validate with the commands listed in the plan and in `docs/VALIDATION.md`.
8. When the plan is completely finished, merge the plan branch back into `main`.
9. Move the plan from `docs/exec-plans/active/` to `docs/exec-plans/completed/`
   in the merge/completion patch.

Docs-only framework migrations may be performed in the current branch when the
user explicitly requests it. Source code and feature changes still require an
active plan.

Do not leave completed work in `active/`. Do not mark a plan complete until its
validation section records the commands that were run and their results.

## Dependency Rules

Agents must not vendor dependencies by running `git clone` into the repository,
copying external source drops into `ThirdParty/`, or otherwise creating ad hoc
local dependency checkouts as part of implementation. New dependency work must be
captured in the active plan and follow this resolution order:

1. explicit user-provided source directory, installed SDK root, or package root
   passed through a documented cache variable;
2. system/package-manager or SDK installation discovered by `find_package`,
   `pkg-config`, or an equivalent config-mode package;
3. an existing project-approved CMake `FetchContent` fallback.

Adding a new fallback downloader, `FetchContent` declaration, vendored source
directory, submodule, or local third-party copy requires explicit user approval
and must be recorded in the active plan before implementation.

## New Plan Onboarding

When a new plan is needed, pause before writing the plan file and lead a focused
onboarding session with the user. Cover:

- the user-visible outcome and why it belongs in GameFramework;
- must-have requirements, nice-to-have requirements, and non-goals;
- public API shape and ownership/lifetime/threading expectations;
- GameFramework-owned contracts versus lower-level module boundaries;
- asset source/cooked data implications;
- editor/runtime/Play-In-Editor behavior;
- performance, allocation, storage, and scalability expectations;
- dependency, build flag, and stripability implications;
- tests, examples, docs, and validation;
- staged milestones that can land safely.

If the user explicitly asks to skip onboarding, record that in the plan notes
along with the assumptions that would normally have been confirmed.

## Branch Rules

- Branch names should describe the plan.
- One branch should map to one active plan unless the plan explicitly records why
  it shares a branch with related work.
- Before starting implementation, check the current branch with
  `git branch --show-current`.
- If another agent is already working in the repository, do not change branches,
  stage files, or run broad formatting/build operations without coordination.

## Plan Status

Every active plan should include:

- Status: `active`, `blocked`, `ready-for-merge`, or `completed`.
- Branch: the git branch that owns the work.
- Integration base: normally `main`.
- Scope: what will change and what will not change.
- Files/contracts expected to change.
- Validation commands.
- Completion criteria.

Use `docs/exec-plans/TEMPLATE.md` for new plans.

## Merge And Completion

A plan is complete only when:

- the implementation branch has been merged back into `main`;
- validation has passed or documented failures have been accepted explicitly;
- the plan has moved to `docs/exec-plans/completed/`;
- references from `AGENTS.md`, `docs/README.md`, and related docs no longer
  point at the completed plan as active work.
