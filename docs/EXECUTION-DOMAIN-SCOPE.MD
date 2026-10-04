# Execution-Domain Dispatch Scope

## Purpose

EDP-Command implements the locked F4 per-call execution-domain policy without taking ownership of routing or Transport. A routed caller can state whether a Command dispatch is eligible for the local execution domain, the remote execution domain, or both while preserving a statically derivable result shape.

The three compile-time policy Types are:

- `LocalOnly`
- `RemoteOnly`
- `LocalAndRemote`

`ExecutionDomainScope<TScope>` aliases the canonical `Primitives::ExecutionDomain::Scope<TScope>` concept and therefore identifies those same three policy Types, including cv/ref-qualified forms. Scope is Dispatch control metadata. It is not part of the Command Request, schema, TypeIdentifier, serialization format, destination identity, or retained invocation state.

## Local Runtime remains local-only

`Runtime::Dispatch(request)` remains the destination-free local Command admission operation. It does not inspect scope, discover routes, select providers, or consult outbound integrations. Existing local-only applications therefore retain their existing Runtime API and storage model.

`InboundAdmission::Dispatch(request)` also remains an ordinary local admission facade. An explicit `InboundAdmission::Dispatch(LocalOnly{}, request)` overload is provided when an integration wants to state that policy at the call site. No `RemoteOnly` or `LocalAndRemote` inbound overload exists because inbound admission is, by definition, a local Runtime capability.

## Higher-layer scoped coordination

`DispatchScoped` coordinates operations already selected by the caller or higher integration/routing layer. Command does not choose destinations or routes. For `LocalAndRemote`, it establishes one deliberate ordering fact: the local admission attempt occurs before the independent remote handoff. It does not wait for local Handler execution.

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

The generic coordinator accepts operations rather than duplicating a Request. The higher layer therefore constructs or retains whatever domain-specific input each selected operation requires, including move-only Request Types.

## Synchronized typed remote handoff

Mesh-style integration uses `OutboundHandoff<TCommand,TRuntime>`. `Prepare(request)` briefly enters the Runtime's application-provided Command mutex and reserves one of `ResourcePlan::RemoteHandoffCapacity` fixed stages. The returned move-only reservation retains only an immutable borrow; the caller must keep the Request alive until commit or abort.

```cpp
MyRequest remoteRequest{42};
Command::OutboundHandoff<MyCommand, MyRuntime> outbound(runtime);
auto prepared = outbound.Prepare(remoteRequest);
if (!prepared.Accepted()) {
    // No adapter call occurred.
}

auto stage = std::move(prepared).TakeReservation();
auto localOperation = [&]() noexcept {
    return runtime.Dispatch(MyRequest{42});
};
auto remoteOperation = [&](const MyRequest& request) noexcept {
    // This one bounded call must consume/encode the borrow synchronously.
    return mesh.Dispatch(request);
};

auto result = Command::DispatchScoped(
    Command::LocalAndRemote{},
    std::move(stage),
    localOperation,
    remoteOperation
);
```

The coordinator invokes `localOperation` first, then commits the stage. Runtime releases the Command mutex before calling the adapter and reacquires it only to retire the stage, so no Mesh work or application callback runs under Command synchronization. Destroying or aborting an uncommitted stage calls no adapter. `RemoteOnly` has a corresponding overload that consumes a prepared stage without local admission.

## Result shape

`LocalOnly` returns the selected local operation's result directly.

`RemoteOnly` returns the selected higher-layer remote operation's result directly and performs no local Runtime admission. There is therefore no local `DispatchResult` for a RemoteOnly call.

`LocalAndRemote` returns `LocalAndRemoteDispatchResult<TLocalResult, TRemoteResult>`. The two non-throwing-movable outcomes remain structurally separate and are accessed through `Local()` and `Remote()`. The result Type intentionally exposes no aggregate `Accepted`, success, quorum, rollback, or combined lifecycle semantic.

A Mesh adapter may return `RemoteCommandOperation<TCommand,TBinding>`. That move-only family surface contains the binding for a bounded frozen recipient set and exposes per-recipient identity/correlation, observation, finite waits, cancellation and at-most-once Response extraction without making Command own routing or wire delivery.

## Independence contract

For `LocalAndRemote`, the selected local operation and selected remote operation are each invoked exactly once, in that order. A negative local outcome does not suppress the remote operation, and a negative/unavailable remote outcome does not turn into local fallback or suppress the already-attempted local admission.

This is an admission/handoff ordering contract only. It promises no local Handler completion before remote handoff, no route/arrival/execution order between recipients and no relationship between the independent lifecycles after the call returns.

## Resource and ownership consequences

The scope Types are empty compile-time policy Types. Generic `DispatchScoped` retains no state. Typed handoff staging adds exactly `RemoteHandoffCapacity` fixed pointer/generation records to Runtime and never duplicates Request payloads. A stage exists only until its bounded adapter call returns or it is aborted. `LocalAndRemoteDispatchResult` contains only the two independent result objects.

No new EDP dependency is introduced. Transport, Security, routing topology, retry and wire delivery remain outside EDP-Command. Command owns only its strong semantic `CommandInvocationCorrelation`; adapters construct it from authenticated source Device/runtime and the original invocation delivery identifier.

## Unsupported interpretations

F4 does not mean:

- implicit broadcast or fan-out;
- runtime provider discovery;
- automatic local fallback after remote failure;
- automatic remote fallback after local failure;
- aggregate success/quorum;
- rollback across domains;
- local Handler completion before remote handoff;
- route, arrival or recipient execution ordering;
- embedding routing scope in the Request or wire representation.
