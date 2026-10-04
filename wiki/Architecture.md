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

A Command Type exposes an exclusive `Handler<TCommand>` capability in the Command domain. A valid EDP-System Architecture supplies exactly one such Handler, one external `EDP-Threading::BoundedWaitWake` provider and one external Command-identity ordinary mutex provider. `Bootstrap<TCommand, TArchitecture, TPlan>` resolves and borrows those application-owned providers and establishes Runtime wiring. Wait-provider capacity must cover every planned invocation slot. Topology is immutable after initialization.

## Lifecycle and outcome

Dispatch is immediate and transactional. Before Handle publication Runtime reserves bounded admission resources and establishes the Request. Failure before commitment leaves no observable invocation.

Accepted lifecycle is `Queued -> Executing -> Completed`, with cancellation able to terminate as `Cancelled`. There is no `Failed` lifecycle state. Lifecycle and semantic terminal outcome are separate dimensions: every terminal invocation exposes one `Outcome` of `Succeeded`, `Rejected`, `Failed`, or `Cancelled`. Failed outcome detail is limited to `ExecutorFailure` or `IntegrationFailure` in the Command domain.

`InvocationObservation` exposes `DidSucceed()`, `WasRejected()`, `DidFail()`, and `WasCancelled()`.

## Handle, waiting and ownership

The Handle is exclusive and move-only. It is the local observation authority. No public InvocationId exists in v1. Retaining a Handle retains terminal record ownership; release permits reclamation.

`WaitFor` and `WaitUntil` are finite and non-consuming. Each invocation record maps to the same stable index in the external bounded Threading wait/wake provider. Runtime re-observes authoritative state after every provider return, so terminal state wins a simultaneous timeout/interruption race. Repeated waits on a retained terminal Handle return `Terminal`. Same-Handle operations are caller-serialized.

## Memory model

Invocation records, queues and outbound handoff stages are fixed-capacity. Requests and Responses are established/destroyed through EDP-Memory typed lifetime operations. Successful/rejected Commands own exactly one typed Response until extraction or record reclamation. Command does not own Threading synchronization objects; Runtime borrows both wait/wake and ordinary-mutex providers.

## Cancellation

Cancellation is atomic/cooperative for executing work and immediate for queued work. Claim and completion are short serialized transitions, while Handler execution and terminal wake occur outside the Command mutex. `RequestCancellation` reports `Requested`, `AlreadyRequested`, `TooLate`, or `InvalidHandle`. Transport failure is never represented as cancellation.

## Typed operation results

Fallible operations do not use Boolean success/failure. Runtime initialization, execution attempts, quiescing, cancellation requests, waits, response extraction and integration completion publication each expose dedicated strongly typed result vocabularies. Boolean APIs are retained only for genuine predicates.

`ExecutionResult<void>` remains a utility specialization, but `void` is not a valid semantic Command Response. Use `NoResponsePayload`.

## Shutdown

`BeginQuiesce` stops new admission. Existing invocations remain observable. Complete reclamation may be delayed by retained Handles.

## Execution-domain scope

`LocalOnly`, `RemoteOnly`, and `LocalAndRemote` are compile-time Dispatch control metadata re-exported from EDP-Primitives. They are never retained in Request/schema/wire state. Destination-free `Runtime::Dispatch` remains local-only; a higher routing/integration layer may use `DispatchScoped` only after selecting applicable local/remote operations.

LocalOnly returns the local operation result directly. RemoteOnly invokes no local Runtime admission. LocalAndRemote attempts local admission first, then independently performs remote handoff without awaiting Handler completion and without suppressing remote work after local refusal. Results remain structurally separate; there is no aggregate success, fallback, rollback, suppression or quorum contract.

## Integration

Inbound adapters reserve constructed-but-unpublished Request backing, populate after complete validation outside the mutex and publish only at commit. Outbound adapters receive a plan-bounded Request-borrow stage whose one call runs outside the mutex. `CommandInvocationCorrelation` supplies semantic identity while adapters retain routing, delivery and wire mapping. `RemoteCommandOperation` provides bounded per-recipient observation/wait/cancellation/response semantics. Detailed Transport/codec/security failures remain owned by those domains. `CompletionPublicationResult` is deliberately separate from semantic `Outcome`.

Maintained integration examples use schema-bearing semantic payload Types even where a narrow integration helper does not itself impose the complete `CommandType` concept.

## Determinism

Capacity is compile-time planned, including `RemoteHandoffCapacity`. No admission wait exists. Back-pressure/retry belongs above Command. Terminal publication is one-way and duplicate integration completion is rejected. No Runtime registry, shadow payload queue or heap storage is introduced.
