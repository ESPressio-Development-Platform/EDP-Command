# Testing and Resources

The v1 acceptance gate is intentionally broader than compilation.

Host coverage progressively verifies admission capacity, FIFO ordering, lifecycle transitions, cancellation, Handle move/release/retention, exactly-once Response extraction, shutdown/quiescence, terminal publication, executor success/rejection/failure and fake outbound completion. Compile-time invalid `ResourcePlan` configurations are rejected by static assertions.

`tests/run_tests.py` consumes sibling EDP repositories from the coherent local checkout. In addition to the lifecycle contract suite, it builds and executes `tests/host/resource_measurement.cpp`, which reports deterministic `sizeof(Runtime)` measurements for minimum, small, representative and high bounded resource plans. This makes the memory delta associated with invocation/queue/concurrency choices visible and repeatable without dynamic allocation.

Target demos exist for each logical demonstration in all Policy/V2 required forms: Arduino IDE, PlatformIO Arduino-ESP32 and PlatformIO ESP-IDF. The PlatformIO projects use the PIOArduino stable platform and sibling checkout dependencies so workstream branches can be tested coherently.

The progressive acceptance sequence is Memory, Threading, Command host/resource, then Command target-demo builds. Failures are corrected against the locked H1-H15 architecture rather than by silently changing the contract.
