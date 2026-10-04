# Reference Index

This is the exhaustive production-source reference index for the current `src` surface. Detailed symbol semantics are in [Public API](Public-API), [Internal API](Internal-API) and [Private Implementation](Private-Implementation).

## `src/ESPressio_Command.hpp`

**PUBLIC ENTRY POINT.** Umbrella aggregation header. It declares no independent symbols; it exposes the eight maintained Command headers below as the supported include surface.

## `src/command/CommandFamily.hpp`

**PUBLIC API.** Defines the canonical Command Primitive `Family` and its associated `Planner`. The family carries ESPressio's stable Primitive-family identity for Command and is required exactly by `CommandType`. See [CommandFamily reference](Reference-command-CommandFamily) and [Public API](Public-API).

## `src/command/CommandTypes.hpp`

**PUBLIC API.** Defines/aliases `Duration`, `MonotonicTimestamp`; `InvocationState`, `Outcome`, `ExecutionFailure`, `DispatchFailure`, `WaitResult`, `CancellationRequestResult`, `TakeResponseStatus`, `CompletionPublicationResult`, `InitializationResult`, `ExecutionAttemptResult`, `QuiesceResult`, `RuntimeState`; canonical schema-bearing `NoRequestPayload` and `NoResponsePayload`; re-exported Primitive scope Types `LocalOnly`, `RemoteOnly`, `LocalAndRemote` plus `ExecutionDomainScope`; `InvocationObservation`; `Request<TCommand>`, `Response<TCommand>`, `CommandType`; `CancellationToken`; and `ExecutionResult<TResponse>` / `ExecutionResult<void>`.

`CommandType` requires a serialisable schema-bearing `Primitives::PrimitiveType`, exact `Command::Family`, and Request/Response Types satisfying both `SchemaType` and `SerialisableType`. See [Public API](Public-API) and [Private Implementation](Private-Implementation).

## `src/command/Composition.hpp`

**PUBLIC COMPOSITION API.** Defines Command `Composition::Domain`; exclusive `Handler<TCommand>` capability; same-domain exactly-one `HandlerRequirement<TCommand>`; `HandlerProvider<TCommand,TComposition>` selection alias; external-domain exactly-one `WaitProviderRequirement`/selection over EDP-Threading `BoundedWaitWake`; and `RuntimeMutexIdentity`, external exactly-one `RuntimeMutexRequirement`, and `RuntimeMutexProvider<TComposition>` over EDP-Threading ordinary mutex.

## `src/command/ResourcePlan.hpp`

**PUBLIC API.** `ResourcePlan<TInvocationCapacity,TQueueCapacity,TExecutionConcurrency,TRemoteHandoffCapacity = 0>` documents each template capacity through named constants, enforces positive invocation/execution and `Queue + Execution <= Invocation`, and exposes invocation, queue, remote-handoff and core storage calculators. Template parameters determine Runtime layout/capacity rather than dynamic configuration.

## `src/command/Handle.hpp`

**PUBLIC API with implementation-facing helpers.** Defines `TakeResponseResult<TResponse>`; exclusive `Handle<TCommand,TRuntime>`; `DispatchResult<TCommand,TRuntime>`; constructed-but-unpublished `InboundReservation`/`InboundReservationResult`; and synchronized borrowed-Request `RemoteHandoffReservation`/`RemoteHandoffReservationResult`. All retained capabilities are move-only and use private Runtime slot/generation identity.

## `src/command/Runtime.hpp`

**PUBLIC API plus PRIVATE IMPLEMENTATION.** Defines `Runtime<TCommand,THandlerProvider,TWaitProvider,TMutexProvider,TPlan>` constrained by `CommandType`. Public operations cover lifecycle, dispatch, unpublished ingress reserve/commit/abort, synchronized outbound handoff reserve/commit/abort, claim/execute/complete, Handle observation/wait/cancellation/response/release. Private implementation/resource details are documented in [Private Implementation](Private-Implementation) and [Resources Lifecycle and Concurrency](Resources-Lifecycle-and-Concurrency).

## `src/command/Bootstrap.hpp`

**PUBLIC COMPOSITION/WIRING API.** Defines `Bootstrap<TCommand,TArchitecture,TPlan>`, resolves unique Handler plus external bounded wait/wake and Command ordinary-mutex providers, validates Architecture/wait capacity, owns stable Runtime wiring, exposes typed `Initialize`, and provides access to Runtime and bound providers.

## `src/command/Integration.hpp`

**PUBLIC INTEGRATION API.** Defines `InboundAdmission<TCommand,TRuntime>` and `OutboundHandoff<TCommand,TRuntime>`; move-capable `LocalAndRemoteDispatchResult`; generic and staged `DispatchScoped` overloads; strong `CommandInvocationCorrelation<TDeliveryIdentifier>`; move-only `RemoteCommandOperation<TCommand,TBinding>`; `OutboundCompletion<TResponse>`/`void`; and `OutboundInvocation<TCommand,TCompletion>`. See [Public API](Public-API) and [Dependency Contracts](Dependency-Contracts).

## Reference coverage validation

Production source units represented: **9 / 9** (umbrella plus eight Command headers). There are no production `.cpp` units and no maintained product tooling modules.
