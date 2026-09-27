# EDP-Command Dependency Graph

Current mandatory production dependencies:

```text
EDP-Command
  +-- EDP-System     compile-time Composition/Architecture and provider resolution
  +-- EDP-Memory     typed object lifetime and ownership transfer
  +-- EDP-Threading  bounded finite wait/wake capability
  `-- EDP-Clock      canonical Duration / MonotonicTimestamp vocabulary
```

EDP-System supplies the Command Composition domain, exclusive `Handler<TCommand>` resolution and cross-domain Architecture used by Bootstrap. EDP-Memory supplies request/response/result lifetime and transfer operations. EDP-Threading supplies the externally-owned `BoundedWaitWake` capability used by Runtime/Handle finite waits. EDP-Clock supplies canonical time vocabulary.

EDP-Threading itself consumes EDP-Platform synchronization providers; Command does not consume native Platform Signal mechanics. Platform-specific applications choose the concrete Signal provider used by the bounded Threading provider (for example FreeRTOS `SignalProvider`).

Explicit non-dependencies remain Transport, Serialisation and Security. Those domains integrate toward Command through adapters; Command never depends back toward them. Runtime service discovery is absent: application-owned provider instances are fixed through the immutable compile-time Architecture before initialization.
