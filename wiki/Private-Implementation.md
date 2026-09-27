# Private Implementation

## Runtime Record

`Runtime::Record` is **PRIVATE IMPLEMENTATION**. `RequestStorage` and `ResponseStorage` are aligned raw storage owned by the slot. `RequestLive`/`ResponseLive` are authoritative lifetime flags and must exactly match whether a live object exists. `ResponseTaken` enforces at-most-once extraction. `Occupied` controls slot availability. `HandleRetained` deliberately prevents terminal reclamation while a Handle may still observe/extract. `CancellationRequested` is cooperative cancellation state. `Generation` changes on reuse and must never be zero after admission, preventing stale Handle aliasing. `State`, `Completion`, `Failure` hold observable lifecycle/result state.

## Runtime queue and counters

`_records` owns all invocation storage. `_queue` is a bounded ring of record indices; for a zero queue capacity the physical array still has one inert element to keep the C++ type well-formed. `_queueHead`, `_queueTail`, `_queueCount` are ring bookkeeping. `_executing` enforces the configured concurrency bound. `_state` owns Runtime lifecycle. `_executor` is a borrowed pointer whose pointee must outlive Runtime use.

`Matches` validates occupied slot plus generation. `ActiveCount` counts occupied records, including terminal records retained by Handles. `ReclaimIfPossible` is the central lifetime/reclamation gate: only terminal, unretained records may be destroyed/freed; it also completes Quiescing→Quiescent when the last occupied record disappears.

## Handle internals

`_runtime`, `_index`, `_generation` are private invocation capability state. Moving a Handle nulls the source runtime pointer. Move assignment first releases any currently retained invocation. Destruction delegates to `Release`, making retention RAII-bound.

## TakeResponseResult internals

`_storage` is aligned embedded raw storage; `_live` is the authoritative object-lifetime flag. Move construction uses EDP-Memory then destroys the source embedded object. Destructor destroys only when `_live`. `Take` transfers, destroys embedded state, and clears `_live`.

## ExecutionResult internals

For non-void Response, `_response` is an optional owning result payload. `_status` and `_failure` preserve completion vocabulary. Failure currently maps to `ExecutorFailure`; integration failure is produced by outbound integration capability rather than Executor result.

## OutboundCompletion internals

`_context` and callback pointers are borrowed adapter state; `_used` is the authoritative exactly-once latch. It is set before invoking a callback, so even a callback returning false consumes the completion attempt. This prevents retry from becoming duplicate terminal publication.