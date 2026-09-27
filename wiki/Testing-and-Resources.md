# Testing and Resources

The v1 acceptance gate is intentionally broader than compilation.

Host coverage progressively verifies admission capacity, lifecycle transitions, cancellation, finite waiting, Handle move/release/retention, exactly-once Response extraction, shutdown/quiescence, terminal Outcome observation, Handler success/rejection/failure and fake outbound completion. Compile-time invalid `ResourcePlan` configurations are rejected by static assertions; missing and duplicate Handler providers are also compile-time rejection cases.

`tests/run_tests.py` consumes sibling EDP repositories from the coherent local checkout. In addition to the lifecycle contract suite, it builds and executes `tests/host/resource_measurement.cpp`, which reports deterministic `sizeof(Runtime)` measurements for minimum, small, representative and high bounded resource plans. This makes the memory delta associated with invocation/queue/concurrency choices visible and repeatable without dynamic allocation.

The first post-H16 dependency-ordered host regression on 2026-09-27 passed EDP-Memory, EDP-Threading and EDP-Command. EDP-Command reported Runtime measurements of 112, 176, 360 and 664 bytes for the minimum, small, representative and high fixtures respectively, and confirmed missing/duplicate Handler rejection at compile time.

Target demos exist for each logical demonstration in all Policy/V2 required forms: Arduino IDE, PlatformIO Arduino-ESP32 and PlatformIO ESP-IDF. The PlatformIO projects use the PIOArduino stable platform and sibling checkout dependencies so workstream branches can be tested coherently.

The progressive acceptance sequence is Memory, Threading, Command host/resource, then Command target-demo builds. H1-H16 are locked architecture; failures are corrected against those decisions rather than by silently changing the contract. A fresh embedded target gate remains required after the final V2/static reconciliation.