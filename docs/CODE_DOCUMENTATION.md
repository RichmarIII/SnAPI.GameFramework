# Code Documentation Rules

## Read This When

- you are adding or changing public APIs, comments, Doxygen output, generated
  docs, examples, tutorials, or documentation layout
- you need documentation quality requirements without loading every architecture
  document

## Related Context

- `ARCHITECTURE.md` for the architecture context router
- `CODING_STYLE.md` for source layout and naming rules
- `docs-src/` for authored MkDocs inputs
- `docs/site/` for generated MkDocs output

## Public API Documentation

Public APIs should have concise comments when behavior is not obvious from the
type or function name. Comments should explain ownership, lifetime, threading,
side effects, validation expectations, and serialization implications when those
matter.

Do not write comments that repeat the type name or narrate obvious assignments.
Prefer comments that protect the next maintainer from a real mistake:

- whether a pointer is borrowed or owned
- whether a handle may be stale and must be resolved
- whether an API is main-thread only
- whether `World::EndFrame()` must run for the change to flush
- whether an asset type is source-authored or cooked runtime data
- whether a renderer object is retained by GameFramework or Renderer.New

## Tutorials And Generated Docs

Authored user docs live under `docs-src/`. Generated site output lives under
`docs/site/` and should not be edited by hand.

The generated API markdown under `docs-src/api/` is produced from Doxygen XML by
`tools/GenerateMkDocsApi.py`. Treat it as generated output even though it is an
input to the final MkDocs build.

Tutorials should use public APIs and normal host workflows. Do not teach users to
reach into private implementation files, generated reflection output, or
lower-level module internals.

## Validation

When public API comments, tutorials, generated docs, or docs navigation change,
run the docs target listed in `docs/VALIDATION.md` when practical. If docs
validation cannot run because local tools or network access are unavailable,
report the exact command and failure reason.
