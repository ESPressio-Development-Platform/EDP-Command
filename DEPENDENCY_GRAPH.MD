# EDP-Command Dependency Graph

Direct architectural dependencies:

```text
EDP-Command
  +-- EDP-Memory      typed object lifetime and ownership transfer
  +-- EDP-Clock       canonical Duration / MonotonicTimestamp vocabulary
  +-- EDP-Threading   bounded wait/wake capability used by asynchronous Handle waiting
  `-- EDP-System      Composition/topology integration contract
```

The current local-runtime core directly includes Memory and Clock. Threading/System remain declared architectural dependencies for wait/provider composition as those provider bindings are integrated.

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

Those domains may depend toward Command through adapters. EDP-Command must not depend back toward them.
