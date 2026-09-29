# Reference Index

This is the exhaustive production-source reference index for the current `src` surface. Detailed symbol semantics are in [Public API](Public-API), [Internal API](Internal-API) and [Private Implementation](Private-Implementation).

## `src/ESPressio_Command.hpp`

**PUBLIC ENTRY POINT.** Umbrella aggregation header. It declares no independent symbols; it exposes the seven maintained Command headers below as the supported include surface.

## `src/command/CommandTypes.hpp`

**PUBLIC API.** Defines/aliases `Duration`, `MonotonicTimestamp`; `InvocationState`, `Outcome`, `ExecutionFailure`, `DispatchFailure`, `WaitResult`, `CancellationRequestResult`, `TakeResponseStatus`, `CompletionPublicationResult`, `InitializationResult`, `ExecutionAttemptResult`, `QuiesceResult`, `RuntimeState` and their explicit values; F4 re-exported canonical Primitive scope Types `LocalOnly`, `RemoteOnly`, `LocalAndRemote` plus the `ExecutionDomainScope` concept alias; `InvocationObservation` and its lifecycle/outcome/failure fields plus terminal predicates; `Request<TCommand>`, `Response<TCommand>`, `CommandType`; `CancellationToken`; and `ExecutionResult<TResponse>` / `ExecutionResult<void>`. See [Public API](Public-API) and [Private Implementation](Private-Implementation).

## `src/command/Composition.hpp`

**PUBLIC COMPOSITION API.** Defines Command `Composition::Domain`; exclusive `Handler<TCommand>` capability; same-domain exactly-one `HandlerRequirement<TCommand>`; `HandlerProvider<TCommand,TComposition>` selection alias; external-domain exactly-one `WaitProviderRequirement` over EDP-Threading `BoundedWaitWake`; and `WaitProvider<TComposition>` selection alias.

## `src/command/ResourcePlan.hpp`

**PUBLIC API.** `ResourcePlan<TInvocationCapacity,TQueueCapacity,TExecutionConcurrency>` documents each template capacity through its named constants, enforces positive invocation/execution and `Queue + Execution <= Invocation`, and exposes `InvocationBytes<TRecord>`, `QueueBytes<TQueueIndex>`, `TotalCoreBytes<TRecord,TQueueIndex>`. The template parameters determine Runtime layout and capacity rather than dynamic configuration.

## `src/command/Handle.hpp`

**PUBLIC API with implementation-facing helpers.** Forward-declares `DispatchResult`; defines `TakeResponseResult<TResponse>` including status, embedded storage/live-state, deleted copy, move/destruction, `Status`, `HasValue`, `Storage`, `MarkLive`, `Take`; `Handle<TCommand,TRuntime>` including private runtime/index/generation identity, private successful-dispatch constructor, move-only RAII, `IsValid`, `State`, `WaitFor`, `WaitUntil`, `RequestCancellation`, constrained `TakeResponse`, `Release`; and `DispatchResult<TCommand,TRuntime>` including accepted/failure/Handle state, failure/success constructors, move-only outcome, `Accepted`, `Failure`, `TakeHandle`. Internal invariants are documented in [Internal API](Internal-API) and [Private Implementation](Private-Implementation).

## `src/command/Runtime.hpp`

**PUBLIC API plus PRIVATE IMPLEMENTATION.** Defines `Runtime<TCommand,THandlerProvider,TWaitProvider,TPlan>` constrained by `CommandType`. Public aliases: `Command`, `RequestType`, `ResponseType`, `Plan`. Public operations: constructor, deleted copy operations, `Initialize`, `State`, `BeginQuiesce`, `Dispatch`, `ExecuteOne`, `Observe`, `WaitFor`, `WaitUntil`, `RequestCancellation`, constrained `TakeResponse`, `Release`. Private `Record` and every field, `_records`, `_queue`, ring counters, `_executing`, `_state`, `_handler`, `_waitProvider`, `Matches`, `IsTerminal`, `WakeTerminal`, `ReclaimIfPossible`, `ActiveCount`, and the finite `Wait` helper are documented in [Private Implementation](Private-Implementation). Resource/lifecycle/concurrency consequences are documented in [Resources Lifecycle and Concurrency](Resources-Lifecycle-and-Concurrency).

## `src/command/Bootstrap.hpp`

**PUBLIC COMPOSITION/WIRING API.** Defines `Bootstrap<TCommand,TArchitecture,TPlan>`, resolves unique Handler and external bounded wait/wake providers, validates Architecture and wait capacity at compile time, owns stable Runtime wiring, exposes typed `Initialize`, and provides access to the Runtime and bound providers.

## `src/command/Integration.hpp`

**PUBLIC INTEGRATION API.** Defines `InboundAdmission<TCommand,TRuntime>` with borrowed `_runtime`, shorthand local `Dispatch` and explicit `LocalOnly` Dispatch; move-capable `LocalAndRemoteDispatchResult<TLocalResult,TRemoteResult>`; `DispatchScoped` overloads for `LocalOnly`, `RemoteOnly` and `LocalAndRemote`; `OutboundCompletion<TResponse>` callback aliases, borrowed callback/context state, `_used` exactly-once latch, constructor and four typed terminal-publication methods; `OutboundCompletion<void>` equivalent response-less contract; and `OutboundInvocation<TCommand,TCompletion>` with `RequestView`, `Cancellation`, `Completion`. See [Public API](Public-API) and [Dependency Contracts](Dependency-Contracts).

## Reference coverage validation

Production source units represented: **8 / 8** (umbrella plus seven Command headers). There are no production `.cpp` units and no maintained product tooling modules. Build/test/demo files are navigation subjects rather than production declaration reference surfaces.