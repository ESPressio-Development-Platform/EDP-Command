# Dependency Contracts

## EDP-System — mandatory schema and Composition dependency

EDP-System owns the universal semantic schema vocabulary consumed by Command:

- `TypeIdentifier`;
- `FieldIdentifier`;
- `FieldBinding`;
- `FieldSet`;
- `SchemaType`.

Every Command operation is schema-bearing through `Primitives::PrimitiveType`, and every Command Request/Response immediately satisfies `System::SchemaType` in the Stage-A prerequisite contract.

EDP-System also supplies the Composition Framework. `Composition.hpp` defines the Command domain and exclusive per-Command Handler capability; `Bootstrap.hpp` resolves exactly one same-domain Handler provider and exactly one external-domain bounded wait/wake provider from immutable application Architecture.

## EDP-Primitives — mandatory family and scope dependency

Command consumes two independent foundational responsibilities from EDP-Primitives:

1. Primitive-family/schema classification: `PrimitiveFamilyIdentifier`, `PrimitiveFamilyType` and schema-bearing `PrimitiveType` underpin `Command::Family` and `CommandType`.
2. execution-domain scope vocabulary: `Primitives::ExecutionDomain::LocalOnly`, `RemoteOnly`, `LocalAndRemote` and `Scope<TScope>` are re-exported in the Command namespace.

Command retains ownership of Handler/lifecycle/admission/response semantics and `DispatchScoped`; EDP-Primitives does not own Command runtime behaviour.

## EDP-Memory — mandatory production dependency

Command consumes `ObjectLifetime` to construct/move/destroy Request and Response objects in fixed raw storage and `OwnershipTransfer::Move` at explicit ownership-transfer boundaries. Command owns the storage; EDP-Memory owns neither objects nor buffers and performs no allocation for Command. This edge is required by `CommandTypes.hpp`, `Handle.hpp`, `Runtime.hpp` and transitively `Integration.hpp`.

## EDP-Clock — mandatory production vocabulary dependency

`CommandTypes.hpp` aliases EDP-Clock `Duration` and `MonotonicTimestamp`. Command does not select or own a clock provider in the implemented Runtime; the dependency supplies canonical cross-domain time Types without creating timing/scheduling ownership.

## EDP-Threading — mandatory external wait/wake dependency

Command consumes the EDP-Threading `BoundedWaitWake` capability and its strongly typed `BoundedWaitWakeResult`. Application owns the concrete provider; Command Bootstrap borrows it after Architecture resolution. Provider `Capacity` must be at least `ResourcePlan::InvocationCapacity`, ensuring every bounded invocation record has a corresponding wait/wake slot.

Command uses this provider for finite non-consuming `WaitFor`/`WaitUntil` and terminal wake publication; it does not create threads or claim Threading scheduler ownership.

## EDP-Platform — demo/concrete-provider dependency, not Command ownership

PlatformIO/Arduino demonstrations use EDP-Platform FreeRTOS synchronization as the concrete provider underlying EDP-Threading `BoundedWaitWakeProvider`. This is an application/demo Composition choice rather than a new Command-domain provider ownership contract.

## Deliberate non-dependencies

Transport, Serialisation and Security remain intentionally absent in Stage A. Inbound adapters retain remote correlation and use `InboundAdmission`; outbound integrations receive invocation-specific completion capability. `DispatchScoped` coordinates caller-selected operations without defining Transport/provider/destination Types or a routing registry.

`EDP-Serialisation` will become relevant only in the separately ordered Stage-C tightening after it exists: Primitives will universally require `SerialisableType`, and Command Request/Response will be constrained to that same canonical concept. No temporary or duplicate serialisability abstraction is introduced here.

## Test/demo dependencies

Branch validation uses the matching `EDP-Primitives/serialisation_prerequisites` branch while this cross-repository contract migration is in progress; `main` validation continues to use `EDP-Primitives/main`. Other sibling dependencies remain on their current main branches unless a tranche-specific dependency branch is explicitly required.
