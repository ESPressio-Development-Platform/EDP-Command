# Private Implementation

## Runtime Record

`Runtime::Record` is **PRIVATE IMPLEMENTATION**. `RequestStorage` and `ResponseStorage` are aligned raw storage owned by the slot. `RequestLive`/`ResponseLive` are authoritative lifetime flags and must exactly match whether a live object exists. `ResponseTaken` enforces at-most-once extraction. `Occupied` controls slot availability. `HandleRetained` deliberately prevents terminal reclamation while a Handle may still observe/extract. `CancellationRequested` is cooperative cancellation state. `Generation` changes on reuse and must never be zero after admission, preventing stale Handle aliasing. `State`, `TerminalOutcome`, and `Failure` hold observable lifecycle/result state.

## Runtime queue, counters and provider bindings

`_records` owns all invocation storage. `_queue` is a bounded ring of record indices; for a zero queue capacity the physical array still has one inert element to keep the C++ type well-formed. `_queueHead`, `_queueTail`, `_queueCount` are ring bookkeeping. `_executing` enforces the configured concurrency bound. `_state` owns Runtime lifecycle. `_handler` and `_waitProvider` are borrowed pointers whose pointees must outlive Runtime use.

`Matches` validates occupied slot plus generation. `ActiveCount` counts occupied records, including terminal records retained by Handles. `ReclaimIfPossible` is the central lifetime/reclamation gate: only terminal, unretained records may be destroyed/freed; it also completes `Quiescing` → `Quiescent` when the last occupied record disappears. `WakeTerminal` delegates terminal wake publication to the external EDP-Threading bounded wait/wake provider.

The private `Wait` helper validates the Handle identity before waiting, performs the finite provider wait, then revalidates identity and terminal state. This ordering enforces the locked rule that terminal state wins timeout/interruption races.

## Handle internals

`_runtime`, `_index`, `_generation` are private invocation capability state. Moving a Handle nulls the source runtime pointer. Move assignment first releases any currently retained invocation. Destruction delegates to `Release`, making retention RAII-bound.

## TakeResponseResult internals

`_storage` is aligned embedded raw storage; `_live` is the authoritative object-lifetime flag. Move construction uses EDP-Memory then destroys the source embedded object. Destructor destroys only when `_live`. `Take` transfers, destroys embedded state, and clears `_live`.

## ExecutionResult internals

For non-void Response, `_response` is an optional owning result payload. `_outcome` stores semantic terminal `Outcome`; `_failure` stores failure detail independently. `Failed()` currently maps to `ExecutorFailure`; integration failure is produced by outbound integration capability rather than Handler execution result.

## OutboundCompletion internals

`_context` and callback pointers are borrowed adapter state; `_used` is the authoritative exactly-once latch. It is set before invoking a callback, so even a callback returning a non-success `CompletionPublicationResult` consumes the completion attempt. This prevents retry from becoming duplicate terminal publication. Callback operational status is deliberately separate from the semantic `Outcome` being published.

## F4 scoped-dispatch internals

`LocalOnly`, `RemoteOnly`, and `LocalAndRemote` are empty policy Types and introduce no retained state. `DispatchScoped` is header-only coordination over callables already selected by the higher integration/routing layer. It stores no Request, destination, provider or route. The `LocalAndRemoteDispatchResult` object owns only the two returned domain-result objects and has no continuing relationship with either invocation lifecycle.

The concrete implementation currently invokes the local operation before the remote operation when constructing a combined result. That sequence is strictly an implementation detail: no application or integration may infer a cross-domain ordering contract from it. Both operations are required to be `noexcept` and non-`void`, enforced at compile time.
