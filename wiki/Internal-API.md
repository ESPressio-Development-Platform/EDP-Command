# Internal API and Extension Boundaries

The current implementation intentionally has very little separately named **INTERNAL API**. Most declarations in `src/command` are public templates because the library is header-only. Public source accessibility does not make Runtime internals extension contracts.

The important internal cross-object contract is the capability/Runtime protocol. A Handle or reservation retains `(runtime pointer, slot index, non-wrapping generation)` privately. Handle operations, inbound publish/abort and outbound commit/abort validate that identity. `DispatchResult` and reservation-result Types are friends solely so successful preparation can construct the otherwise private capabilities. Consumers must not recreate or persist these tuples as an InvocationId or wire correlation.

`TakeResponseResult::Storage` and `MarkLive` are public only to support the typed extraction path in the header-only implementation. They are implementation-facing operations: `Storage` exposes uninitialized embedded storage, and `MarkLive` may only be called after exactly one valid `TResponse` has been constructed there. Violating that pairing breaks destruction invariants.

Handler is an application extension contract rather than a base-class API. `Runtime<TCommand,THandlerProvider,TWaitProvider,TMutexProvider,TPlan>` expects the resolved Handler provider to expose `Execute(const RequestType&, CancellationToken)` returning an execution-result-compatible value and respecting the non-throwing architectural contract. Handler ownership remains with the application; Runtime stores only a pointer. Claim and completion occur under the borrowed Command mutex, while `Execute` itself runs outside it.

Runtime also borrows the Architecture-resolved EDP-Threading bounded wait/wake and ordinary-mutex providers. Handle waits and terminal wakes run without the mutex. Runtime rechecks terminal state after the wait provider returns so terminal state wins timeout/interruption races.

Inbound adapters may consume `InboundAdmission::Prepare`, populate only the unpublished destination after complete validation and commit once. Outbound adapters receive a `RemoteHandoffReservation` borrow only for their one bounded call. They construct the family-owned `CommandInvocationCorrelation` but retain delivery/wire identifiers and the bounded correlation-to-local-Handle/terminal-proof mapping. Semantic terminal `Outcome` is distinct from `CompletionPublicationResult`, which describes whether a publication attempt itself was accepted.

## F4 scoped-dispatch coordination

Generic `DispatchScoped` is stateless glue between Command's compile-time scope policy and operations already selected by a higher integration/routing layer. It owns no routing table, provider reference or destination identity. The callable boundary avoids imposing copyability on Command Request Types when local and remote domains need independent ownership/materialization.

All selected operations must be non-throwing and return a non-void domain outcome. `LocalAndRemoteDispatchResult` stores only those two returned results; it creates no continuing relationship between their lifecycles. Local admission before remote handoff is an explicit contract. It does not imply local Handler completion, remote arrival or recipient execution order.

Typed overloads consume the move-only outbound stage. Runtime validates it under the Command mutex, releases the mutex, invokes the adapter with the borrowed Request, then retires the stage under the mutex. Adapter code must synchronously consume/encode the Request and must not retain the reference.
