# Resources, Lifecycle and Concurrency

Command's core memory is compile-time bounded by `ResourcePlan`. Runtime embeds invocation records, queue indices and exactly `RemoteHandoffCapacity` outbound pointer/generation stages; a zero handoff capacity uses empty state. No admission allocates. Requests and Responses live in aligned raw slot storage and their lifetime is mediated through EDP-Memory. Outbound stages never duplicate a Request payload.

Ordinary dispatch is transactional: Runtime state and queue/slot capacity are checked before Request construction into a slot. Successful admission increments a non-wrapping slot generation, marks retention and queues the slot. Failed admission creates no invocation. Remote ingress may instead reserve exact queue/invocation entitlement and a constructed but unpublished Request, populate it outside the mutex, then publish only at commit; abort/destruction restores everything.

A Handle intentionally extends the lifetime of a terminal record. Reclamation occurs only after terminal state **and** Handle release. This permits stable observation and one-time response extraction without a global InvocationId. Response extraction transfers the object and immediately destroys Runtime's stored instance.

`ExecuteOne` claims at most one queue entry under the application-owned Command mutex and checks `_executing` against `ExecutionConcurrency`. It releases the mutex for Handler execution, then reacquires it to publish completion. V1 does not itself spawn workers; application-owned contexts may execute concurrently up to the planned limit. Cancellation uses an atomic flag for executing work and immediate terminal marking for queued work. It is not forced thread cancellation.

Handle waiting is provided by an externally owned EDP-Threading `BoundedWaitWake` provider resolved through the immutable application Architecture. `WaitFor` and `WaitUntil` are finite and non-consuming and run outside the Command mutex. Runtime checks terminal state both before and after the provider wait; therefore an invocation that becomes terminal during the wait is reported as `WaitResult::Terminal` even if the provider simultaneously reports timeout or interruption. Terminal wake publication also runs outside the mutex.

The Runtime serializes short state transitions through exactly one application-supplied `OrdinaryMutex<Composition::RuntimeMutexIdentity>`. This covers dispatch, reserve/commit/abort, Handle observation/mutation, cancellation, claim/completion and handoff-stage lifecycle. Same object instances such as one mutable Handle or reservation still require normal caller ownership discipline. No application Handler, wait/wake provider or remote adapter callback runs while the mutex is held.

Initialization is `Uninitialized` → `Running`. `Initialize` reports `InitializationResult`. `BeginQuiesce` reports `QuiesceResult`, prevents new invocation and handoff-stage admission and chooses `Quiescent` immediately only when no invocation or outbound stage remains; otherwise it enters `Quiescing`. Final reclamation/abort/handback transitions to `Quiescent` when active count reaches zero.

Semantic terminal result is represented by `Outcome`, independent from lifecycle state. A cancelled invocation has `InvocationState::Cancelled` and `Outcome::Cancelled`; successful, rejected and failed invocations are lifecycle `Completed` with the corresponding terminal Outcome.

Measured x86-64 GCC host `sizeof(Runtime)` fixtures for the Mesh V1 contract are: **136 bytes** for local-only plan 2/1/1/0, **232 bytes** for small plan 4/3/1/2, **448 bytes** for representative plan 10/8/2/4, and **816 bytes** for high plan 20/16/4/8 with 4-byte Request/Response fixture types. These are validation measurements, not ABI guarantees for arbitrary Command/Handler/provider or target ABI Types.

## Execution-domain scope and remote-stage resources

The scope policy Types are empty compile-time tags and generic `DispatchScoped` retains no state after the call. `LocalAndRemoteDispatchResult` is caller-owned return state containing only the independently produced local and remote result objects.

Typed outbound integration uses the separately declared `RemoteHandoffCapacity`. Each live record contains only a Request pointer, non-wrapping generation and occupied state. Prepare/commit/abort transitions use the same Command mutex; the bounded adapter call occurs outside it. LocalAndRemote establishes local-attempt-before-remote-handoff order only. Continuing remote lifecycle state is carried by the adapter binding inside `RemoteCommandOperation`, not a Command payload queue or Mesh-shadow Command side table.
