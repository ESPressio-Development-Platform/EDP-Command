# EDP-Command Developer Wiki

EDP-Command owns deterministic bounded Command admission, unpublished ingress staging, synchronized outbound handoff staging, execution, lifecycle, cancellation, response extraction, semantic invocation correlation and the move-only remote-operation surface. It also exposes `LocalOnly`, `RemoteOnly`, and `LocalAndRemote` policy with explicit local-admission-before-remote coordination. It does **not** own Transport, routing, delivery identity, Security, codec execution or scheduling.

The consumer entry point is `src/ESPressio_Command.hpp`. EDP-Memory supplies lifetime/transfer operations and EDP-Clock supplies time vocabulary.

Start with [Architecture](Architecture), then [Dependency Contracts](Dependency-Contracts), [Public API](Public-API), [Internal API](Internal-API), [Private Implementation](Private-Implementation), [Resources Lifecycle and Concurrency](Resources-Lifecycle-and-Concurrency), and the exhaustive [Reference Index](Reference-Index). Build/test information is in [Testing and Resources](Testing-and-Resources); build definitions are in [Compiler Definitions](Compiler-Definitions). This repository has no substantive maintained code-generation/CLI tooling, as recorded in [Tooling Reference](Tooling-Reference).
