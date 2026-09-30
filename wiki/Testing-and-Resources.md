# Testing and Resources

The v1 acceptance gate is intentionally broader than compilation.

Host coverage verifies admission capacity, lifecycle transitions, cancellation, finite waiting, Handle move/release/retention, exactly-once Response extraction, shutdown/quiescence, terminal Outcome observation, Handler success/rejection/failure, LocalOnly/RemoteOnly/LocalAndRemote scope semantics and fake outbound completion.

The Serialisation prerequisite contract adds compile-time proof that:

- a Command operation must itself be a schema-bearing Primitive in `Command::Family`;
- Request and Response Types are schema-bearing;
- an anonymous Request/Response-only Command is rejected;
- a Command carrying a non-schema Request is rejected;
- missing/duplicate Handler tests remain schema-valid first, so they continue failing for the Handler contract they are intended to verify rather than being masked by the schema gate.

`tests/run_tests.py` consumes sibling EDP repositories from the coherent local checkout. `CXX` selects the host compiler and optional `CXXFLAGS` are appended to every host compile so the same runner can exercise ASan/UBSan. It also builds and executes `tests/host/resource_measurement.cpp`, which reports deterministic `sizeof(Runtime)` measurements for bounded resource plans.

The `serialisation_prerequisites` GitHub Actions path deliberately checks out `EDP-Primitives/serialisation_prerequisites`; `main` continues to validate against `EDP-Primitives/main`. This keeps the cross-repository contract migration coherent without weakening either branch.

Target demos exist in Policy/V2 required Arduino IDE, PlatformIO Arduino-ESP32 and PlatformIO ESP-IDF forms. `CommandLifecycle` demonstrates schema-bearing Command/Request/Response declarations plus the maintained runtime lifecycle. `FakeOutboundIntegration` uses a schema-bearing response payload. `ExecutionDomainScope` remains a scope-only demonstration and therefore requires no artificial Command payload declaration.

`.github/workflows/validation.yml` runs GCC and Clang host suites, ASan/UBSan coverage, and PlatformIO Arduino/ESP-IDF builds for all maintained PlatformIO demonstrations. The Stage-A implementation task is tracked as EDP-Command issue #3.

RPI400 remains the authoritative executable-validation appliance under current Policy, but it is intentionally **not used by this tranche while the Remote System implementation owns it**. GitHub Actions/host/PlatformIO success is therefore a strong intermediate gate, not a claim that the deferred RPI400 acceptance gate has been completed.
