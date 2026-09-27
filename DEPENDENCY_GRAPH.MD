# EDP-Command Dependency Graph

Current mandatory production dependencies:

```text
EDP-Command
  +-- EDP-System   compile-time Composition and handler-provider resolution
  +-- EDP-Memory   typed object lifetime and ownership transfer
  `-- EDP-Clock    canonical Duration / MonotonicTimestamp vocabulary
```

EDP-System is directly consumed by the Command Composition domain, exclusive `Handler<TCommand>` capability, exactly-one handler requirement/provider resolution and the application Bootstrap binding. EDP-Memory is directly consumed by Command request/response/result lifetime and transfer paths. EDP-Clock is directly consumed by `CommandTypes.hpp` for canonical time vocabulary.

The active architecture also requires EDP-Threading-backed bounded waiting. That reconciliation slice is not represented here as a completed production edge until Command source actually consumes the Threading capability; package metadata must not get ahead of implementation.

The PlatformIO demo projects may name additional coherent sibling repositories needed by the selected framework/dependency chain; those are demo/build dependencies and are not automatically Command production dependency edges.

Explicit non-dependencies:

```text
Transport / Serialisation / Security
              |
              v
        integration adapter
              |
              v
          EDP-Command
```

Those domains may integrate toward Command through adapters. EDP-Command must not depend back toward them. Command also performs no runtime service discovery: application-owned provider instances are wired through the immutable compile-time EDP-System Composition topology.
