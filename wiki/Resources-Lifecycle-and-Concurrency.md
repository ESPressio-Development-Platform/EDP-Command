# Resources, Lifecycle and Concurrency

Command's core memory is compile-time bounded by `ResourcePlan`. Runtime embeds invocation records and queue indices; no invocation admission allocates. Requests and Responses live in aligned raw slot storage and their lifetime is mediated through EDP-Memory.

Admission is transactional: Runtime state and queue/slot capacity are checked before Request construction into a slot. Successful admission increments the slot generation, marks retention and queues the slot. Failed admission creates no invocation.

A Handle intentionally extends the lifetime of a terminal record. Reclamation occurs only after terminal state **and** Handle release. This permits stable observation and one-time response extraction without a global InvocationId. Response extraction transfers the object and immediately destroys Runtime's stored instance.

`ExecuteOne` is synchronous and processes at most one queue entry. `_executing` is checked against `ExecutionConcurrency`; v1 does not itself spawn workers. Cancellation is cooperative for executing work and immediate terminal marking for queued work. It is not forced thread cancellation.

The Runtime is not documented as safe for unsynchronized concurrent calls. Its booleans, ring indices and records are ordinary state, not atomics. A caller integrating multiple execution contexts must serialize access using the appropriate EDP-Threading architecture rather than assuming internal locking.

Initialization is `Uninitialized → Running`. `BeginQuiesce` prevents new admission and chooses `Quiescent` immediately only when no occupied records remain; otherwise it enters `Quiescing`. Final reclamation transitions to `Quiescent` when active count reaches zero.

Measured host `sizeof(Runtime)` fixtures on 2026-09-27 were: 104 bytes for plan 2/1/1, 168 for 4/3/1, 352 for 10/8/2, and 656 for 20/16/4 with 4-byte Request/Response fixture types. These are validation measurements, not ABI guarantees for arbitrary Command/Executor types.