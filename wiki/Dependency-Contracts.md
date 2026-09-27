# Dependency Contracts

## EDP-Memory — mandatory production dependency

Command consumes `ObjectLifetime` to construct/move/destroy Request and Response objects in fixed raw storage and `OwnershipTransfer::Move` at explicit ownership-transfer boundaries. Command owns the storage; EDP-Memory owns neither objects nor buffers and performs no allocation for Command. This edge is required by `CommandTypes.hpp`, `Handle.hpp`, `Runtime.hpp` and transitively `Integration.hpp`.

## EDP-Clock — mandatory production vocabulary dependency

`CommandTypes.hpp` aliases EDP-Clock `Duration` and `MonotonicTimestamp`. Command does not select or own a clock provider in the implemented v1 Runtime; the dependency supplies canonical cross-domain time types without creating timing/scheduling ownership.

## EDP-System / EDP-Platform

These are present in package/build dependency metadata as part of the current coherent EDP dependency chain used by demos. The Command production headers do not acquire System/Platform provider ownership or use them to introduce Transport, Serialisation or Security.

## Deliberate non-dependencies

Transport, Serialisation and Security are intentionally absent. Inbound adapters retain remote correlation and use `InboundAdmission`; outbound integrations receive invocation-specific completion capability. This preserves dependency direction and keeps Command transport-agnostic.

## Test/demo dependencies

The PlatformIO demonstrations reference coherent sibling EDP repositories so workstream branches can be built together. Those paths are demonstration/build configuration, not additional Command runtime ownership contracts.