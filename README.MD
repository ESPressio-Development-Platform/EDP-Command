# EDP-Command

Deterministic, bounded Command dispatch and execution for the ESPressio Development Platform.

EDP-Command gives an application a fixed-capacity way to submit typed work, execute it through a compile-time-bound handler, observe its lifecycle, cancel cooperatively and take a typed response. It performs no heap allocation in the Command runtime and does not hide provider discovery or mutable service registration behind runtime APIs.

## A minimal Command application

A locally executable Command has four pieces: a Request/Response type pair, the Command type itself, a persistent handler provider, and an application Bootstrap binding.

### 1. Declare the Command

```cpp
struct AddRequest { int Left; int Right; };
struct AddResponse { int Value; };

struct Add final {
    using Request = AddRequest;
    using Response = AddResponse;
};
```

### 2. Provide its handler through Composition

```cpp
namespace Command = ESPressio::Command;
namespace Composition = ESPressio::System::CompositionFramework;

class AddHandler final : public Composition::Provider<
    Command::Composition::Domain,
    Composition::Offers<
        Composition::Offer<Command::Composition::Handler<Add>>
    >
> {
public:
    Command::ExecutionResult<AddResponse> Execute(
        const AddRequest& request,
        Command::CancellationToken cancellation
    ) noexcept {
        if (cancellation.IsRequested()) {
            return Command::ExecutionResult<AddResponse>::Failed();
        }

        return Command::ExecutionResult<AddResponse>::Succeeded(
            AddResponse{request.Left + request.Right}
        );
    }
};
```

`Handler<Add>` is an exclusive Command-domain capability. The Command Composition therefore admits exactly one handler provider for this Command type; ambiguity is rejected at compile time rather than resolved dynamically.

### 3. Fix the application topology and resource plan

```cpp
using ApplicationCommands = Composition::Composition<
    Command::Composition::Domain,
    AddHandler
>;

using AddBootstrap = Command::Bootstrap<
    Add,
    ApplicationCommands,
    Command::ResourcePlan<4, 3, 1>
>;
```

The application topology is compile-time fixed. `ResourcePlan<4, 3, 1>` reserves four invocation records, permits three queued invocations and one executing invocation.

### 4. Bootstrap, initialize and dispatch

```cpp
AddHandler addHandler;          // persistent application-owned provider
AddBootstrap addCommand(addHandler);

if (!addCommand.Initialize()) {
    return 1;
}

auto& runtime = addCommand.RuntimeInstance();
auto dispatch = runtime.Dispatch(AddRequest{20, 22});
if (!dispatch.Accepted()) {
    return 2;
}

auto handle = dispatch.TakeHandle();
runtime.ExecuteOne();

auto response = handle.TakeResponse();
if (response.HasValue()) {
    std::printf("result=%d\n", response.Take().Value);
}
```

The complete buildable source for this walkthrough is [`examples/HelloCommand/main.cpp`](./examples/HelloCommand/main.cpp).

## Lifecycle contract

A Dispatch either fails immediately without creating an invocation, or atomically admits one invocation and returns its exclusive move-only Handle. Accepted invocations move through `Queued`, `Executing`, `Completed` or `Cancelled`. `Completed` is orthogonal to `Succeeded`, `Rejected` and `Failed`.

A retained Handle keeps its terminal invocation record available for observation and response extraction. Releasing/destroying the Handle never implicitly cancels work.

## Admission failures

`NoCapacity`, `TypeUnrecognised`, `RuntimeUnavailable`, `BindingUnavailable`.

## Integration boundary

EDP-Command does not depend on Transport, Serialisation or Security. Inbound adapters use `InboundAdmission`; outbound adapters receive narrow invocation/completion capabilities and retain their own correlation identity.

## Dependencies

```text
EDP-Command
  +-- EDP-System   compile-time Composition and handler-provider resolution
  +-- EDP-Memory   typed object lifetime and ownership transfer
  `-- EDP-Clock    canonical Duration / MonotonicTimestamp vocabulary
```

These are mandatory production dependencies in the current reconciliation state. EDP-Threading bounded waiting is the next active reconciliation slice and is not documented as complete until Command consumes it in production code. See [DEPENDENCY_GRAPH.MD](./DEPENDENCY_GRAPH.MD).

## Documentation

- [Command Architecture Contract](./docs/COMMAND-ARCHITECTURE.MD) — durable lifecycle, ownership, deterministic resource, execution, cancellation and integration contracts.
- [`wiki/`](./wiki/Home.md) — comprehensive internal developer/reference documentation.

## Repository acceptance

`tests/run_tests.py` builds the host lifecycle/Composition contract and deterministic resource-measurement suites. `demos/CommandLifecycle` demonstrates the same Composition-backed Bootstrap path plus admission, execution, response extraction and quiescence. `demos/FakeOutboundIntegration` demonstrates the exactly-once integration completion boundary without inventing a Transport architecture.
