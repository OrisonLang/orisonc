# ADR-0009: Request-Aware Object Cache

## Status
Accepted

## Context

Explicit build plans recompiled every reachable source package during each generic-specialization fixed-point round and
again on later invocations. Reuse must preserve dependency-interface changes, production compiler options, discovered
generic requests, and link-library metadata without relying on timestamps or ambient directories.

## Decision

A version-1 build plan may opt into caching with `cache <directory>`. Relative cache paths resolve from the plan
directory. Plans without this record perform no cache reads or writes.

Each source-package cache key is a SHA-256 digest of a cache-schema identifier, production compiler-profile identifier,
package name, source bytes, canonical reachable package interfaces, and canonical specialization requests targeting
that package. Imported interfaces and requests supplied through driver options participate in the same canonical data.

An entry stores object bytes, discovered specialization requests, and link libraries in separate files. A readiness
record is published last. A hit restores all three outputs so generic fixed-point scheduling behaves the same as a
fresh compilation. Cache write failures stop the build rather than silently changing performance or persistence.

## Consequences

Repeated unchanged builds reuse object emission, while source, interface, or relevant specialization changes select a
new entry. Intermediate fixed-point states may remain cached and can accelerate later builds. The compiler-profile or
cache-schema identifier must advance whenever production compilation behavior changes without an input-format change.
The initial implementation does not prune entries or coordinate cache sharing across machines.

## Follow-up work

- Add deterministic cache pruning and inspection commands.
- Derive the compiler-profile identifier from release/build identity when that identity is stable.
- Add output checksums if cache directories become shared across untrusted storage.
