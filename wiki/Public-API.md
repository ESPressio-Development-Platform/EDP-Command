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

`InvocationState`: `Reserved` constructed but unpublished ingress backing; `Queued` awaiting execution; `Executing` claimed by an application execution context; `Completed` terminal completion; `Cancelled` terminal cancellation. A public Handle is created only at `Queued`, so it cannot observe `Reserved`.

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

Read-only cooperative cancellation view. Construction retains a pointer to Runtime-owned atomic cancellation state; the atomic must outlive token use. `IsRequested()` performs an acquire load so an executing Handler may safely observe a concurrent cancellation request. It does not interrupt execution and is not an ISR synchronization primitive.

## ExecutionResult<TResponse>

Move-oriented Handler execution result. `Succeeded(response)` and `Rejected(response)` retain a Response; `Failed()` carries no Response and reports `ExecutorFailure`. `GetOutcome`, `Failure`, `HasResponse` observe result state; `TakeResponse` transfers the retained Response.

`ExecutionResult<void>` remains a response-less utility specialization, but `void` is not accepted by `CommandType`. Semantic no-response operations declare `Response = Command::NoResponsePayload`.

## ResourcePlan<I,Q,C,R = 0>

Compile-time resource contract. `I` is total invocation slots, `Q` queue slots, `C` maximum simultaneous execution count and `R` simultaneous synchronized outbound handoff stages. `R` defaults to zero. `I > 0`, `C > 0`, and `Q + C <= I`. Constants expose capacities; `InvocationBytes`, `QueueBytes`, `RemoteHandoffBytes` and `TotalCoreBytes` provide compile-time storage calculations for supplied implementation Types.

## Handle and response extraction

`Handle<TCommand,TRuntime>` is exclusive and move-only. Default construction creates an invalid Handle. Move transfers retention; destruction/`Release` releases the Runtime record. `IsValid` and outcome helpers are predicates; `State`, finite `WaitFor`/`WaitUntil`, `RequestCancellation`, and `TakeResponse` expose typed lifecycle operations while generation identity matches. There is deliberately no public InvocationId.

`TakeResponseResult<TResponse>` is move-only embedded ownership for extracted responses. `Status` reports extraction outcome, `HasValue` reports live embedded value, and `Take` transfers/destroys it. `Storage` and `MarkLive` support the Runtime/Handle extraction path and must preserve the live-storage invariant.

`DispatchResult<TCommand,TRuntime>` is move-only dispatch outcome. `Accepted` is a genuine predicate determining whether an invocation exists. `Failure` is meaningful for rejection. `TakeHandle` transfers the exclusive Handle.

`InboundReservation<TCommand,TRuntime>` is a move-only owner of constructed but unpublished Request backing plus exact queue/invocation entitlement. `Value()` exposes the exclusive population destination, `Commit()` publishes and returns `DispatchResult`, and `Abort()`/destruction restores all resources. `InboundReservationResult` reports whether preparation succeeded and transfers the reservation.

`RemoteHandoffReservation<TCommand,TRuntime>` is a move-only owner of one outbound stage and immutable Request borrow. `Commit(remoteOperation)` performs exactly one bounded adapter call outside Command synchronization and retires the stage; `Abort()`/destruction invokes no adapter. `RemoteHandoffReservationResult` reports preparation failure or transfers the stage.

## Runtime<TCommand,THandlerProvider,TWaitProvider,TMutexProvider,TPlan>

Fixed-storage Command engine. Constructor borrows the resolved Handler, bounded wait/wake provider and Command-identity ordinary mutex for the Runtime lifetime. `Initialize` returns `InitializationResult`; `State` observes lifecycle; `BeginQuiesce` returns `QuiesceResult` and closes admission. `Dispatch` transactionally admits a Request. `PrepareIngress`/commit/abort own unpublished destination staging. `PrepareRemoteHandoff`/commit/abort own plan-bounded outbound borrow staging. `ExecuteOne` performs claim/execute/complete around at most one queued invocation. `Observe`, finite waits, `RequestCancellation`, `TakeResponse`, and `Release` are Handle-facing operations keyed by private slot/generation identity.

All short state transitions are serialized by the application mutex. Handler execution, waits, wake publication and remote adapter calls occur outside it. Runtime does not allocate or create threads; application-owned execution contexts may call `ExecuteOne` concurrently up to the plan limit. Handler `Execute(request,cancellation)` must be non-throwing.

## Execution-domain scope API

`DispatchScoped(LocalOnly{}, localOperation)` invokes the selected non-throwing local operation exactly once and returns its non-void result directly. `DispatchScoped(RemoteOnly{}, remoteOperation)` invokes only the selected higher-layer remote operation and returns its non-void result directly; it creates no local Runtime admission.

`DispatchScoped(LocalAndRemote{}, localOperation, remoteOperation)` invokes local admission first and then the independent remote handoff even after a negative local result. It returns `LocalAndRemoteDispatchResult<TLocalResult,TRemoteResult>`, whose non-throwing-movable `Local()` and `Remote()` results remain independent. There is deliberately no aggregate success predicate, fallback, rollback, suppression, quorum or Handler-completion ordering semantic.

Typed `RemoteOnly` and `LocalAndRemote` overloads consume a `RemoteHandoffReservation` so the Request borrow is valid only during one bounded adapter call outside the Command mutex.

## Composition and Bootstrap

`Composition::Handler<TCommand>` is the exclusive Command-domain capability for one Command Type. `HandlerRequirement<TCommand>` requires exactly one same-domain provider. `WaitProviderRequirement` requires exactly one external-domain `EDP-Threading::BoundedWaitWake` provider. `RuntimeMutexIdentity`, `RuntimeMutexRequirement` and `RuntimeMutexProvider` select exactly one external Command-identity ordinary mutex.

`Bootstrap<TCommand,TArchitecture,TPlan>` resolves those providers from immutable EDP-System Architecture, verifies wait-provider capacity covers every invocation slot, owns Runtime wiring, and exposes typed initialization plus bound Handler/wait/mutex/Runtime access.

## Integration API

`InboundAdmission<TCommand,TRuntime>` borrows a Runtime and forwards typed Request ownership into local `Dispatch`. `Prepare()` returns constructed but unpublished inbound backing when the Request is nothrow default constructible. `Dispatch(request)` and explicit `Dispatch(LocalOnly{}, request)` have the same local-only semantics. Remote scopes are intentionally not accepted by this local admission facade.

`OutboundHandoff<TCommand,TRuntime>` borrows a Runtime and prepares synchronized plan-bounded outbound stages for lvalue Requests; rvalues are rejected to prevent dangling borrows.

`CommandInvocationCorrelation<TDeliveryIdentifier>` is the strong Command semantic key containing authenticated source `DeviceIdentifier`, source `RuntimeIncarnationId` and the original Invocation delivery identifier. Later Cancellation and TerminalResult deliveries have independent delivery identities while referencing this value.

`RemoteCommandOperation<TCommand,TBinding>` is move-only and owns a bounded adapter binding by value. It exposes `RecipientCount`, `Recipient`, `Observe`, finite `WaitFor`/`WaitUntil`, `RequestCancellation`, at-most-once `TakeResponse`, and deterministic `Release`. Binding-defined result Types preserve Transport-neutral family code while the binding must retain exact frozen recipients/correlations and semantic proof.

`OutboundCompletion<TResponse>` is semantically exactly-once. Callback aliases define non-throwing function-pointer contracts. `Succeeded`, `Rejected`, `Failed`, `Cancelled` consume the capability on first publication attempt and return `CompletionPublicationResult`; later attempts report `AlreadyCompleted`. A missing applicable callback reports `Unavailable`.

`OutboundInvocation<TCommand,TCompletion>` groups a borrowed `RequestView`, `CancellationToken`, and completion capability for an outbound adapter. The Request reference must remain valid for invocation use.
