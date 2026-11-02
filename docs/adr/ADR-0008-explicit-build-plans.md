# ADR-0008: Explicit Build Plans

## Status
Accepted

## Context

Separate package compilation required callers to emit package interfaces, collect generic specialization requests,
route those requests to producer compilation, select object images, and invoke the linker manually. Implicit filesystem
discovery would hide dependency selection and make package resolution sensitive to ambient directory state.

## Decision

The compiler driver accepts a versioned text build plan through `orisonc --build-plan <plan> -o <executable>`. Version 1
starts with `orison-build-plan 1`, names one entry package, and maps each package to one source file. Relative source
paths resolve from the build plan directory; the remaining path text may contain spaces.

A package may instead map to a prebuilt package-interface artifact and object artifact through separate `interface`
and `object` records. Prebuilt transitive package requirements use `depends` records, and required host libraries use
`library` records. These records keep dependency and linker inputs explicit without extending package-interface
artifacts or inspecting ambient directories. A package cannot mix source and prebuilt mappings.

The driver parses only modules reachable through named imports from the entry package. Every reachable import must map
to a plan package, and each source package declaration must match its mapping. The driver builds typed package
interfaces for the reachable graph, compiles the graph until the canonical generic-specialization request set is
stable, merges link libraries, and links only reachable object images. This is build metadata, not Orison syntax.
Prebuilt packages contribute their parsed interface, object image, declared dependencies, and declared host libraries;
they are not recompiled.

## Consequences

Multi-package generic programs can build and link through one deterministic driver command without implicit package
search. Unreachable plan modules do not contribute interfaces, objects, entry points, or diagnostics. The initial
implementation recompiles reachable modules while specialization requests grow; it does not cache intermediate objects.
Each source package currently maps to one source file, and every reachable package dependency must be present in the
plan. A prebuilt generic object must already contain every specialization required by its consumers.

## Follow-up work

- Add incremental caching keyed by source, interface, options, and specialization requests.
- Support multiple source files per package after package-level semantic merging is defined.
