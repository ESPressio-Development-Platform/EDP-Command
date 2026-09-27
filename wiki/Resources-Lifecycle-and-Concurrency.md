# Resources, Lifecycle and Concurrency

Command's core memory is compile-time bounded by `ResourcePlan`. Runtime embeds invocation records and queue indices; no invocation admission allocates. Requests and Responses live in aligned raw slot storage and their lifetime is mediated through EDP-Memory.

Admission is transactional: Runtime state and queue/slot capacity are checked before Request construction into a slot. Successful admission increments the slot generation, marks retention and queues the slot. Failed admission creates no invocation.

A Handle intentionally extends the lifetime of a terminal record. Reclamation occurs only after terminal state **and** Handle release. This permits stable observation and one-time response extraction without a global InvocationId. Response extraction transfers the object and immediately destroys Runtime's stored instance.

`ExecuteOne` is synchronous and processes at most one queue entry. `_executing` is checked against `ExecutionConcurrency`; v1 does not itself spawn workers. Cancellation is cooperative for executing work and immediate terminal marking for queued work. It is not forced thread cancellation.

Handle waiting is provided by an externally owned EDP-Threading `BoundedWaitWake` provider resolved through the immutable application Architecture. `WaitFor` and `WaitUntil` are finite and non-consuming. Runtime checks terminal state both before and after the provider wait; therefore an invocation that becomes terminal during the wait is reported as `WaitResult::Terminal` even if the provider simultaneously reports timeout or interruption.

The Runtime is not documented as safe for unsynchronized concurrent calls. Its booleans, ring indices and records are ordinary state, not atomics. A caller integrating multiple execution contexts must serialize access using the appropriate EDP-Threading architecture rather than assuming internal locking.

Initialization is `Uninitialized` → `Running`. `Initialize` reports `InitializationResult`. `BeginQuiesce` reports `QuiesceResult`, prevents new admission and chooses `Quiescent` immediately only when no occupied records remain; otherwise it enters `Quiescing`. Final reclamation transitions to `Quiescent` when active count reaches zero.

Semantic terminal result is represented by `Outcome`, independent from lifecycle state. A cancelled invocation has `InvocationState::Cancelled` and `Outcome::Cancelled`; successful, rejected and failed invocations are lifecycle `Completed` with the corresponding terminal Outcome.

Measured host `sizeof(Runtime)` fixtures on the first post-H16 regression gate on 2026-09-27 were: **112 bytes** for plan 2/1/1, **176 bytes** for 4/3/1, **360 bytes** for 10/8/2, and **664 bytes** for 20/16/4 with 4-byte Request/Response fixture types. These are validation measurements, not ABI guarantees for arbitrary Command/Handler/provider types.

## F4 execution-domain scope resources

F4 adds no `Runtime`, `ResourcePlan`, queue, invocation-record or Bootstrap storage. The scope policy Types are empty compile-time tags and `DispatchScoped` retains no state after the call. `LocalAndRemoteDispatchResult` is caller-owned return state containing only the independently produced local and remote result objects.

The final F4 regression gate preserved the existing measured `sizeof(Runtime)` fixtures exactly at **112 / 176 / 360 / 664 bytes**, confirming zero retained Runtime RAM increase for the scope feature. Local and remote operation concurrency/lifecycle remains owned by the corresponding domains; `DispatchScoped` does not add synchronization or establish a cross-domain execution-order guarantee.
