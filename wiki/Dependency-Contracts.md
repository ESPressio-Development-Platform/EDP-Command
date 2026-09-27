# Dependency Contracts

## EDP-Memory — mandatory production dependency

Command consumes `ObjectLifetime` to construct/move/destroy Request and Response objects in fixed raw storage and `OwnershipTransfer::Move` at explicit ownership-transfer boundaries. Command owns the storage; EDP-Memory owns neither objects nor buffers and performs no allocation for Command. This edge is required by `CommandTypes.hpp`, `Handle.hpp`, `Runtime.hpp` and transitively `Integration.hpp`.

## EDP-Clock — mandatory production vocabulary dependency

`CommandTypes.hpp` aliases EDP-Clock `Duration` and `MonotonicTimestamp`. Command does not select or own a clock provider in the implemented v1 Runtime; the dependency supplies canonical cross-domain time types without creating timing/scheduling ownership.

## EDP-System — mandatory Composition/Architecture dependency

`Composition.hpp` defines the Command domain and exclusive per-Command Handler capability using EDP-System Composition Framework contracts. `Bootstrap.hpp` resolves exactly one same-domain Handler provider and exactly one external-domain bounded wait/wake provider from the immutable application Architecture. Command does not mutate topology after initialization.

## EDP-Threading — mandatory external wait/wake dependency

Command consumes the EDP-Threading `BoundedWaitWake` capability and its strongly typed `BoundedWaitWakeResult`. The application owns the concrete provider; Command Bootstrap borrows it after Architecture resolution. Provider `Capacity` must be at least the Command `ResourcePlan::InvocationCapacity`, ensuring every bounded invocation record has a corresponding wait/wake slot. Command uses this provider for finite non-consuming `WaitFor`/`WaitUntil` and terminal wake publication; it does not create threads or claim Threading scheduler ownership.

EDP-Threading itself consumes EDP-Memory for its typed lifetime/ownership abstractions. Command must not bypass either abstraction with direct Standard Library ownership-transfer operations.

## EDP-Platform — demo/concrete-provider dependency, not Command ownership

PlatformIO/Arduino demonstrations use EDP-Platform FreeRTOS synchronization as the concrete provider underlying EDP-Threading `BoundedWaitWakeProvider`. This is an application/demo Composition choice rather than a new Command-domain provider ownership contract.

## Deliberate non-dependencies

Transport, Serialisation and Security are intentionally absent. Inbound adapters retain remote correlation and use `InboundAdmission`; outbound integrations receive invocation-specific completion capability. F4 execution-domain scope adds no dependency: `DispatchScoped` coordinates caller-selected operations without defining Transport/provider/destination Types or a routing registry. This preserves dependency direction and keeps Command transport-agnostic.

## Test/demo dependencies

The PlatformIO demonstrations reference coherent sibling EDP repositories so workstream branches can be built together. Those paths are demonstration/build configuration, not additional Command runtime ownership contracts.