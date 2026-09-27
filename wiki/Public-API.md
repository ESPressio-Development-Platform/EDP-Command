# Public API

All declarations below are **PUBLIC API** unless explicitly stated otherwise. Source traceability is consolidated in [Reference Index](Reference-Index).

## Vocabulary

`InvocationState`: `Queued` awaiting execution; `Executing` synchronously inside the Handler; `Completed` terminal completion; `Cancelled` terminal cancellation.

`Outcome`: `Succeeded`, `Rejected`, `Failed`, `Cancelled`. Outcome describes how a terminal invocation finished independently of its lifecycle state.

`ExecutionFailure`: `ExecutorFailure` local Handler/executor failure; `IntegrationFailure` integration completion failure.

`DispatchFailure`: `NoCapacity`, `TypeUnrecognised`, `RuntimeUnavailable`, `BindingUnavailable` respectively mean bounded capacity exhaustion, unsupported type, closed/uninitialized runtime, or unavailable external binding.

`WaitResult`: `Terminal`, `TimedOut`, `Interrupted`, `InvalidHandle`.

`CancellationRequestResult`: `Requested`, `AlreadyRequested`, `TooLate`, `InvalidHandle`.

`TakeResponseStatus`: `Taken`, `NotTerminal`, `NoResponse`, `AlreadyTaken`, `InvalidHandle`.

`CompletionPublicationResult`: `Accepted`, `AlreadyCompleted`, `Unavailable`. It reports the operational result of attempting outbound terminal publication and is deliberately separate from semantic `Outcome`.

`InitializationResult`: `Initialized`, `AlreadyInitialized`.

`ExecutionAttemptResult`: `Executed`, `NotExecuted`.

`QuiesceResult`: `Started`, `NotRunning`.

`RuntimeState`: `Uninitialized`, `Running`, `Quiescing`, `Quiescent`.

`InvocationObservation` is a value snapshot. `State` always describes lifecycle. `HasOutcome` gates `TerminalOutcome`; `HasFailure` gates `Failure`. `DidSucceed()`, `WasRejected()`, `DidFail()` and `WasCancelled()` are genuine Boolean predicates over terminal outcome state. Default outcome/failure values are placeholders unless their corresponding presence flag is true.

`Request<TCommand>` / `Response<TCommand>` select nested request/response types. `CommandType` requires both nested types. `Duration` and `MonotonicTimestamp` preserve EDP-Clock vocabulary.

## CancellationToken

Read-only cooperative cancellation view. Construction retains a pointer to Runtime-owned cancellation state; the referenced bool must outlive token use. `IsRequested()` reports the current request flag. It does not interrupt execution and is not an ISR synchronization primitive.

## ExecutionResult<TResponse>

Move-oriented Handler execution result. `Succeeded(response)` and `Rejected(response)` retain a Response; `Failed()` carries no Response and reports `ExecutorFailure`. `GetOutcome`, `Failure`, `HasResponse` observe result state; `TakeResponse` transfers the retained Response. `ExecutionResult<void>` supplies corresponding response-less factories/accessors.

## ResourcePlan<I,Q,C>

Compile-time resource contract. `I` is total invocation slots, `Q` queue slots, `C` maximum simultaneous execution count. `I > 0`, `C > 0`, and `Q + C <= I`. Constants expose capacities; `InvocationBytes`, `QueueBytes`, `TotalCoreBytes` provide compile-time core-storage calculations for supplied record/index types.

## Handle and response extraction

`Handle<TCommand,TRuntime>` is exclusive and move-only. Default construction creates an invalid Handle. Move transfers retention; destruction/`Release` releases the Runtime record. `IsValid` and the outcome helpers are predicates; `State`, finite `WaitFor`/`WaitUntil`, `RequestCancellation`, and non-void `TakeResponse` expose typed lifecycle operations while generation identity matches. There is deliberately no public InvocationId.

`TakeResponseResult<TResponse>` is move-only embedded ownership for extracted responses. `Status` reports extraction outcome, `HasValue` reports live embedded value, and `Take` transfers/destroys it. `Storage` and `MarkLive` support the Runtime/Handle extraction path and must preserve the live-storage invariant.

`DispatchResult<TCommand,TRuntime>` is move-only dispatch outcome. `Accepted` is a genuine predicate determining whether an invocation exists. `Failure` is meaningful for rejection. `TakeHandle` transfers the exclusive Handle.

## Runtime<TCommand,THandlerProvider,TWaitProvider,TPlan>

Fixed-storage Command engine. Constructor borrows the resolved Handler and bounded wait/wake provider for the Runtime lifetime. `Initialize` returns `InitializationResult`; `State` observes lifecycle; `BeginQuiesce` returns `QuiesceResult` and closes admission. `Dispatch` transactionally admits a Request or returns typed dispatch failure. `ExecuteOne` returns `ExecutionAttemptResult` after attempting to process at most one queued invocation synchronously. `Observe`, finite waits, `RequestCancellation`, `TakeResponse`, and `Release` are the Handle-facing operations keyed by private slot/generation identity.

Runtime does not allocate, create threads, or claim concurrent-call thread safety. Handler `Execute(request,cancellation)` must be non-throwing in the architectural contract.

## Composition and Bootstrap

`Composition::Handler<TCommand>` is the exclusive Command-domain capability for one Command type. `HandlerRequirement<TCommand>` requires exactly one same-domain provider. `WaitProviderRequirement` requires exactly one external-domain `EDP-Threading::BoundedWaitWake` provider.

`Bootstrap<TCommand,TArchitecture,TPlan>` resolves those providers from the immutable EDP-System Architecture, verifies wait-provider capacity covers every invocation slot, owns the Runtime wiring, and exposes typed initialization plus bound provider/Runtime access.

## Integration API

`InboundAdmission<TCommand,TRuntime>` borrows a Runtime and forwards typed Request ownership into `Dispatch`; external correlation remains adapter-owned.

`OutboundCompletion<TResponse>` is semantically exactly-once. Callback aliases define non-throwing function-pointer contracts. `Succeeded`, `Rejected`, `Failed`, `Cancelled` consume the capability on the first publication attempt and return `CompletionPublicationResult`; later attempts report `AlreadyCompleted`. A missing applicable callback reports `Unavailable`. The `void` specialization has the same contract without Response payload.

`OutboundInvocation<TCommand,TCompletion>` groups a borrowed `RequestView`, `CancellationToken`, and completion capability for an outbound adapter. The Request reference must remain valid for invocation use.