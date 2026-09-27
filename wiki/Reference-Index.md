# Reference Index

This is the exhaustive production-source reference index for the current `src` surface. Detailed symbol semantics are in [Public API](Public-API), [Internal API](Internal-API) and [Private Implementation](Private-Implementation).

## `src/ESPressio_Command.hpp`

**PUBLIC ENTRY POINT.** Umbrella aggregation header. It declares no independent symbols; it exposes the five maintained Command headers below as the supported include surface.

## `src/command/CommandTypes.hpp`

**PUBLIC API.** Defines/aliases `Duration`, `MonotonicTimestamp`, `InvocationState` and every value, `CompletionStatus` and every value, `ExecutionFailure` and every value, `DispatchFailure` and every value, `WaitResult` and every value, `CancellationRequestResult` and every value, `TakeResponseStatus` and every value, `RuntimeState` and every value, `InvocationObservation` and all five state/result fields, `Request<TCommand>`, `Response<TCommand>`, `CommandType`, `CancellationToken` (constructor, `IsRequested`, `_requested` private state), `ExecutionResult<TResponse>` (factories, observers, `TakeResponse`, `_status/_failure/_response`) and `ExecutionResult<void>` (response-less factories/observers and `_status`). See [Public API](Public-API) and [Private Implementation](Private-Implementation).

## `src/command/ResourcePlan.hpp`

**PUBLIC API.** `ResourcePlan<TInvocationCapacity,TQueueCapacity,TExecutionConcurrency>` documents each template capacity through its named constants, enforces positive invocation/execution and `Queue+Execution<=Invocation`, and exposes `InvocationBytes<TRecord>`, `QueueBytes<TQueueIndex>`, `TotalCoreBytes<TRecord,TQueueIndex>`. The template parameters determine Runtime layout and capacity rather than dynamic configuration.

## `src/command/Handle.hpp`

**PUBLIC API with implementation-facing helpers.** Forward-declares `DispatchResult`; defines `TakeResponseResult<TResponse>` including status, embedded storage/live-state, deleted copy, move/destruction, `Status`, `HasValue`, `Storage`, `MarkLive`, `Take`; `Handle<TCommand,TRuntime>` including private runtime/index/generation identity, private successful-dispatch constructor, move-only RAII, `IsValid`, `State`, `RequestCancellation`, constrained `TakeResponse`, `Release`; and `DispatchResult<TCommand,TRuntime>` including accepted/failure/Handle state, failure/success constructors, move-only outcome, `Accepted`, `Failure`, `TakeHandle`. Internal invariants are documented in [Internal API](Internal-API) and [Private Implementation](Private-Implementation).

## `src/command/Runtime.hpp`

**PUBLIC API plus PRIVATE IMPLEMENTATION.** Defines `Runtime<TCommand,TExecutor,TPlan>` constrained by `CommandType`. Public aliases: `Command`, `RequestType`, `ResponseType`, `Plan`. Public operations: constructor, deleted copy operations, `Initialize`, `State`, `BeginQuiesce`, `Dispatch`, `ExecuteOne`, `Observe`, `RequestCancellation`, constrained `TakeResponse`, `Release`. Private `Record` and every field, `_records`, `_queue`, ring counters, `_executing`, `_state`, `_executor`, `Matches`, `ReclaimIfPossible`, `ActiveCount` are documented in [Private Implementation](Private-Implementation). Resource/lifecycle/concurrency consequences are documented in [Resources Lifecycle and Concurrency](Resources-Lifecycle-and-Concurrency).

## `src/command/Integration.hpp`

**PUBLIC INTEGRATION API.** Defines `InboundAdmission<TCommand,TRuntime>` with borrowed `_runtime`, constructor and `Dispatch`; `OutboundCompletion<TResponse>` callback aliases, borrowed callback/context state, `_used` exactly-once latch, constructor and four terminal methods; `OutboundCompletion<void>` equivalent response-less contract; and `OutboundInvocation<TCommand,TCompletion>` with `RequestView`, `Cancellation`, `Completion`. See [Public API](Public-API) and [Dependency Contracts](Dependency-Contracts).

## Reference coverage validation

Production source units represented: **6 / 6** (umbrella plus five command headers). There are no production `.cpp` units and no maintained product tooling modules. Build/test/demo files are navigation subjects rather than production declaration reference surfaces.