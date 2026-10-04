# EDP-Command Dependency Graph

> Revalidated: 2026-10-04 — Mesh V1 Command seams add no repository dependency and version remains `0.1.0`.

## Library Dependency Tree

```text
EDP-Command
  +-- EDP-System      [mandatory] Type/Field schema, Composition/Architecture and provider resolution
  +-- EDP-Primitives  [mandatory] Command Primitive-family contract and execution-domain scope vocabulary
  +-- EDP-Serialisation [mandatory] serialisable semantic Type qualification
  +-- EDP-Memory      [mandatory] typed object lifetime and ownership transfer
  +-- EDP-Threading   [mandatory] bounded finite wait/wake and ordinary-mutex capabilities
  `-- EDP-Clock       [mandatory] canonical Duration / MonotonicTimestamp vocabulary
```

## Mandatory Dependencies

EDP-System supplies universal `TypeIdentifier`, `FieldIdentifier`, `FieldBinding`, `FieldSet`, `SchemaType`, the Command Composition domain infrastructure and cross-domain Architecture used by Bootstrap.

EDP-Primitives supplies `PrimitiveFamilyIdentifier`, serialisable schema-bearing `PrimitiveType`, the family contract used by `Command::Family`, and canonical `LocalOnly`, `RemoteOnly`, `LocalAndRemote` execution-domain scope vocabulary re-exported by Command.

EDP-Serialisation supplies the canonical `SerialisableType` predicate used directly for Request and Response qualification; Command consumes no codec operation or wire profile.

EDP-Memory supplies request/response/result lifetime and transfer operations. EDP-Threading supplies externally-owned `BoundedWaitWake` and Command-identity `OrdinaryMutex` capabilities. EDP-Clock supplies canonical time vocabulary.

EDP-Threading itself consumes EDP-Platform synchronization providers; Command does not consume native Platform Signal/Mutex mechanics. Platform-specific applications choose concrete providers used by bounded wait/wake and ordinary-mutex wrappers, for example FreeRTOS `SignalProvider` and `MutexProvider`.

## Optional Dependencies

EDP-Command has no optional ESPressio-library dependencies.

## Explicit non-dependencies

Transport and Security integrate toward Command through adapters. EDP-Serialisation is now a direct compile-time semantic dependency only; runtime codec/buffer/parser responsibilities remain outside Command.

Runtime service discovery is absent: application-owned provider instances are fixed through immutable compile-time Architecture before initialization.
