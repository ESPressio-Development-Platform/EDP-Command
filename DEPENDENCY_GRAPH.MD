# EDP-Command Dependency Graph

> Revalidated: 2026-09-30 — restoring Command Primitive-family identity and System schema-bearing operation/Request/Response contracts adds no dependency edge. Version remains `0.1.0`.

## Library Dependency Tree

```text
EDP-Command
  +-- EDP-System      [mandatory] Type/Field schema, Composition/Architecture and provider resolution
  +-- EDP-Primitives  [mandatory] Command Primitive-family contract and execution-domain scope vocabulary
  +-- EDP-Memory      [mandatory] typed object lifetime and ownership transfer
  +-- EDP-Threading   [mandatory] bounded finite wait/wake capability
  `-- EDP-Clock       [mandatory] canonical Duration / MonotonicTimestamp vocabulary
```

## Mandatory Dependencies

EDP-System supplies universal `TypeIdentifier`, `FieldIdentifier`, `FieldBinding`, `FieldSet`, `SchemaType`, the Command Composition domain infrastructure and cross-domain Architecture used by Bootstrap.

EDP-Primitives supplies `PrimitiveFamilyIdentifier`, schema-bearing `PrimitiveType`, the family contract used by `Command::Family`, and canonical `LocalOnly`, `RemoteOnly`, `LocalAndRemote` execution-domain scope vocabulary re-exported by Command.

EDP-Memory supplies request/response/result lifetime and transfer operations. EDP-Threading supplies the externally-owned `BoundedWaitWake` capability used by Runtime/Handle finite waits. EDP-Clock supplies canonical time vocabulary.

EDP-Threading itself consumes EDP-Platform synchronization providers; Command does not consume native Platform Signal mechanics. Platform-specific applications choose the concrete Signal provider used by the bounded Threading provider, for example FreeRTOS `SignalProvider`.

## Optional Dependencies

EDP-Command has no optional ESPressio-library dependencies.

## Explicit non-dependencies

Transport, Serialisation and Security integrate toward Command through adapters; Command never depends back toward them in the Stage-A prerequisite contract. Serialisability becomes a universal semantic qualification only in the separately ordered Stage-C pass after EDP-Serialisation exists.

Runtime service discovery is absent: application-owned provider instances are fixed through immutable compile-time Architecture before initialization.
