# Changelog

## 0.1.0 — unreleased

- Establish bounded compile-time `ResourcePlan`.
- Add transactional local Dispatch and exclusive move-only Handle ownership.
- Add queued/executing/completed/cancelled lifecycle with orthogonal completion disposition.
- Add cooperative cancellation, typed Response extraction and quiescing shutdown.
- Add finite non-consuming `Handle::WaitFor` / `WaitUntil` backed by an externally-owned EDP-Threading bounded wait/wake provider.
- Add compile-time Command Composition contracts: exactly one Handler per Command and exactly one external bounded Threading wait provider.
- Add application `Bootstrap` wiring over EDP-System Architecture with immutable post-initialization topology and compile-time wait-capacity validation.
- Add inbound admission and exactly-once outbound completion capability contracts.
- Add EDP-Memory-based typed lifetime/ownership-transfer integration.
- Add host lifecycle/wait/cancellation tests, missing/duplicate Handler compile-fail contracts, and deterministic Runtime resource measurements.
- Add CommandLifecycle and FakeOutboundIntegration demonstrations in Arduino IDE, PlatformIO Arduino and PlatformIO ESP-IDF forms.
- Add `examples/HelloCommand` and a consumer-oriented README walkthrough covering declarations, Composition, Bootstrap, dispatch and Handle use.
- Add durable Command architecture documentation and comprehensive repository-local developer Wiki/reference baseline.
