# Architecture

## Composition and Bootstrap

A Command type exposes an exclusive `Handler<TCommand>` capability in the Command domain. A valid EDP-System Architecture supplies exactly one such Handler and exactly one external `EDP-Threading::BoundedWaitWake` provider. `Bootstrap<TCommand, TArchitecture, TPlan>` resolves and borrows those application-owned providers and establishes Runtime wiring. Wait-provider capacity must cover every planned invocation slot. The topology is immutable after initialization.

## Lifecycle and outcome

Dispatch is immediate and transactional. Before Handle publication the runtime reserves bounded admission resources and establishes the Request. Failure before commitment leaves no observable invocation.

Accepted lifecycle is `Queued -> Executing -> Completed`, with cancellation able to terminate as `Cancelled`. There is no `Failed` lifecycle state. Lifecycle and semantic terminal outcome are separate dimensions: every terminal invocation exposes one `Outcome` of `Succeeded`, `Rejected`, `Failed`, or `Cancelled`. Cancellation therefore remains the distinct `InvocationState::Cancelled` lifecycle state while also exposing `Outcome::Cancelled`. Failed outcome detail is limited to `ExecutorFailure` or `IntegrationFailure` in the Command domain.

`InvocationObservation` exposes the genuine state predicates `DidSucceed()`, `WasRejected()`, `DidFail()`, and `WasCancelled()`.

## Handle, waiting and ownership

The Handle is exclusive and move-only. It is the local observation authority. No public InvocationId exists in v1. Retaining a Handle retains terminal record ownership; release permits reclamation.

`WaitFor` and `WaitUntil` are finite and non-consuming. Each invocation record maps to the same stable index in the external bounded Threading wait/wake provider. Runtime re-observes authoritative state after every provider return, so terminal state wins a simultaneous timeout/interruption race. Repeated waits on a retained terminal Handle return `Terminal`. Same-Handle operations are caller-serialized.

## Memory model

Invocation records and queues are fixed-capacity. Requests and Responses are established/destroyed through EDP-Memory typed lifetime operations. Successful/rejected non-void Commands own exactly one typed Response until extraction or record reclamation. Command does not own the Threading synchronization objects; Runtime retains only a borrowed provider pointer.

## Cancellation

Cancellation is cooperative for executing work and immediate for queued work. Terminal completion/cancellation wakes the invocation's Threading wait slot. `RequestCancellation` reports `Requested`, `AlreadyRequested`, `TooLate`, or `InvalidHandle`. Transport failure is never represented as cancellation.

## Typed operation results

Fallible operations do not use Boolean success/failure. Runtime initialization, execution attempts, quiescing, cancellation requests, waits, response extraction and integration completion publication each expose dedicated strongly typed result vocabularies. Boolean APIs are retained only where they are genuine predicates.

## Shutdown

`BeginQuiesce` stops new admission. Existing invocations remain observable. Complete reclamation may be delayed by retained Handles.

## Execution-domain scope

F4 scope is compile-time Dispatch control metadata: `LocalOnly`, `RemoteOnly`, or `LocalAndRemote`. These names re-export the canonical `EDP-Primitives` execution-domain scope Types rather than defining Command-local duplicates. It is never retained in Request/schema/wire state. Destination-free `Runtime::Dispatch` remains local-only; a higher routing/integration layer may use `DispatchScoped` only after selecting the applicable local and/or remote operations.

LocalOnly returns the local operation result directly. RemoteOnly invokes no local Runtime and returns only the higher-layer remote result. LocalAndRemote invokes both selected operations independently and returns structurally separate `Local()` and `Remote()` outcomes. Neither domain can create implicit fallback, rollback or suppression in the other, and there is no aggregate success/quorum or cross-domain ordering contract. `DispatchScoped` retains no provider, route, Request or runtime state, so F4 adds no persistent Runtime resource dimension.

## Integration

Inbound adapters use the same local bounded admission semantics; `InboundAdmission::Dispatch(LocalOnly{}, request)` makes that scope explicit, while the original shorthand remains available. Outbound adapters receive only Request/cancellation/completion capabilities. Detailed Transport/codec/security failures remain owned by those domains. `CompletionPublicationResult` reports whether an attempted terminal publication was `Accepted`, `AlreadyCompleted`, or `Unavailable`; it is deliberately separate from the invocation's semantic `Outcome`.

## Determinism

Capacity is compile-time planned. No admission wait exists. Back-pressure/retry belongs above Command. Terminal publication is one-way and duplicate integration completion is rejected. The pre-H16 measured Command Runtime cost of adding external finite-wait support was one pointer (+8 bytes on the accepted host measurements), independent of invocation capacity; external Threading storage is separately owned and planned. Final H16 regression must remeasure the Runtime before those figures are treated as current.
