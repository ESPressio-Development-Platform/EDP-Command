# EDP-Command Developer Wiki

EDP-Command owns deterministic bounded Command admission, execution, lifecycle, cancellation, response extraction and narrow integration capabilities. It does **not** own Transport, Serialisation, Security, remote correlation or scheduling.

The consumer entry point is `src/ESPressio_Command.hpp`. EDP-Memory supplies lifetime/transfer operations and EDP-Clock supplies time vocabulary.

Start with [Architecture](Architecture), then [Dependency Contracts](Dependency-Contracts), [Public API](Public-API), [Internal API](Internal-API), [Private Implementation](Private-Implementation), [Resources Lifecycle and Concurrency](Resources-Lifecycle-and-Concurrency), and the exhaustive [Reference Index](Reference-Index). Build/test information is in [Testing and Resources](Testing-and-Resources); build definitions are in [Compiler Definitions](Compiler-Definitions). This repository has no substantive maintained code-generation/CLI tooling, as recorded in [Tooling Reference](Tooling-Reference).