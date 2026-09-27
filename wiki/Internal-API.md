# Internal API and Extension Boundaries

The current implementation intentionally has very little separately named **INTERNAL API**. Most declarations in `src/command` are public templates because the library is header-only. Public source accessibility does not make Runtime internals extension contracts.

The important internal cross-object contract is the Handle/Runtime protocol: a Handle retains `(runtime pointer, slot index, generation)` privately; Runtime `Observe`, `RequestCancellation`, `TakeResponse` and `Release` validate that identity through generation matching. `DispatchResult` is a friend solely so successful dispatch can construct the otherwise private Handle. Consumers must not recreate or persist this identity as an InvocationId.

`TakeResponseResult::Storage` and `MarkLive` are public only to support the typed extraction path in the header-only implementation. They are implementation-facing operations: `Storage` exposes uninitialized embedded storage, and `MarkLive` may only be called after exactly one valid `TResponse` has been constructed there. Violating that pairing breaks destruction invariants.

Executor is an application extension contract rather than a base-class API. `Runtime<TCommand,TExecutor,TPlan>` expects `TExecutor::Execute(const RequestType&, CancellationToken)` to return an execution-result-compatible value and to respect the non-throwing architectural contract. Executor ownership remains with the caller; Runtime stores only a pointer.

Inbound/outbound adapter implementations may consume `InboundAdmission` and `OutboundCompletion`; they must preserve adapter-owned correlation and exactly-once completion rather than adding Transport identity to Command.