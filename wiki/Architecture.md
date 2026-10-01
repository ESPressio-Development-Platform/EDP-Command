# Architecture

## Semantic Command model

One semantic operation is one Command Primitive Type.

Every valid Command:

- satisfies `Primitives::PrimitiveType` and therefore both `System::SchemaType` and `Serialisation::SerialisableType`;
- declares `Family = Command::Family`;
- owns a stable universal TypeIdentifier identifying the operation contract;
- normally declares `System::FieldSet<>` because operation payload data lives in Request/Response Types;
- explicitly declares serialisable schema-bearing `Request` and `Response` Types.

Command, Request and Response identities are deliberately separate. Request/Response are ordinary semantic data Types, not independently deployed Primitive Types. Numeric `System::FieldIdentifier` values are authoritative for their schema-visible members.

`Command::NoRequestPayload` and `Command::NoResponsePayload` are distinct identified zero-field SchemaTypes. `void`, anonymous payloads, generated IDs, name hashing, RTTI and source/member order are not part of the semantic contract.

`Command::Family` is the stable ESPressio Command Primitive family under ESPressio Type Authority 1. Stage A restores this semantic family association while preserving the existing Bootstrap/Runtime execution architecture.

EDP-Serialisation is now a direct compile-time dependency for `SerialisableType`; Command still owns no codec, wire profile, parser, or buffer behavior.

## Composition and Bootstrap

A Command Type exposes an exclusive `Handler<TCommand>` capability in the Command domain. A valid EDP-System Architecture supplies exactly one such Handler and exactly one external `EDP-Threading::BoundedWaitWake` provider. `Bootstrap<TCommand, TArchitecture, TPlan>` resolves and borrows those application-owned providers and establishes Runtime wiring. Wait-provider capacity must cover every planned invocation slot. Topology is immutable after initialization.

## Lifecycle and outcome

Dispatch is immediate and transactional. Before Handle publication Runtime reserves bounded admission resources and establishes the Request. Failure before commitment leaves no observable invocation.

Accepted lifecycle is `Queued -> Executing -> Completed`, with cancellation able to terminate as `Cancelled`. There is no `Failed` lifecycle state. Lifecycle and semantic terminal outcome are separate dimensions: every terminal invocation exposes one `Outcome` of `Succeeded`, `Rejected`, `Failed`, or `Cancelled`. Failed outcome detail is limited to `ExecutorFailure` or `IntegrationFailure` in the Command domain.

`InvocationObservation` exposes `DidSucceed()`, `WasRejected()`, `DidFail()`, and `WasCancelled()`.

## Handle, waiting and ownership

The Handle is exclusive and move-only. It is the local observation authority. No public InvocationId exists in v1. Retaining a Handle retains terminal record ownership; release permits reclamation.

`WaitFor` and `WaitUntil` are finite and non-consuming. Each invocation record maps to the same stable index in the external bounded Threading wait/wake provider. Runtime re-observes authoritative state after every provider return, so terminal state wins a simultaneous timeout/interruption race. Repeated waits on a retained terminal Handle return `Terminal`. Same-Handle operations are caller-serialized.

## Memory model

Invocation records and queues are fixed-capacity. Requests and Responses are established/destroyed through EDP-Memory typed lifetime operations. Successful/rejected Commands own exactly one typed Response until extraction or record reclamation. Command does not own Threading synchronization objects; Runtime retains only a borrowed provider pointer.

## Cancellation

Cancellation is cooperative for executing work and immediate for queued work. Terminal completion/cancellation wakes the invocation's Threading wait slot. `RequestCancellation` reports `Requested`, `AlreadyRequested`, `TooLate`, or `InvalidHandle`. Transport failure is never represented as cancellation.

## Typed operation results

Fallible operations do not use Boolean success/failure. Runtime initialization, execution attempts, quiescing, cancellation requests, waits, response extraction and integration completion publication each expose dedicated strongly typed result vocabularies. Boolean APIs are retained only for genuine predicates.

`ExecutionResult<void>` remains a utility specialization, but `void` is not a valid semantic Command Response. Use `NoResponsePayload`.

## Shutdown

`BeginQuiesce` stops new admission. Existing invocations remain observable. Complete reclamation may be delayed by retained Handles.

## Execution-domain scope

`LocalOnly`, `RemoteOnly`, and `LocalAndRemote` are compile-time Dispatch control metadata re-exported from EDP-Primitives. They are never retained in Request/schema/wire state. Destination-free `Runtime::Dispatch` remains local-only; a higher routing/integration layer may use `DispatchScoped` only after selecting applicable local/remote operations.

LocalOnly returns the local operation result directly. RemoteOnly invokes no local Runtime. LocalAndRemote invokes both selected operations independently and returns structurally separate `Local()` and `Remote()` outcomes. There is no aggregate success, fallback, rollback, suppression, quorum or cross-domain ordering contract.

## Integration

Inbound adapters use the same local bounded admission semantics. Outbound adapters receive only Request/cancellation/completion capabilities. Detailed Transport/codec/security failures remain owned by those domains. `CompletionPublicationResult` is deliberately separate from semantic `Outcome`.

Maintained integration examples use schema-bearing semantic payload Types even where a narrow integration helper does not itself impose the complete `CommandType` concept.

## Determinism

Capacity is compile-time planned. No admission wait exists. Back-pressure/retry belongs above Command. Terminal publication is one-way and duplicate integration completion is rejected. Stage-A schema qualification adds compile-time metadata only and does not add Runtime registry or heap storage.
