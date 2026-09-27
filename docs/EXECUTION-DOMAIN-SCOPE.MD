# Execution-Domain Dispatch Scope

## Purpose

EDP-Command implements the locked F4 per-call execution-domain policy without taking ownership of routing or Transport. A routed caller can state whether a Command dispatch is eligible for the local execution domain, the remote execution domain, or both while preserving a statically derivable result shape.

The three compile-time policy Types are:

- `LocalOnly`
- `RemoteOnly`
- `LocalAndRemote`

`ExecutionDomainScope<TScope>` identifies those three policy Types. Scope is Dispatch control metadata. It is not part of the Command Request, schema, TypeIdentifier, serialization format, destination identity, or retained invocation state.

## Local Runtime remains local-only

`Runtime::Dispatch(request)` remains the destination-free local Command admission operation. It does not inspect scope, discover routes, select providers, or consult outbound integrations. Existing local-only applications therefore retain their existing Runtime API and storage model.

`InboundAdmission::Dispatch(request)` also remains an ordinary local admission facade. An explicit `InboundAdmission::Dispatch(LocalOnly{}, request)` overload is provided when an integration wants to state that policy at the call site. No `RemoteOnly` or `LocalAndRemote` inbound overload exists because inbound admission is, by definition, a local Runtime capability.

## Higher-layer scoped coordination

`DispatchScoped` coordinates operations that have already been selected by the caller or higher integration/routing layer. Command does not decide what the remote operation means or where it sends work.

```cpp
auto localOperation = [&]() noexcept {
    return runtime.Dispatch(MyRequest{42});
};

auto remoteOperation = [&]() noexcept {
    return router.DispatchToSelectedDestination(MyRequest{42});
};

auto result = Command::DispatchScoped(
    Command::LocalAndRemote{},
    localOperation,
    remoteOperation
);
```

Both operations must be non-throwing and must return an observable, non-`void` result. This keeps failure/outcome semantics in the domain that owns each operation rather than inventing a Command-wide aggregate result.

The coordinator deliberately accepts operations rather than a Request object. That boundary avoids assuming that a Request is copyable or trying to transfer the same Request object into two independent ownership domains. The higher layer constructs or retains whatever domain-specific input each selected operation requires.

## Result shape

`LocalOnly` returns the selected local operation's result directly.

`RemoteOnly` returns the selected higher-layer remote operation's result directly and performs no local Runtime call. There is therefore no local `DispatchResult` for a RemoteOnly call.

`LocalAndRemote` returns `LocalAndRemoteDispatchResult<TLocalResult, TRemoteResult>`. The two outcomes remain structurally separate and are accessed through `Local()` and `Remote()`. The result Type intentionally exposes no aggregate `Accepted`, success, quorum, rollback, or combined lifecycle semantic.

A remote result can itself be a scalar or a topology-bounded per-destination result collection owned by the higher integration/routing layer. EDP-Command neither defines nor dynamically sizes that collection.

## Independence contract

For `LocalAndRemote`, the selected local operation and selected remote operation are each invoked exactly once. A negative local outcome does not suppress the remote operation, and a negative/unavailable remote outcome does not turn into local fallback or suppress local admission.

There is no cross-domain ordering contract. The implementation necessarily invokes operations in some sequence, but applications must not infer causality or ordering semantics from that sequence. Local and remote invocation lifecycles remain independent after the scoped call returns.

## Resource and ownership consequences

The scope Types are empty compile-time policy Types. `DispatchScoped` retains no provider, route, Request, destination, Handle, queue, or runtime state. `LocalAndRemoteDispatchResult` contains only the two result objects it returns to the caller. No new persistent state is added to `Runtime`, `ResourcePlan`, invocation records, or Bootstrap.

No new EDP dependency is introduced. Transport, Serialisation, Security, destination identity, routing topology, correlation and remote retry remain outside EDP-Command.

## Unsupported interpretations

F4 does not mean:

- implicit broadcast or fan-out;
- runtime provider discovery;
- automatic local fallback after remote failure;
- automatic remote fallback after local failure;
- aggregate success/quorum;
- rollback across domains;
- a local/remote execution ordering guarantee;
- embedding routing scope in the Request or wire representation.
