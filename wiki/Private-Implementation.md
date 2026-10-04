# Private Implementation

## Runtime Record

`Runtime::Record` is **PRIVATE IMPLEMENTATION**. `RequestStorage` and `ResponseStorage` are aligned raw storage owned by the slot. `RequestLive`/`ResponseLive` are authoritative lifetime flags. `ResponseTaken` enforces at-most-once extraction. `Occupied`, `HandleRetained`, `QueuedInRing` and `TerminalWakePending` prevent premature reclamation. `CancellationRequested` is atomic cooperative cancellation state. `Generation` advances without wrapping; an exhausted generation slot is never reused. `State`, `TerminalOutcome`, and `Failure` hold observable lifecycle/result state.

## Runtime queue, counters and provider bindings

`_records` owns all invocation storage. `_queue` is a bounded ring of record indices; for zero queue capacity the physical array has one inert element. `_reservedQueueCount` protects ingress reservations from later queue admission. `_executing` enforces the configured concurrency bound. `_remoteHandoffs` is zero-capacity empty storage or a fixed array of caller-owned Request pointers plus generation/occupied state; it never contains payload copies. `_state` owns Runtime lifecycle. `_handler`, `_waitProvider` and `_mutex` are borrowed pointers whose pointees must outlive Runtime use.

`MatchesLocked` and `MatchesRemoteHandoffLocked` validate occupied slot plus generation under the application mutex. `ActiveCountLocked` includes invocation records and outbound stages. `ReclaimIfPossibleLocked` is the central lifetime gate: only terminal records with no Handle, queue tombstone or pending wake may be destroyed. Release helpers also complete `Quiescing` → `Quiescent` when the final active capability disappears. `WakeTerminal` delegates after unlocking.

The private `Wait` helper validates under the mutex, unlocks for the finite provider wait, then revalidates identity and terminal state. This ordering enforces the rule that terminal state wins timeout/interruption races without holding Command synchronization across a wait.

`ExecuteOne` is split into claim/execute/complete. Claim pops the ring entry and consumes one concurrency lease under the mutex. The Handler receives a stable Request plus atomic cancellation token after unlock. Completion reacquires the mutex, validates the same non-wrapping generation and publishes terminal state, then wakes after unlock.

## Handle internals

`_runtime`, `_index`, `_generation` are private invocation capability state. Moving a Handle nulls the source runtime pointer. Move assignment first releases any currently retained invocation. Destruction delegates to `Release`, making retention RAII-bound.

## TakeResponseResult internals

`_storage` is aligned embedded raw storage; `_live` is the authoritative object-lifetime flag. Move construction uses EDP-Memory then destroys the source embedded object. Destructor destroys only when `_live`. `Take` transfers, destroys embedded state, and clears `_live`.

## ExecutionResult internals

For non-void Response, `_response` is an optional owning result payload. `_outcome` stores semantic terminal `Outcome`; `_failure` stores failure detail independently. `Failed()` currently maps to `ExecutorFailure`; integration failure is produced by outbound integration capability rather than Handler execution result.

## OutboundCompletion internals

`_context` and callback pointers are borrowed adapter state; `_used` is the authoritative exactly-once latch. It is set before invoking a callback, so even a callback returning a non-success `CompletionPublicationResult` consumes the completion attempt. This prevents retry from becoming duplicate terminal publication. Callback operational status is deliberately separate from the semantic `Outcome` being published.

## Reservation and remote-operation internals

Inbound reservations own a constructed unpublished Request in an invocation record plus one reserved queue entitlement. Only commit sets `Queued`, creates Handle retention and inserts the ring index. Outbound handoff reservations own only a Runtime slot/generation and borrowed Request pointer; commit validates under the mutex, calls the adapter after unlock and retires the stage under the mutex. Both reservation kinds abort through RAII.

`RemoteCommandOperation` embeds its adapter binding by value, uses one validity bit for move/release ownership and delegates per-recipient semantic calls. The binding is responsible for bounded frozen recipient/correlation storage, generation/runtime validation and at-most-once Response extraction; Command supplies the stable operation vocabulary without copying Mesh records.

## F4 scoped-dispatch internals

`LocalOnly`, `RemoteOnly`, and `LocalAndRemote` are empty policy Types and introduce no retained state. `DispatchScoped` is header-only coordination over callables already selected by the higher integration/routing layer. It stores no Request, destination, provider or route. The `LocalAndRemoteDispatchResult` object owns only the two returned domain-result objects and has no continuing relationship with either invocation lifecycle.

The implementation explicitly evaluates the local operation before the remote operation. That sequence is a public admission/handoff contract, but it creates no promise about asynchronous Handler completion, remote arrival or recipient execution. Both operations are required to be `noexcept` and non-`void`, enforced at compile time.
