# Public API

All declarations below are **PUBLIC API** unless explicitly stated otherwise. Source traceability is consolidated in [Reference Index](Reference-Index).

## Vocabulary

`InvocationState`: `Queued` awaiting execution; `Executing` synchronously inside Executor; `Completed` terminal with a completion disposition; `Cancelled` terminal cancellation.

`CompletionStatus`: `Succeeded` accepted successful result; `Rejected` application-level rejection (may carry Response); `Failed` execution/integration failure.

`ExecutionFailure`: `ExecutorFailure` local executor failure; `IntegrationFailure` integration completion failure.

`DispatchFailure`: `NoCapacity`, `TypeUnrecognised`, `RuntimeUnavailable`, `BindingUnavailable` respectively mean bounded capacity exhaustion, unsupported type, closed/uninitialized runtime, or unavailable external binding.

`WaitResult`: `Finished`, `TimedOut`, `InvalidHandle`, `ProviderFailure`; reserved public wait vocabulary. The current core Runtime does not expose a wait method.

`CancellationRequestResult`: `Accepted`, `AlreadyRequested`, `AlreadyTerminal`, `InvalidHandle`.

`TakeResponseStatus`: `Taken`, `NotTerminal`, `NoResponse`, `AlreadyTaken`, `InvalidHandle`.

`RuntimeState`: `Uninitialized`, `Running`, `Quiescing`, `Quiescent`.

`InvocationObservation` is a value snapshot: `State`; `HasCompletion` gates `Completion`; `HasFailure` gates `Failure`. Default values are placeholders unless their corresponding presence flag is true.

`Request<TCommand>` / `Response<TCommand>` select nested request/response types. `CommandType` requires both nested types. `Duration` and `MonotonicTimestamp` preserve EDP-Clock vocabulary.

## CancellationToken

Read-only cooperative cancellation view. Construction retains a pointer to Runtime-owned cancellation state; the referenced bool must outlive token use. `IsRequested()` reports the current request flag. It does not interrupt execution and is not an ISR synchronization primitive.

## ExecutionResult<TResponse>

Move-oriented executor result. `Succeeded(response)` and `Rejected(response)` retain a Response; `Failed()` carries no Response and reports `ExecutorFailure`. `Status`, `Failure`, `HasResponse` observe result state; `TakeResponse` transfers the retained Response. `ExecutionResult<void>` supplies corresponding response-less factories/accessors.

## ResourcePlan<I,Q,C>

Compile-time resource contract. `I` is total invocation slots, `Q` queue slots, `C` maximum simultaneous execution count. `I>0`, `C>0`, and `Q+C<=I`. Constants expose capacities; `InvocationBytes`, `QueueBytes`, `TotalCoreBytes` provide compile-time core-storage calculations for supplied record/index types.

## Handle and response extraction

`Handle<TCommand,TRuntime>` is exclusive and move-only. Default construction creates an invalid Handle. Move transfers retention; destruction/`Release` releases the Runtime record. `IsValid`, `State`, `RequestCancellation` and non-void `TakeResponse` operate only while generation identity matches. There is deliberately no public InvocationId.

`TakeResponseResult<TResponse>` is move-only embedded ownership for extracted responses. `Status` reports extraction outcome, `HasValue` reports live embedded value, and `Take` transfers/destroys it. `Storage` and `MarkLive` support the Runtime/Handle extraction path and must preserve the live-storage invariant.

`DispatchResult<TCommand,TRuntime>` is move-only dispatch outcome. `Accepted` determines whether an invocation exists. `Failure` is meaningful for rejection. `TakeHandle` transfers the exclusive Handle.

## Runtime<TCommand,TExecutor,TPlan>

Fixed-storage Command engine. Constructor borrows the Executor for the Runtime lifetime. `Initialize` transitions once to Running; `State` observes lifecycle; `BeginQuiesce` closes admission. `Dispatch` transactionally admits a Request or returns failure. `ExecuteOne` processes at most one queued invocation synchronously. `Observe`, `RequestCancellation`, `TakeResponse`, and `Release` are the Handle-facing operations keyed by private slot/generation identity.

Runtime does not allocate, create threads, or claim concurrent-call thread safety. Executor `Execute(request,cancellation)` must be non-throwing in the architectural contract.

## Integration API

`InboundAdmission<TCommand,TRuntime>` borrows a Runtime and forwards typed Request ownership into `Dispatch`; external correlation remains adapter-owned.

`OutboundCompletion<TResponse>` is move/copy-restricted by its state and semantically exactly-once. Callback aliases define non-throwing function-pointer contracts. `Succeeded`, `Rejected`, `Failed`, `Cancelled` consume the capability on first attempt; later attempts return false. The `void` specialization has the same contract without Response payload.

`OutboundInvocation<TCommand,TCompletion>` groups a borrowed `RequestView`, `CancellationToken`, and completion capability for an outbound adapter. The Request reference must remain valid for invocation use.