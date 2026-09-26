# Architecture

## Lifecycle

Dispatch is immediate and transactional. Before Handle publication the runtime reserves bounded admission resources and establishes the Request. Failure before commitment leaves no observable invocation.

Accepted lifecycle: `Queued -> Executing -> Completed`, with cancellation able to terminate as `Cancelled`. There is no `Failed` lifecycle state. Completed carries `Succeeded`, `Rejected`, or `Failed`; failed completion carries only `ExecutorFailure` or `IntegrationFailure` in the Command domain.

## Handle and ownership

The Handle is exclusive and move-only. It is the local observation authority. No public InvocationId exists in v1. Retaining a Handle retains terminal record ownership; release permits reclamation.

## Memory model

Invocation records and queues are fixed-capacity. Requests and Responses are established/destroyed through EDP-Memory typed lifetime operations. Successful/rejected non-void Commands own exactly one typed Response until extraction or record reclamation.

## Cancellation

Cancellation is cooperative for executing work and immediate for queued work. Transport failure is never represented as cancellation.

## Shutdown

`BeginQuiesce` stops new admission. Existing invocations remain observable. Complete reclamation may be delayed by retained Handles.

## Integration

Inbound adapters use the same local bounded admission semantics. Outbound adapters receive only Request/cancellation/completion capabilities. Detailed Transport/codec/security failures remain owned by those domains.

## Determinism

Capacity is compile-time planned. No admission wait exists. Back-pressure/retry belongs above Command. Terminal publication is one-way and duplicate integration completion is rejected.
