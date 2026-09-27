# EDP-Command

Deterministic, bounded Command dispatch and execution for the ESPressio Development Platform.

EDP-Command lets an application submit typed work to a compile-time-bound handler, observe/cancel it through a move-only Handle, wait for terminal state with a finite timeout, and take a typed response. Runtime topology and capacity are fixed before initialization; there is no runtime service registry or Command-runtime heap allocation.

## Build a minimal Command application

A local Command needs a Request/Response pair, a Command type, one persistent handler, an EDP-Threading bounded wait provider, and an application Architecture tying those domains together.

### 1. Declare the Command

```cpp
struct AddRequest { int Left; int Right; };
struct AddResponse { int Value; };
struct Add final { using Request = AddRequest; using Response = AddResponse; };
```

### 2. Declare its handler

```cpp
namespace Command = ESPressio::Command;
namespace Composition = ESPressio::System::CompositionFramework;

class AddHandler final : public Composition::Provider<
    Command::Composition::Domain,
    Composition::Offers<Composition::Offer<Command::Composition::Handler<Add>>>
> {
public:
    Command::ExecutionResult<AddResponse> Execute(
        const AddRequest& request,
        Command::CancellationToken cancellation
    ) noexcept {
        if (cancellation.IsRequested()) return Command::ExecutionResult<AddResponse>::Failed();
        return Command::ExecutionResult<AddResponse>::Succeeded(
            AddResponse{request.Left + request.Right}
        );
    }
};
```

`Handler<Add>` is exclusive: the Architecture must resolve exactly one provider for this Command type.

### 3. Provide bounded waiting

On ESP32/FreeRTOS a four-slot provider can be declared as:

```cpp
using WaitProvider = ESPressio::Threading::BoundedWaitWakeProvider<
    ESPressio::Platform::FreeRTOS::Synchronization::SignalProvider,
    4U
>;
```

The wait-provider capacity must cover every Command invocation record. Command borrows this application-owned Threading provider; it does not own native FreeRTOS/Platform synchronization itself.

### 4. Build the immutable application Architecture

```cpp
using ApplicationCommands = Composition::Composition<
    Command::Composition::Domain,
    AddHandler
>;
using ApplicationThreading = Composition::Composition<
    ESPressio::Threading::Domain,
    WaitProvider
>;
using ApplicationArchitecture = Composition::Architecture<
    ApplicationCommands,
    ApplicationThreading
>;

using AddBootstrap = Command::Bootstrap<
    Add,
    ApplicationArchitecture,
    Command::ResourcePlan<4, 3, 1>
>;
```

`ResourcePlan<4, 3, 1>` reserves four invocation records, permits three queued invocations and one executing invocation. Bootstrap statically rejects a wait provider whose capacity is smaller than the invocation capacity.

### 5. Construct persistent providers and Bootstrap

```cpp
AddHandler addHandler;
WaitProvider waits;
AddBootstrap addCommand(addHandler, waits);

if (!addCommand.Initialize()) return 1;
```

These provider objects must outlive the Command Bootstrap. Topology is immutable after initialization.

### 6. Dispatch, execute, wait and take the response

```cpp
auto& runtime = addCommand.RuntimeInstance();
auto dispatch = runtime.Dispatch(AddRequest{20, 22});
if (!dispatch.Accepted()) return 2;

auto handle = dispatch.TakeHandle();
if (!runtime.ExecuteOne()) return 3;

if (handle.WaitFor(Command::Duration{}) != Command::WaitResult::Terminal) return 4;

auto response = handle.TakeResponse();
if (response.HasValue()) {
    std::printf("result=%d\n", response.Take().Value);
}
```

`WaitFor` and `WaitUntil` are finite and non-consuming. Repeated waits on a terminal Handle return `Terminal`; provider interruption is reported as `Interrupted`; and authoritative terminal state wins a simultaneous timeout/wake race.

The complete source is [`examples/HelloCommand/main.cpp`](./examples/HelloCommand/main.cpp).

## Lifecycle and cancellation

Dispatch either fails without creating an invocation or atomically admits one and returns its exclusive move-only Handle. Accepted invocations move through `Queued`, `Executing`, `Completed` or `Cancelled`. `Completed` has the orthogonal disposition `Succeeded`, `Rejected` or `Failed`.

Cancellation request outcomes are `Requested`, `AlreadyRequested`, `TooLate`, and `InvalidHandle`. Cancellation is cooperative once execution has begun. Releasing/destroying a Handle never implicitly cancels work.

## Admission failures

`NoCapacity`, `TypeUnrecognised`, `RuntimeUnavailable`, `BindingUnavailable`.

## Integration boundary

EDP-Command does not depend on Transport, Serialisation or Security. Inbound adapters use `InboundAdmission`; outbound adapters receive narrow invocation/completion capabilities and retain their own correlation identity.

## Dependencies

```text
EDP-Command
  +-- EDP-System     Composition, Architecture and provider resolution
  +-- EDP-Memory     typed object lifetime and ownership transfer
  +-- EDP-Threading  bounded finite wait/wake capability
  `-- EDP-Clock      Duration / MonotonicTimestamp vocabulary
```

See [DEPENDENCY_GRAPH.MD](./DEPENDENCY_GRAPH.MD).

## Documentation

- [Command Architecture Contract](./docs/COMMAND-ARCHITECTURE.MD)
- [`wiki/`](./wiki/Home.md)

## Repository acceptance

`tests/run_tests.py` builds the host lifecycle/Architecture contract and deterministic resource-measurement suites. `demos/CommandLifecycle` exercises Composition/Architecture/Bootstrap, execution, finite terminal waiting, response extraction and quiescence. `demos/FakeOutboundIntegration` exercises exactly-once integration completion without inventing a Transport architecture.
