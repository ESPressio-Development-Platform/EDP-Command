# Command Architecture Contract

## Scope and ownership

EDP-Command owns bounded local Command admission, invocation lifecycle, execution, cancellation, response retention/extraction and narrow integration-facing admission/completion capabilities. It deliberately does not own Transport, Serialisation, Security, remote correlation, scheduling or provider discovery.

## Admission and lifecycle

A dispatch is transactional: failure creates no invocation; success atomically consumes one bounded invocation slot and returns the invocation's exclusive move-only `Handle`. Accepted invocations begin `Queued`, may become `Executing`, and terminate as `Completed` or `Cancelled`. `Completed` has a separate disposition: `Succeeded`, `Rejected` or `Failed`.

`DispatchFailure` distinguishes `NoCapacity`, `TypeUnrecognised`, `RuntimeUnavailable` and `BindingUnavailable`; the core typed Runtime directly produces the capacity/runtime cases while integration/binding layers may preserve the wider vocabulary.

## Static resources and determinism

`ResourcePlan<InvocationCapacity, QueueCapacity, ExecutionConcurrency>` fixes the core capacities at compile time. All capacities are positive where required and `QueueCapacity + ExecutionConcurrency <= InvocationCapacity`. Runtime storage is embedded in fixed `std::array` members. Request and Response objects are constructed, moved and destroyed in caller/runtime-owned raw storage through EDP-Memory; Command performs no heap allocation.

A retained Handle deliberately keeps a terminal record occupied until release/destruction so observation and response extraction remain valid. A response is extractable at most once.

## Execution and cancellation

The application-supplied Executor is retained by reference and is called synchronously by `ExecuteOne`. Executors are required to expose non-throwing execution compatible with the Command type and receive a read-only `CancellationToken`. Cancellation is cooperative while executing. A queued cancellation marks the invocation cancelled; execution observes that terminal state when the queued entry is encountered. Cancellation never implies asynchronous thread interruption.

The Runtime itself does not create threads and does not claim concurrent-call thread safety. Higher-level execution ownership must serialize Runtime mutation or provide the appropriate Threading integration.

## Initialization and shutdown

A Runtime starts `Uninitialized`; `Initialize` transitions once to `Running`. `BeginQuiesce` closes admission and transitions to `Quiescing` while retained/active records remain, then `Quiescent` after the last record can be reclaimed. No topology mutation or rebinding is introduced after initialization.

## Integration boundary

`InboundAdmission` is a typed facade over local Runtime dispatch; remote identity/correlation remains adapter-owned. `OutboundCompletion<TResponse>` is an invocation-specific, exactly-once completion capability. Its first success/rejection/failure/cancellation attempt consumes the capability; subsequent attempts fail. `OutboundInvocation` packages a request view, cancellation token and completion capability without introducing Transport ownership into Command.

## Ownership and lifetime

`Handle` is move-only. Its runtime pointer/index/generation tuple is private implementation identity, not a public InvocationId. Generation checking prevents a stale Handle from observing a reused slot. `TakeResponseResult` owns any extracted response in embedded storage and destroys it unless the caller takes it.

## Dependencies

EDP-Memory is mandatory for typed lifetime and ownership-transfer operations. EDP-Clock supplies the Command `Duration` and `MonotonicTimestamp` vocabulary. EDP-System and EDP-Platform are package/build dependencies inherited through the current EDP dependency chain; Command does not use them to acquire Transport, Security or Serialisation responsibilities.

## Validation

`tests/run_tests.py` covers host lifecycle contracts and deterministic Runtime-size measurements. `demos/CommandLifecycle` and `demos/FakeOutboundIntegration` are maintained in Arduino IDE, PlatformIO Arduino and PlatformIO ESP-IDF forms. The 2026-09-27 acceptance pass compiled both PlatformIO forms of both demos and measured representative bounded Runtime layouts.
