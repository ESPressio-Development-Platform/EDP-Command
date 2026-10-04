# ExecutionDomainScope demo

This demo exercises the locked F4 per-call Command execution-domain policy without introducing a concrete Transport or routing implementation.

The local and remote operations are deliberately small stand-ins for an ordinary local Runtime Dispatch and a higher routing/integration operation. The `LocalAndRemote` example returns a negative local result and a successful remote result while proving that local admission is attempted first and both operations run exactly once. Local failure does not suppress valid remote participation, and no aggregate success/rollback/fallback or Handler-completion ordering contract is created.

Maintained forms:

- `Arduino_IDE/ExecutionDomainScope.ino`
- `PlatformIO_Arduino`
- `PlatformIO_ESP-IDF`

The demo intentionally does not model destination selection, Transport, Security or Serialisation because those remain outside EDP-Command.
