# Changelog

## 0.1.0 — unreleased

- Establish bounded compile-time `ResourcePlan`.
- Add transactional local Dispatch and exclusive move-only Handle ownership.
- Add queued/executing/completed/cancelled lifecycle with separate terminal `Outcome { Succeeded, Rejected, Failed, Cancelled }`.
- Add terminal outcome predicates `DidSucceed`, `WasRejected`, `DidFail`, and `WasCancelled`, with `ExecutionFailure` retained as orthogonal failure-cause detail.
- Add cooperative cancellation, typed Response extraction and quiescing shutdown.
- Add strongly typed operational results for initialization, execution attempts, quiescing, waiting, cancellation, response extraction and completion publication; Boolean APIs remain predicates.
- Add finite non-consuming `Handle::WaitFor` / `WaitUntil` backed by an externally-owned EDP-Threading bounded wait/wake provider.
- Add compile-time Command Composition contracts: exactly one Handler per Command and exactly one external bounded Threading wait provider.
- Add an application-supplied Command-identity ordinary mutex and serialize every short Runtime, Handle, cancellation, ingress and handoff-stage transition while keeping Handler execution, waits, wakes and adapter calls outside the mutex.
- Make Handler cancellation observation atomic and execution claim/execute/complete safe for application-owned concurrent execution contexts up to `ExecutionConcurrency`; Command still creates no worker task.
- Add constructed-but-unpublished inbound Request reservation/commit/abort with exact invocation and queue entitlement.
- Add plan-bounded synchronized outbound Request handoff staging, explicit local-admission-before-remote coordination, strong `CommandInvocationCorrelation`, and move-only `RemoteCommandOperation` binding over frozen recipient semantics.
- Add application `Bootstrap` wiring over EDP-System Architecture with immutable post-initialization topology and compile-time wait-capacity validation.
- Add inbound admission and exactly-once outbound completion capability contracts, with typed `CompletionPublicationResult` distinct from semantic terminal `Outcome`.
- Add EDP-Memory-based typed lifetime/ownership-transfer integration.
- Restore canonical `Command::Family` Primitive-family identity and require every `CommandType` operation to be a schema-bearing Command Primitive.
- Require every Command Request and Response to satisfy `System::SchemaType` and restore distinct schema identities for operation, Request and Response.
- Add canonical identified zero-field `NoRequestPayload` and `NoResponsePayload` schema Types; `void` is not a semantic Command payload contract.
- Add compile-fail coverage for anonymous Commands and non-schema Request Types.
- Migrate maintained tests, examples and demonstrations to explicit TypeIdentifier/FieldSet/FieldBinding schema metadata without changing Runtime lifecycle semantics.
- Add host lifecycle/wait/cancellation tests, real-thread concurrent execution coverage, missing/duplicate Handler and non-throwing scope-result compile-fail contracts, and deterministic Runtime resource measurements.
- Add CommandLifecycle and FakeOutboundIntegration demonstrations in Arduino IDE, PlatformIO Arduino and PlatformIO ESP-IDF forms.
- Add `examples/HelloCommand` and a consumer-oriented README walkthrough covering schema declarations, Composition, Bootstrap, dispatch and Handle use.
- Add durable Command architecture documentation and comprehensive repository-local developer Wiki/reference baseline.

The library remains `0.1.0` throughout the Serialisation prerequisite and Mesh V1 family-seam work by explicit project direction.
