# EDP-Command Dependency Graph

Current mandatory production dependencies:

```text
EDP-Command
  +-- EDP-Memory   typed object lifetime and ownership transfer
  `-- EDP-Clock    canonical Duration / MonotonicTimestamp vocabulary
```

EDP-Memory is directly consumed by Command request/response/result lifetime and transfer paths. EDP-Clock is directly consumed by `CommandTypes.hpp` for canonical time vocabulary.

There are currently **no optional production dependencies**.

The PlatformIO demo projects may name additional coherent sibling repositories needed by the selected framework/dependency chain; those are demo/build dependencies and are not Command production dependency edges.

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

Those domains may integrate toward Command through adapters. EDP-Command must not depend back toward them. EDP-Threading is likewise not a current Command package dependency: the implemented Runtime creates no threads and exposes no wait method. Any future Threading-backed waiting/execution integration must be introduced explicitly rather than documented as already present.
