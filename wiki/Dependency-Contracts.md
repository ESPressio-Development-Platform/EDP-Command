# Dependency Contracts

## EDP-System — mandatory schema and Composition dependency

EDP-System owns the universal semantic schema vocabulary consumed by Command:

- `TypeIdentifier`;
- `FieldIdentifier`;
- `FieldBinding`;
- `FieldSet`;
- `SchemaType`.

Every Command operation is serialisable/schema-bearing through `Primitives::PrimitiveType`; every Request/Response satisfies both `System::SchemaType` and `Serialisation::SerialisableType`.

EDP-System also supplies Device/runtime identity and the Composition Framework. `Composition.hpp` defines the Command domain and exclusive per-Command Handler capability; `Bootstrap.hpp` resolves exactly one same-domain Handler plus exactly one external bounded wait/wake and one external Command-identity ordinary mutex provider from immutable application Architecture.

## EDP-Primitives — mandatory family and scope dependency

Command consumes two independent foundational responsibilities from EDP-Primitives:

1. Primitive-family/schema classification: `PrimitiveFamilyIdentifier`, `PrimitiveFamilyType` and serialisable schema-bearing `PrimitiveType` underpin `Command::Family` and `CommandType`.
2. execution-domain scope vocabulary: `Primitives::ExecutionDomain::LocalOnly`, `RemoteOnly`, `LocalAndRemote` and `Scope<TScope>` are re-exported in the Command namespace.

Command retains ownership of Handler/lifecycle/admission/response semantics and `DispatchScoped`; EDP-Primitives does not own Command runtime behaviour.

## EDP-Memory — mandatory production dependency

Command consumes `ObjectLifetime` to construct/move/destroy Request and Response objects in fixed raw storage and `OwnershipTransfer::Move` at explicit ownership-transfer boundaries. Command owns the storage; EDP-Memory owns neither objects nor buffers and performs no allocation for Command. This edge is required by `CommandTypes.hpp`, `Handle.hpp`, `Runtime.hpp` and transitively `Integration.hpp`.

## EDP-Clock — mandatory production vocabulary dependency

`CommandTypes.hpp` aliases EDP-Clock `Duration` and `MonotonicTimestamp`. Command does not select or own a clock provider in the implemented Runtime; the dependency supplies canonical cross-domain time Types without creating timing/scheduling ownership.

## EDP-Threading — mandatory external synchronization dependency

Command consumes EDP-Threading `BoundedWaitWake` and `OrdinaryMutex<Composition::RuntimeMutexIdentity>`. Application owns both concrete providers; Command Bootstrap borrows them after Architecture resolution. Wait provider `Capacity` must be at least `ResourcePlan::InvocationCapacity`, ensuring every bounded invocation record has a corresponding wait/wake slot.

Command uses the mutex only for short state transitions and the wait provider for finite non-consuming waits/terminal wake publication. Handler execution, waits, wakes and remote adapter calls occur outside the mutex. Command does not create threads or claim Threading scheduler ownership.

## EDP-Platform — demo/concrete-provider dependency, not Command ownership

PlatformIO/Arduino demonstrations use EDP-Platform FreeRTOS synchronization as the concrete provider underlying EDP-Threading `BoundedWaitWakeProvider` and `OrdinaryMutexProvider`. This is an application/demo Composition choice rather than Command owning native Platform synchronization.

## EDP-Serialisation — mandatory semantic qualification dependency

Command directly consumes the public `SerialisableType` predicate for Request/Response qualification. Primitive operation serialisability is inherited through EDP-Primitives. Command does not consume codec operations, wire profiles, parser state, Localisation, or caller buffers.

## Deliberate non-dependencies

Transport and Security remain intentionally absent. Command owns the strong semantic `CommandInvocationCorrelation`, but adapters construct it and retain delivery/wire correlation plus routing state. `DispatchScoped`, reservations and `RemoteCommandOperation` expose family semantics without defining Transport/provider/destination Types or a routing registry.

## Test/demo dependencies

Stage-C validation consumes `EDP-Primitives/main` after the universal Primitive serialisability checkpoint. Other sibling dependencies remain on their current main branches unless a tranche-specific dependency branch is explicitly required.
