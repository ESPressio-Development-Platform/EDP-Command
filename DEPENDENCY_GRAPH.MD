# EDP-Command Dependency Graph

> Revalidated: 2026-09-27 — H16 terminal Outcome, typed operational results, consumer/demo reconciliation and documentation changes add no dependency edge.

## Library Dependency Tree

```text
EDP-Command
  +-- EDP-System     [mandatory] compile-time Composition/Architecture and provider resolution
  +-- EDP-Memory     [mandatory] typed object lifetime and ownership transfer
  +-- EDP-Threading  [mandatory] bounded finite wait/wake capability
  `-- EDP-Clock      [mandatory] canonical Duration / MonotonicTimestamp vocabulary
```

## Mandatory Dependencies

EDP-System supplies the Command Composition domain, exclusive `Handler<TCommand>` resolution and cross-domain Architecture used by Bootstrap. EDP-Memory supplies request/response/result lifetime and transfer operations. EDP-Threading supplies the externally-owned `BoundedWaitWake` capability used by Runtime/Handle finite waits. EDP-Clock supplies canonical time vocabulary.

EDP-Threading itself consumes EDP-Platform synchronization providers; Command does not consume native Platform Signal mechanics. Platform-specific applications choose the concrete Signal provider used by the bounded Threading provider (for example FreeRTOS `SignalProvider`).

## Optional Dependencies

EDP-Command has no optional ESPressio-library dependencies.

## Explicit non-dependencies

Transport, Serialisation and Security integrate toward Command through adapters; Command never depends back toward them. Runtime service discovery is absent: application-owned provider instances are fixed through the immutable compile-time Architecture before initialization.
