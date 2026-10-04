# Command Architecture Contract

## Scope and ownership

EDP-Command owns the semantic Command Primitive family, bounded local Command admission, invocation lifecycle, execution, cancellation, finite local waiting, response retention/extraction, unpublished inbound staging, synchronized outbound handoff staging, semantic Command correlation and the source-facing remote-operation surface. It deliberately does not own Transport, codec mechanics, Security, routing, delivery identity, application scheduling or provider discovery.

## Semantic Command and payload schema contract

One semantic operation is one Command Primitive Type.

Every valid `CommandType<TCommand>`:

- satisfies `Primitives::PrimitiveType<TCommand>`;
- therefore has a stable universal `System::TypeIdentifier` and canonical `System::FieldSet`;
- declares `Family = Command::Family`;
- explicitly declares `Request` and `Response` Types;
- requires both Request and Response to satisfy `System::SchemaType` and `Serialisation::SerialisableType`.

The operation Type identifies the operation contract itself. It normally contains no payload members and therefore uses `System::FieldSet<>`.

Request and Response are independent semantic data Types with their own TypeIdentifiers and Field schemas. They are not separate deployed Primitive Types, and their identities do not replace the Command operation identity. A schema-bearing payload Type may be reused by multiple Commands when semantically appropriate.

Command defines two distinct canonical zero-payload Types:

- `Command::NoRequestPayload`;
- `Command::NoResponsePayload`.

Both are real identified zero-field Types satisfying both `System::SchemaType` and `Serialisation::SerialisableType`. They are intentionally not `void`, `std::monostate`, one shared empty marker, or exceptions to the schema contract.

No compiler-generated identity, name hashing, RTTI, source location, member order or runtime registration participates in the semantic contract. Numeric `System::FieldIdentifier` is authoritative for payload Field identity.

`Command::Family` is the stable ESPressio Command Primitive family under ESPressio Type Authority 1. The current Stage-A reconciliation restores semantic family association without redesigning the already-maintained Bootstrap/Runtime deployment mechanics.

EDP-Command now directly depends on EDP-Serialisation for the canonical compile-time `SerialisableType` predicate. Universal Primitive serialisability arrives through EDP-Primitives; Command additionally constrains Request and Response directly.

## Composition and Bootstrap

Each Command Type has an exclusive `Composition::Handler<TCommand>` capability. A valid application architecture supplies exactly one Handler provider in the Command domain. Command also requires exactly one external `EDP-Threading::BoundedWaitWake` provider and exactly one external `EDP-Threading::OrdinaryMutex<Composition::RuntimeMutexIdentity>` provider. `Bootstrap<TCommand, TArchitecture, TPlan>` resolves these providers statically, borrows the application-owned instances, and constructs Runtime wiring without runtime discovery or topology mutation.

The wait provider's compile-time `Capacity` must be at least `ResourcePlan::InvocationCapacity`. Once initialized, application topology is immutable.

## Admission, lifecycle and terminal outcome

A dispatch is transactional: failure creates no invocation; success atomically consumes one bounded invocation slot and returns the invocation's exclusive move-only `Handle`. Accepted invocations begin `Queued`, may become `Executing`, and terminate as `Completed` or `Cancelled`.

Lifecycle state and terminal semantic outcome are deliberately separate. Every terminal invocation has an `Outcome`: `Succeeded`, `Rejected`, `Failed`, or `Cancelled`. Cancellation therefore remains `InvocationState::Cancelled` while also exposing `Outcome::Cancelled`. `InvocationObservation` provides Boolean state predicates `DidSucceed()`, `WasRejected()`, `DidFail()`, and `WasCancelled()`. `ExecutionFailure` is orthogonal failure-cause detail when the outcome is `Failed`.

`DispatchFailure` distinguishes `NoCapacity`, `TypeUnrecognised`, `RuntimeUnavailable` and `BindingUnavailable`; the core typed Runtime directly produces the capacity/runtime cases while integration/binding layers may preserve the wider vocabulary.

## Static resources and determinism

`ResourcePlan<InvocationCapacity, QueueCapacity, ExecutionConcurrency, RemoteHandoffCapacity>` fixes core capacities at compile time; the fourth value defaults to zero for local-only applications. Runtime storage is embedded in fixed arrays or zero-capacity state that compiles away. Request and Response objects are constructed, moved and destroyed in caller/runtime-owned raw storage through EDP-Memory; Command performs no heap allocation.

Each invocation slot maps deterministically to the same indexed slot in the external bounded wait/wake provider. Synchronization objects remain owned by EDP-Threading/application composition rather than Command. Runtime borrows the Command-identity ordinary mutex and fails closed if its provider cannot acquire/release as contracted.

A retained Handle deliberately keeps a terminal record occupied until release/destruction so observation, waiting and response extraction remain valid. A response is extractable at most once.

## Finite waiting

`Handle::WaitFor(Duration)` and `Handle::WaitUntil(MonotonicTimestamp)` are finite, non-consuming observation operations. They never create waiter collections inside Command. If the invocation is already terminal they return `WaitResult::Terminal` immediately. Otherwise Runtime delegates to the invocation's stable EDP-Threading wait slot and then re-observes authoritative invocation state before interpreting the provider result. Consequently a terminal transition wins a simultaneous timeout/interruption race.

The public results are `Terminal`, `TimedOut`, `Interrupted`, and `InvalidHandle`. Repeated waits on the same retained terminal Handle remain valid. Same-Handle operations are caller-serialized.

## Execution and cancellation

The application-supplied Handler provider is retained by reference. `ExecuteOne` claims one queued invocation and execution-concurrency lease under the Command mutex, releases it, calls the Handler, then reacquires it to publish the terminal result. Multiple application-owned execution contexts may therefore execute Handlers concurrently up to `ExecutionConcurrency`; Runtime creates no worker task. Handlers receive the typed Request and an atomic read-only `CancellationToken`. Cancellation is cooperative while executing and immediate for queued work. Terminal wake publication happens after the Command mutex is released.

`RequestCancellation` returns `Requested`, `AlreadyRequested`, `TooLate`, or `InvalidHandle`. Cancellation never implies asynchronous thread interruption.

Runtime itself does not create threads. Execution scheduling/ownership belongs outside Command; in particular, a Mesh worker may validate/decode/commit inbound work but never calls a Command Handler.

## Typed operational results

Fallible operations use dedicated strongly typed results rather than Boolean success/failure. `Initialize` returns `InitializationResult`; `ExecuteOne` returns `ExecutionAttemptResult`; `BeginQuiesce` returns `QuiesceResult`; cancellation, waiting, response extraction and completion publication likewise have dedicated result vocabularies. Boolean values remain appropriate for genuine predicates such as `DispatchResult::Accepted()`, `TakeResponseResult::HasValue()`, `CancellationToken::IsRequested()` and terminal-outcome predicates.

`ExecutionResult<void>` remains a generic response-less utility specialization for source compatibility of that utility Type. `void` is not a valid `CommandType::Response`; semantic no-response Commands use `NoResponsePayload`.

## Initialization and shutdown

A Runtime starts `Uninitialized`; `Initialize` transitions once to `Running`. `BeginQuiesce` closes admission and transitions to `Quiescing` while retained/active records remain, then `Quiescent` after the last record can be reclaimed. No topology mutation or rebinding is introduced after initialization.

## Execution-domain Dispatch scope

Command exposes compile-time per-call policy names `LocalOnly`, `RemoteOnly`, and `LocalAndRemote` as exact re-exports of canonical `EDP-Primitives` execution-domain scope Types. Scope is Dispatch control metadata rather than Request/schema/wire state. Core destination-free `Runtime::Dispatch(request)` remains naturally LocalOnly and therefore acquires no routing or Transport responsibility.

Higher routing/integration code may use `DispatchScoped` after it has already selected applicable operations. LocalOnly returns the local operation result directly; RemoteOnly returns only the higher-layer remote result and performs no local Runtime admission. LocalAndRemote explicitly attempts local admission first and then performs the independent remote handoff regardless of the local result, without awaiting asynchronous Handler completion. It returns `LocalAndRemoteDispatchResult` containing separate `Local()` and `Remote()` outcomes. No aggregate success, transaction, rollback, fallback, suppression or quorum semantic is introduced.

The generic coordinator accepts non-throwing operations rather than duplicating a Request, so move-only Request Types remain supported. The typed integration overload consumes a `RemoteHandoffReservation`: a caller-owned immutable Request stays alive only through one bounded adapter call, which runs after the local attempt and outside Command synchronization. The full contract is documented in [Execution-Domain Dispatch Scope](./EXECUTION-DOMAIN-SCOPE.MD).

## Integration boundary

`InboundAdmission` is a typed facade over local Runtime dispatch. Its `Prepare()` operation reserves a generation-safe constructed-but-unpublished Request plus exact invocation/queue entitlement. Validation and population happen outside Command synchronization; `Commit()` alone publishes to the execution queue, while abort/destruction restores all capacity.

`OutboundHandoff` reserves a plan-bounded synchronized stage retaining only a caller-owned immutable Request borrow. Commit invokes one bounded adapter call after releasing the Command mutex; abort/destruction invokes nothing. `CommandInvocationCorrelation<TDeliveryIdentifier>` is the family-owned strong semantic value scoped by authenticated source Device/runtime plus the original Invocation delivery identifier. The adapter constructs it and binds separate Invocation, Cancellation and TerminalResult deliveries to it.

`RemoteCommandOperation<TCommand,TBinding>` is the move-only family semantic surface over adapter-owned bounded state. The binding supplies the frozen recipient/correlation set, per-recipient observation, finite waits, cooperative cancellation, terminal facts and at-most-once Response extraction; releasing the source object abandons observation only. Adapter storage continues to own routing, delivery and wire correlation.

`OutboundCompletion<TResponse>` remains an invocation-specific, exactly-once completion capability. Its first success/rejection/failure/cancellation publication consumes the capability. Publication returns `CompletionPublicationResult`: `Accepted`, `AlreadyCompleted`, or `Unavailable`. This operational result is distinct from the invocation's semantic terminal `Outcome`. `OutboundInvocation` packages a request view, atomic cancellation token and completion capability without introducing Transport ownership into Command.

Integration payload Types shown by maintained demonstrations are schema-bearing semantic Types even where a particular narrow completion helper does not itself need to enforce `CommandType`.

## Ownership and lifetime

`Handle`, inbound reservations and outbound handoff reservations are move-only. Their Runtime pointer/index/generation tuples are private implementation identities, not public InvocationIds. Non-wrapping generation checking prevents stale capabilities from observing reused slots. `TakeResponseResult` owns any extracted response in embedded storage and destroys it unless the caller takes it.

## Dependencies

EDP-System owns TypeIdentifier/FieldIdentifier/FieldBinding/FieldSet/SchemaType, Device/runtime identity and the compile-time Composition/Architecture framework. EDP-Primitives supplies Command's Primitive-family classification and canonical execution-domain scope. EDP-Memory is mandatory for typed lifetime and ownership-transfer operations. EDP-Clock supplies `Duration` and `MonotonicTimestamp`. EDP-Threading supplies external bounded finite wait/wake and ordinary-mutex capabilities. EDP-Platform and EDP-BoundedTopology are inherited through the Threading dependency chain.

Command does not acquire Transport, Security, codec, parser, or wire-format responsibilities from this dependency.

## Validation

`tests/run_tests.py` covers host lifecycle/wait/cancellation contracts, Handler/wait/wake/adapter-outside-lock behavior, unpublished ingress commit/abort, bounded handoff stages, local-first coordination, semantic correlation, remote-operation move/release/extraction behavior, compile-time rejection contracts and deterministic Runtime-size measurements. Branch validation executes both g++ and clang++, ASan/UBSan, and maintained PlatformIO Arduino/ESP-IDF demo builds.

`demos/CommandLifecycle` and `demos/FakeOutboundIntegration` use explicit schema-bearing semantic Types in Arduino IDE, PlatformIO Arduino and PlatformIO ESP-IDF forms. `examples/HelloCommand` provides the consumer-oriented schema-bearing Bootstrap example described by README.
