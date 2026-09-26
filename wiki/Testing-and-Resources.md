# Testing and Resources

The v1 acceptance gate is intentionally broader than compilation.

Host coverage must progressively verify admission capacity, FIFO ordering, execution concurrency, lifecycle transitions, cancellation interleavings, Handle move/release/retention, exactly-once Response extraction, shutdown/quiescence, terminal publication, executor success/rejection/failure and fake outbound completion.

Negative compile tests must cover invalid resource plans, Handle copying and invalid Command/executor contracts. Race coverage should prefer controlled interleavings over sleep-based tests.

`ResourcePlan` exposes invocation and queue byte accounting. Representative plans should be measured so capacity/concurrency changes produce explicit bounded deltas.

The initial test execution is intentionally deferred until the implementation tranches are complete; failures discovered by that progressive run are to be corrected against the locked H1-H15 architecture rather than by silently changing the contract.
