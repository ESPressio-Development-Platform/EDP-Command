# Internal API and Extension Boundaries

The current implementation intentionally has very little separately named **INTERNAL API**. Most declarations in `src/command` are public templates because the library is header-only. Public source accessibility does not make Runtime internals extension contracts.

The important internal cross-object contract is the Handle/Runtime protocol: a Handle retains `(runtime pointer, slot index, generation)` privately; Runtime `Observe`, finite waits, `RequestCancellation`, `TakeResponse` and `Release` validate that identity through generation matching. `DispatchResult` is a friend solely so successful dispatch can construct the otherwise private Handle. Consumers must not recreate or persist this identity as an InvocationId.

`TakeResponseResult::Storage` and `MarkLive` are public only to support the typed extraction path in the header-only implementation. They are implementation-facing operations: `Storage` exposes uninitialized embedded storage, and `MarkLive` may only be called after exactly one valid `TResponse` has been constructed there. Violating that pairing breaks destruction invariants.

Handler is an application extension contract rather than a base-class API. `Runtime<TCommand,THandlerProvider,TWaitProvider,TPlan>` expects the resolved Handler provider to expose `Execute(const RequestType&, CancellationToken)` returning an execution-result-compatible value and respecting the non-throwing architectural contract. Handler ownership remains with the application; Runtime stores only a pointer.

The Runtime also borrows the Architecture-resolved EDP-Threading bounded wait/wake provider. Handle waits are finite and non-consuming. Runtime rechecks terminal state after the provider returns so terminal state wins timeout/interruption races.

Inbound/outbound adapter implementations may consume `InboundAdmission` and `OutboundCompletion`; they must preserve adapter-owned correlation and exactly-once completion rather than adding Transport identity to Command. Semantic terminal `Outcome` is distinct from `CompletionPublicationResult`, which describes whether a publication attempt itself was accepted.

## F4 scoped-dispatch coordination

`DispatchScoped` is deliberately stateless glue between Command's compile-time scope policy and operations already selected by a higher integration/routing layer. It owns no routing table, provider reference, destination identity or Request. The callable boundary is significant: it avoids imposing copyability on Command Request Types when local and remote domains need independent ownership/materialization.

All selected operations must be non-throwing and return a non-void domain outcome. `LocalAndRemoteDispatchResult` stores only those two returned results; it creates no continuing relationship between their lifecycles. The concrete invocation sequence is an implementation detail and is not an extension contract.
