# EDP-Command

Deterministic, bounded Command dispatch and execution for the ESPressio Development Platform.

## v1 contract

A Dispatch either fails immediately without creating an invocation, or atomically admits one invocation and returns its exclusive move-only Handle. Accepted invocations move through `Queued`, `Executing`, `Completed` or `Cancelled`. `Completed` is orthogonal to `Succeeded`, `Rejected` and `Failed`.

The runtime is statically capacity planned. It performs no heap allocation and uses EDP-Memory for typed object lifetime/ownership transfer. Application executors are non-throwing and receive a read-only cancellation token.

## Admission failures

`NoCapacity`, `TypeUnrecognised`, `RuntimeUnavailable`, `BindingUnavailable`.

## Integration boundary

EDP-Command does not depend on Transport, Serialisation or Security. Inbound adapters use `InboundAdmission`; outbound adapters receive narrow invocation/completion capabilities and retain their own correlation identity.

## Repository acceptance

`tests/run_tests.py` builds the host lifecycle contract suite. `demos/CommandLifecycle` demonstrates admission, execution, response extraction and quiescence. `demos/FakeOutboundIntegration` demonstrates the exactly-once integration completion boundary without inventing a Transport architecture.

See `wiki/` for the architecture narrative and `DEPENDENCY_GRAPH.MD` for dependency ownership.
