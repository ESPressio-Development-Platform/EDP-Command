# Public API

All declarations below are **PUBLIC API** unless explicitly stated otherwise. Source traceability is consolidated in [Reference Index](Reference-Index).

## Semantic family and schema contract

`Command::Family` is the canonical Primitive family tag for Command operations. Its stable `PrimitiveFamilyIdentifier` is owned by ESPressio and it names `Command::Planner` as the family planner Type.

`CommandType<TCommand>` requires all of the following:

- `Primitives::PrimitiveType<TCommand>`;
- `TCommand::Family` is exactly `Command::Family`;
- nested `Request` and `Response` Types exist;
- both Request and Response satisfy `System::SchemaType` and `Serialisation::SerialisableType`.

Because `PrimitiveType` is now serialisable and schema-bearing, a valid Command operation directly declares its stable `System::TypeIdentifier` and canonical `Fields` Type. Operation Types normally use `System::FieldSet<>`.

Request and Response are independent semantic schema Types with identities distinct from the Command operation. Payload members use explicit stable numeric `System::FieldBinding`s.

`NoRequestPayload` and `NoResponsePayload` are separate identified zero-field Types satisfying both `System::SchemaType` and `Serialisation::SerialisableType`. They are the canonical no-payload contracts; `void` is not a valid semantic Request/Response Type for `CommandType`.

The Stage-C API directly consumes `EDP-Serialisation` for Request/Response `SerialisableType` qualification; no codec execution is owned here.

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

`LocalOnly`, `RemoteOnly`, and `LocalAndRemote` are Command-facing aliases of canonical empty compile-time EDP-Primitives execution-domain Dispatch policy Types. `ExecutionDomainScope<TScope>` accepts exactly those policy Types including cv/ref-qualified forms. Scope is Dispatch control metadata and not Request/schema/wire state.

`InvocationObservation` is a value snapshot. `State` always describes lifecycle. `HasOutcome` gates `TerminalOutcome`; `HasFailure` gates `Failure`. `DidSucceed()`, `WasRejected()`, `DidFail()` and `WasCancelled()` are genuine Boolean predicates over terminal outcome state.

`Request<TCommand>` / `Response<TCommand>` select nested schema Types. `Duration` and `MonotonicTimestamp` preserve EDP-Clock vocabulary.

## CancellationToken

Read-only cooperative cancellation view. Construction retains a pointer to Runtime-owned cancellation state; the referenced bool must outlive token use. `IsRequested()` reports the current request flag. It does not interrupt execution and is not an ISR synchronization primitive.

## ExecutionResult<TResponse>

Move-oriented Handler execution result. `Succeeded(response)` and `Rejected(response)` retain a Response; `Failed()` carries no Response and reports `ExecutorFailure`. `GetOutcome`, `Failure`, `HasResponse` observe result state; `TakeResponse` transfers the retained Response.

`ExecutionResult<void>` remains a response-less utility specialization, but `void` is not accepted by `CommandType`. Semantic no-response operations declare `Response = Command::NoResponsePayload`.

## ResourcePlan<I,Q,C>

Compile-time resource contract. `I` is total invocation slots, `Q` queue slots, `C` maximum simultaneous execution count. `I > 0`, `C > 0`, and `Q + C <= I`. Constants expose capacities; `InvocationBytes`, `QueueBytes`, `TotalCoreBytes` provide compile-time core-storage calculations for supplied record/index Types.

## Handle and response extraction

`Handle<TCommand,TRuntime>` is exclusive and move-only. Default construction creates an invalid Handle. Move transfers retention; destruction/`Release` releases the Runtime record. `IsValid` and outcome helpers are predicates; `State`, finite `WaitFor`/`WaitUntil`, `RequestCancellation`, and `TakeResponse` expose typed lifecycle operations while generation identity matches. There is deliberately no public InvocationId.

`TakeResponseResult<TResponse>` is move-only embedded ownership for extracted responses. `Status` reports extraction outcome, `HasValue` reports live embedded value, and `Take` transfers/destroys it. `Storage` and `MarkLive` support the Runtime/Handle extraction path and must preserve the live-storage invariant.

`DispatchResult<TCommand,TRuntime>` is move-only dispatch outcome. `Accepted` is a genuine predicate determining whether an invocation exists. `Failure` is meaningful for rejection. `TakeHandle` transfers the exclusive Handle.

## Runtime<TCommand,THandlerProvider,TWaitProvider,TPlan>

Fixed-storage Command engine. Constructor borrows the resolved Handler and bounded wait/wake provider for the Runtime lifetime. `Initialize` returns `InitializationResult`; `State` observes lifecycle; `BeginQuiesce` returns `QuiesceResult` and closes admission. `Dispatch` transactionally admits a Request or returns typed dispatch failure. `ExecuteOne` returns `ExecutionAttemptResult` after attempting to process at most one queued invocation synchronously. `Observe`, finite waits, `RequestCancellation`, `TakeResponse`, and `Release` are Handle-facing operations keyed by private slot/generation identity.

Runtime does not allocate, create threads, or claim concurrent-call thread safety. Handler `Execute(request,cancellation)` must be non-throwing in the architectural contract.

## Execution-domain scope API

`DispatchScoped(LocalOnly{}, localOperation)` invokes the selected non-throwing local operation exactly once and returns its non-void result directly. `DispatchScoped(RemoteOnly{}, remoteOperation)` invokes only the selected higher-layer remote operation and returns its non-void result directly; it creates no local Runtime admission.

`DispatchScoped(LocalAndRemote{}, localOperation, remoteOperation)` invokes both operations independently. It returns `LocalAndRemoteDispatchResult<TLocalResult,TRemoteResult>`, whose `Local()` and `Remote()` accessors expose outcomes independently. There is deliberately no aggregate success predicate, fallback, rollback, suppression, quorum or ordering semantic.

## Composition and Bootstrap

`Composition::Handler<TCommand>` is the exclusive Command-domain capability for one Command Type. `HandlerRequirement<TCommand>` requires exactly one same-domain provider. `WaitProviderRequirement` requires exactly one external-domain `EDP-Threading::BoundedWaitWake` provider.

`Bootstrap<TCommand,TArchitecture,TPlan>` resolves those providers from immutable EDP-System Architecture, verifies wait-provider capacity covers every invocation slot, owns Runtime wiring, and exposes typed initialization plus bound provider/Runtime access.

## Integration API

`InboundAdmission<TCommand,TRuntime>` borrows a Runtime and forwards typed Request ownership into local `Dispatch`; external correlation remains adapter-owned. `Dispatch(request)` and explicit `Dispatch(LocalOnly{}, request)` have the same local-only semantics. Remote scopes are intentionally not accepted by this local admission facade.

`OutboundCompletion<TResponse>` is semantically exactly-once. Callback aliases define non-throwing function-pointer contracts. `Succeeded`, `Rejected`, `Failed`, `Cancelled` consume the capability on first publication attempt and return `CompletionPublicationResult`; later attempts report `AlreadyCompleted`. A missing applicable callback reports `Unavailable`.

`OutboundInvocation<TCommand,TCompletion>` groups a borrowed `RequestView`, `CancellationToken`, and completion capability for an outbound adapter. The Request reference must remain valid for invocation use.
