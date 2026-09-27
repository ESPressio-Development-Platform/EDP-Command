# Architecture

## Composition and Bootstrap

A Command type exposes an exclusive `Handler<TCommand>` capability in the Command domain. A valid EDP-System Architecture supplies exactly one such Handler and exactly one external `EDP-Threading::BoundedWaitWake` provider. `Bootstrap<TCommand, TArchitecture, TPlan>` resolves and borrows those application-owned providers and establishes Runtime wiring. Wait-provider capacity must cover every planned invocation slot. The topology is immutable after initialization.

## Lifecycle

Dispatch is immediate and transactional. Before Handle publication the runtime reserves bounded admission resources and establishes the Request. Failure before commitment leaves no observable invocation.

Accepted lifecycle: `Queued -> Executing -> Completed`, with cancellation able to terminate as `Cancelled`. There is no `Failed` lifecycle state. Completed carries `Succeeded`, `Rejected`, or `Failed`; failed completion carries only `ExecutorFailure` or `IntegrationFailure` in the Command domain.

## Handle, waiting and ownership

The Handle is exclusive and move-only. It is the local observation authority. No public InvocationId exists in v1. Retaining a Handle retains terminal record ownership; release permits reclamation.

`WaitFor` and `WaitUntil` are finite and non-consuming. Each invocation record maps to the same stable index in the external bounded Threading wait/wake provider. Runtime re-observes authoritative state after every provider return, so terminal state wins a simultaneous timeout/interruption race. Repeated waits on a retained terminal Handle return `Terminal`. Same-Handle operations are caller-serialized.

## Memory model

Invocation records and queues are fixed-capacity. Requests and Responses are established/destroyed through EDP-Memory typed lifetime operations. Successful/rejected non-void Commands own exactly one typed Response until extraction or record reclamation. Command does not own the Threading synchronization objects; Runtime retains only a borrowed provider pointer.

## Cancellation

Cancellation is cooperative for executing work and immediate for queued work. Terminal completion/cancellation wakes the invocation's Threading wait slot. `RequestCancellation` reports `Requested`, `AlreadyRequested`, `TooLate`, or `InvalidHandle`. Transport failure is never represented as cancellation.

## Shutdown

`BeginQuiesce` stops new admission. Existing invocations remain observable. Complete reclamation may be delayed by retained Handles.

## Integration

Inbound adapters use the same local bounded admission semantics. Outbound adapters receive only Request/cancellation/completion capabilities. Detailed Transport/codec/security failures remain owned by those domains.

## Determinism

Capacity is compile-time planned. No admission wait exists. Back-pressure/retry belongs above Command. Terminal publication is one-way and duplicate integration completion is rejected. The measured Command Runtime cost of adding external finite-wait support is one pointer (+8 bytes on the accepted host measurements), independent of invocation capacity; external Threading storage is separately owned and planned.
