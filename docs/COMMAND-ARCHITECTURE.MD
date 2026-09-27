# Command Architecture Contract

## Scope and ownership

EDP-Command owns bounded local Command admission, invocation lifecycle, execution, cancellation, finite local waiting, response retention/extraction and narrow integration-facing admission/completion capabilities. It deliberately does not own Transport, Serialisation, Security, remote correlation, scheduling or provider discovery.

## Composition and Bootstrap

Each Command type has an exclusive `Composition::Handler<TCommand>` capability. A valid application architecture supplies exactly one Handler provider in the Command domain. Command also requires exactly one external `EDP-Threading::BoundedWaitWake` provider. `Bootstrap<TCommand, TArchitecture, TPlan>` resolves these providers statically, borrows the application-owned instances, and constructs the Runtime wiring without runtime discovery or topology mutation.

The wait provider's compile-time `Capacity` must be at least `ResourcePlan::InvocationCapacity`. Once initialized, the application topology is immutable.

## Admission, lifecycle and terminal outcome

A dispatch is transactional: failure creates no invocation; success atomically consumes one bounded invocation slot and returns the invocation's exclusive move-only `Handle`. Accepted invocations begin `Queued`, may become `Executing`, and terminate as `Completed` or `Cancelled`.

Lifecycle state and terminal semantic outcome are deliberately separate. Every terminal invocation has an `Outcome`: `Succeeded`, `Rejected`, `Failed`, or `Cancelled`. Cancellation therefore remains `InvocationState::Cancelled` while also exposing `Outcome::Cancelled`. `InvocationObservation` provides Boolean state predicates `DidSucceed()`, `WasRejected()`, `DidFail()`, and `WasCancelled()`. `ExecutionFailure` is orthogonal failure-cause detail when the outcome is `Failed`.

`DispatchFailure` distinguishes `NoCapacity`, `TypeUnrecognised`, `RuntimeUnavailable` and `BindingUnavailable`; the core typed Runtime directly produces the capacity/runtime cases while integration/binding layers may preserve the wider vocabulary.

## Static resources and determinism

`ResourcePlan<InvocationCapacity, QueueCapacity, ExecutionConcurrency>` fixes the core capacities at compile time. Runtime storage is embedded in fixed arrays. Request and Response objects are constructed, moved and destroyed in caller/runtime-owned raw storage through EDP-Memory; Command performs no heap allocation.

Each invocation slot maps deterministically to the same indexed slot in the external bounded wait/wake provider. The synchronization objects remain owned by EDP-Threading/application composition rather than Command. Post-reconciliation Runtime measurement adds exactly one borrowed provider pointer: 112, 176, 360 and 664 bytes for the accepted 2/1/1, 4/3/1, 10/8/2 and 20/16/4 plans respectively, an invariant +8 bytes over the historical Runtime sizes on the measured host. These measurements predate the H16 terminal-Outcome representation and must be revalidated by the final regression gate.

A retained Handle deliberately keeps a terminal record occupied until release/destruction so observation, waiting and response extraction remain valid. A response is extractable at most once.

## Finite waiting

`Handle::WaitFor(Duration)` and `Handle::WaitUntil(MonotonicTimestamp)` are finite, non-consuming observation operations. They never create waiter collections inside Command. If the invocation is already terminal they return `WaitResult::Terminal` immediately. Otherwise Runtime delegates to the invocation's stable EDP-Threading wait slot and then re-observes authoritative invocation state before interpreting the provider result. Consequently a terminal transition wins a simultaneous timeout/interruption race.

The public results are `Terminal`, `TimedOut`, `Interrupted`, and `InvalidHandle`. Repeated waits on the same retained terminal Handle remain valid. Same-Handle operations are caller-serialized.

## Execution and cancellation

The application-supplied Handler provider is retained by reference and is called synchronously by `ExecuteOne`. Handlers receive the typed Request and a read-only `CancellationToken`. Cancellation is cooperative while executing and immediate for queued work. Terminal transitions wake the corresponding bounded Threading wait slot.

`RequestCancellation` returns `Requested`, `AlreadyRequested`, `TooLate`, or `InvalidHandle`. Cancellation never implies asynchronous thread interruption.

The Runtime itself does not create threads. Execution scheduling/ownership belongs outside Command; EDP-Threading supplies the bounded wait/wake primitive rather than Command implementing platform synchronization itself.

## Typed operational results

Fallible operations use dedicated strongly typed results rather than Boolean success/failure. `Initialize` returns `InitializationResult`; `ExecuteOne` returns `ExecutionAttemptResult`; `BeginQuiesce` returns `QuiesceResult`; cancellation, waiting, response extraction and completion publication likewise have dedicated result vocabularies. Boolean values remain appropriate for genuine predicates such as `DispatchResult::Accepted()`, `TakeResponseResult::HasValue()`, `CancellationToken::IsRequested()` and the terminal-outcome predicates.

## Initialization and shutdown

A Runtime starts `Uninitialized`; `Initialize` transitions once to `Running`. `BeginQuiesce` closes admission and transitions to `Quiescing` while retained/active records remain, then `Quiescent` after the last record can be reclaimed. No topology mutation or rebinding is introduced after initialization.

## Integration boundary

`InboundAdmission` is a typed facade over local Runtime dispatch; remote identity/correlation remains adapter-owned. `OutboundCompletion<TResponse>` is an invocation-specific, exactly-once completion capability. Its first success/rejection/failure/cancellation publication consumes the capability. Publication returns `CompletionPublicationResult`: `Accepted`, `AlreadyCompleted`, or `Unavailable`. This operational result is distinct from the invocation's semantic terminal `Outcome`. `OutboundInvocation` packages a request view, cancellation token and completion capability without introducing Transport ownership into Command.

## Ownership and lifetime

`Handle` is move-only. Its runtime pointer/index/generation tuple is private implementation identity, not a public InvocationId. Generation checking prevents a stale Handle from observing a reused slot. `TakeResponseResult` owns any extracted response in embedded storage and destroys it unless the caller takes it.

## Dependencies

EDP-Memory is mandatory for typed lifetime and ownership-transfer operations. EDP-Clock supplies `Duration` and `MonotonicTimestamp`. EDP-System supplies the compile-time Composition/Architecture framework used by Bootstrap. EDP-Threading supplies the external bounded finite wait/wake capability. EDP-Platform and EDP-BoundedTopology are inherited through the Threading dependency chain. Command does not acquire Transport, Security or Serialisation responsibilities.

## Validation

`tests/run_tests.py` covers host lifecycle/wait/cancellation contracts, compile-time rejection of missing and duplicate Command Handlers, and deterministic Runtime-size measurements. `demos/CommandLifecycle` and `demos/FakeOutboundIntegration` are maintained in Arduino IDE, PlatformIO Arduino and PlatformIO ESP-IDF forms. `examples/HelloCommand` provides the consumer-oriented Bootstrap example described by README.
