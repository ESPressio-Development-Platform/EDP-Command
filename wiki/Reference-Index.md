# Reference Index

This is the exhaustive production-source reference index for the current `src` surface. Detailed symbol semantics are in [Public API](Public-API), [Internal API](Internal-API) and [Private Implementation](Private-Implementation).

## `src/ESPressio_Command.hpp`

**PUBLIC ENTRY POINT.** Umbrella aggregation header. It declares no independent symbols; it exposes the eight maintained Command headers below as the supported include surface.

## `src/command/CommandFamily.hpp`

**PUBLIC API.** Defines the canonical Command Primitive `Family` and its associated `Planner`. The family carries ESPressio's stable Primitive-family identity for Command and is required exactly by `CommandType`. See [CommandFamily reference](Reference-command-CommandFamily) and [Public API](Public-API).

## `src/command/CommandTypes.hpp`

**PUBLIC API.** Defines/aliases `Duration`, `MonotonicTimestamp`; `InvocationState`, `Outcome`, `ExecutionFailure`, `DispatchFailure`, `WaitResult`, `CancellationRequestResult`, `TakeResponseStatus`, `CompletionPublicationResult`, `InitializationResult`, `ExecutionAttemptResult`, `QuiesceResult`, `RuntimeState`; canonical schema-bearing `NoRequestPayload` and `NoResponsePayload`; re-exported Primitive scope Types `LocalOnly`, `RemoteOnly`, `LocalAndRemote` plus `ExecutionDomainScope`; `InvocationObservation`; `Request<TCommand>`, `Response<TCommand>`, `CommandType`; `CancellationToken`; and `ExecutionResult<TResponse>` / `ExecutionResult<void>`.

`CommandType` requires a schema-bearing `Primitives::PrimitiveType`, exact `Command::Family`, and schema-bearing Request/Response Types. See [Public API](Public-API) and [Private Implementation](Private-Implementation).

## `src/command/Composition.hpp`

**PUBLIC COMPOSITION API.** Defines Command `Composition::Domain`; exclusive `Handler<TCommand>` capability; same-domain exactly-one `HandlerRequirement<TCommand>`; `HandlerProvider<TCommand,TComposition>` selection alias; external-domain exactly-one `WaitProviderRequirement` over EDP-Threading `BoundedWaitWake`; and `WaitProvider<TComposition>` selection alias.

## `src/command/ResourcePlan.hpp`

**PUBLIC API.** `ResourcePlan<TInvocationCapacity,TQueueCapacity,TExecutionConcurrency>` documents each template capacity through its named constants, enforces positive invocation/execution and `Queue + Execution <= Invocation`, and exposes `InvocationBytes<TRecord>`, `QueueBytes<TQueueIndex>`, `TotalCoreBytes<TRecord,TQueueIndex>`. Template parameters determine Runtime layout/capacity rather than dynamic configuration.

## `src/command/Handle.hpp`

**PUBLIC API with implementation-facing helpers.** Forward-declares `DispatchResult`; defines `TakeResponseResult<TResponse>` including status, embedded storage/live-state, deleted copy, move/destruction, `Status`, `HasValue`, `Storage`, `MarkLive`, `Take`; `Handle<TCommand,TRuntime>` including private runtime/index/generation identity, private successful-dispatch constructor, move-only RAII, `IsValid`, `State`, `WaitFor`, `WaitUntil`, `RequestCancellation`, constrained `TakeResponse`, `Release`; and `DispatchResult<TCommand,TRuntime>` including accepted/failure/Handle state, failure/success constructors, move-only outcome, `Accepted`, `Failure`, `TakeHandle`.

## `src/command/Runtime.hpp`

**PUBLIC API plus PRIVATE IMPLEMENTATION.** Defines `Runtime<TCommand,THandlerProvider,TWaitProvider,TPlan>` constrained by `CommandType`. Public aliases: `Command`, `RequestType`, `ResponseType`, `Plan`. Public operations: constructor, deleted copy operations, `Initialize`, `State`, `BeginQuiesce`, `Dispatch`, `ExecuteOne`, `Observe`, `WaitFor`, `WaitUntil`, `RequestCancellation`, constrained `TakeResponse`, `Release`. Private implementation/resource details are documented in [Private Implementation](Private-Implementation) and [Resources Lifecycle and Concurrency](Resources-Lifecycle-and-Concurrency).

## `src/command/Bootstrap.hpp`

**PUBLIC COMPOSITION/WIRING API.** Defines `Bootstrap<TCommand,TArchitecture,TPlan>`, resolves unique Handler and external bounded wait/wake providers, validates Architecture and wait capacity at compile time, owns stable Runtime wiring, exposes typed `Initialize`, and provides access to Runtime and bound providers.

## `src/command/Integration.hpp`

**PUBLIC INTEGRATION API.** Defines `InboundAdmission<TCommand,TRuntime>`; move-capable `LocalAndRemoteDispatchResult<TLocalResult,TRemoteResult>`; `DispatchScoped` overloads for `LocalOnly`, `RemoteOnly` and `LocalAndRemote`; `OutboundCompletion<TResponse>` and `OutboundCompletion<void>` completion capabilities; and `OutboundInvocation<TCommand,TCompletion>`. See [Public API](Public-API) and [Dependency Contracts](Dependency-Contracts).

## Reference coverage validation

Production source units represented: **9 / 9** (umbrella plus eight Command headers). There are no production `.cpp` units and no maintained product tooling modules.
