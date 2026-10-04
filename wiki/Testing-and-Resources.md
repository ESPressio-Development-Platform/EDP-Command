# Testing and Resources

The v1 acceptance gate is intentionally broader than compilation.

Host coverage verifies admission capacity, lifecycle transitions, atomic cancellation, finite waiting, Handle move/release/retention, exactly-once Response extraction, shutdown/quiescence, terminal Outcome observation, Handler success/rejection/failure, application-mutex balance, Handler/wait/wake/adapter calls outside that mutex, unpublished ingress commit/abort, bounded outbound stages, local-first scope semantics, semantic correlation, move-only remote operations and fake outbound completion.

The Stage-C serialisability contract adds compile-time proof that:

- a Command operation must itself be a schema-bearing Primitive in `Command::Family`;
- Request and Response Types are schema-bearing and serialisable;
- an anonymous Request/Response-only Command is rejected;
- a Command carrying a non-schema Request is rejected;
- missing/duplicate Handler tests remain schema-valid first, so they continue failing for the Handler contract they are intended to verify rather than being masked by the schema gate.

`tests/run_tests.py` consumes sibling EDP repositories from the coherent local checkout. `CXX` selects the host compiler and optional `CXXFLAGS` are appended to every host compile so the same runner can exercise ASan/UBSan. It also builds and executes `tests/host/resource_measurement.cpp`, which reports deterministic `sizeof(Runtime)` measurements for bounded resource plans.

The Mesh V1 Command branch consumes the frozen public dependency tips recorded by the Policy handoff; no dependency repository change is required for this seam.

Target demos exist in the required Arduino IDE, PlatformIO Arduino-ESP32 and PlatformIO ESP-IDF forms. `CommandLifecycle` demonstrates schema-bearing Command/Request/Response declarations, application-owned Command mutex wiring and the maintained runtime lifecycle. `FakeOutboundIntegration` uses a schema-bearing response payload. `ExecutionDomainScope` demonstrates independent result shape plus explicit local-attempt-before-remote order.

`.github/workflows/validation.yml` runs GCC and Clang host suites, ASan/UBSan coverage, and PlatformIO Arduino/ESP-IDF builds for all maintained PlatformIO demonstrations. The Mesh V1 family-seam task is tracked as EDP-Command issue #5 and Policy issue #23.

Authoritative checkpoint evidence records the exact AI-AGENT-02 and independent RPI400 refs/commands/results in the living Policy handoff; this page does not substitute a stale appliance claim for that evidence.
