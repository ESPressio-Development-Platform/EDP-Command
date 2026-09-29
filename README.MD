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
        if (cancellation.IsRequested()) {
            return Command::ExecutionResult<AddResponse>::Failed();
        }

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
    Command::ResourcePlan<4U, 3U, 1U>
>;
```

`ResourcePlan<4U, 3U, 1U>` reserves four invocation records, permits three queued invocations and one executing invocation. Bootstrap statically rejects a wait provider whose capacity is smaller than the invocation capacity.

### 5. Construct persistent providers and Bootstrap

```cpp
AddHandler addHandler;
WaitProvider waits;
AddBootstrap addCommand(addHandler, waits);

if (addCommand.Initialize() != Command::InitializationResult::Initialized) {
    return 1;
}
```

These provider objects must outlive the Command Bootstrap. Topology is immutable after initialization. Initialization is an operation, so its result is strongly typed rather than Boolean.

### 6. Dispatch, execute, wait and take the response

```cpp
auto& runtime = addCommand.RuntimeInstance();
auto dispatch = runtime.Dispatch(AddRequest{20, 22});
if (!dispatch.Accepted()) {
    return 2;
}

auto handle = dispatch.TakeHandle();
if (runtime.ExecuteOne() != Command::ExecutionAttemptResult::Executed) {
    return 3;
}

if (handle.WaitFor(Command::Duration{}) != Command::WaitResult::Terminal) {
    return 4;
}

auto response = handle.TakeResponse();
if (response.HasValue()) {
    std::printf("result=%d\n", response.Take().Value);
}
```

`Accepted()` and `HasValue()` are predicates. Operations that can fail or produce multiple operational outcomes use dedicated result types. `WaitFor` and `WaitUntil` are finite and non-consuming. Repeated waits on a terminal Handle return `Terminal`; provider interruption is reported as `Interrupted`; and authoritative terminal state wins a simultaneous timeout/wake race.

The complete source is [`examples/HelloCommand/main.cpp`](./examples/HelloCommand/main.cpp).

## Lifecycle, outcome and cancellation

Dispatch either fails without creating an invocation or atomically admits one and returns its exclusive move-only Handle. Accepted invocations move through `Queued`, `Executing`, `Completed` or `Cancelled`.

Lifecycle state and terminal outcome are separate dimensions. A terminal invocation exposes one `Outcome`: `Succeeded`, `Rejected`, `Failed`, or `Cancelled`. Cancellation therefore remains the distinct lifecycle state `InvocationState::Cancelled` while also being observable as `Outcome::Cancelled`. `InvocationObservation` provides the genuine predicates `DidSucceed()`, `WasRejected()`, `DidFail()`, and `WasCancelled()`. `ExecutionFailure` remains orthogonal detail describing why an `Outcome::Failed` invocation failed.

Cancellation request outcomes are `Requested`, `AlreadyRequested`, `TooLate`, and `InvalidHandle`. Cancellation is cooperative once execution has begun. Releasing/destroying a Handle never implicitly cancels work.

## Admission failures

`NoCapacity`, `TypeUnrecognised`, `RuntimeUnavailable`, `BindingUnavailable`.

## Per-call execution-domain scope

The locked F4 integration policy is available through the compile-time `LocalOnly`, `RemoteOnly`, and `LocalAndRemote` scope Types. Ordinary `Runtime::Dispatch(request)` remains naturally local-only. Higher routing/integration code may use `DispatchScoped` to coordinate already-selected local and remote operations without giving Command responsibility for provider selection, destinations or Transport.

`LocalOnly` and `RemoteOnly` preserve the scalar result returned by the selected domain operation. `LocalAndRemote` returns structurally separate `Local()` and `Remote()` outcomes with no aggregate success, rollback, fallback, suppression or cross-domain ordering semantic.

See [Execution-Domain Dispatch Scope](./docs/EXECUTION-DOMAIN-SCOPE.MD) for the complete contract and the [`ExecutionDomainScope` demo](./demos/ExecutionDomainScope/README.MD).

## Integration boundary

EDP-Command does not depend on Transport, Serialisation or Security. Inbound adapters use `InboundAdmission`; outbound adapters receive narrow invocation/completion capabilities and retain their own correlation identity. Completion publication is a fallible operation and returns `CompletionPublicationResult` (`Accepted`, `AlreadyCompleted`, or `Unavailable`) rather than a Boolean.

## Dependencies

```text
EDP-Command
  +-- EDP-System      Composition, Architecture and provider resolution
  +-- EDP-Primitives  canonical execution-domain scope vocabulary
  +-- EDP-Memory      typed object lifetime and ownership transfer
  +-- EDP-Threading   bounded finite wait/wake capability
  `-- EDP-Clock       Duration / MonotonicTimestamp vocabulary
```

See [DEPENDENCY_GRAPH.MD](./DEPENDENCY_GRAPH.MD).

## Documentation

- [Command Architecture Contract](./docs/COMMAND-ARCHITECTURE.MD)
- [Execution-Domain Dispatch Scope](./docs/EXECUTION-DOMAIN-SCOPE.MD)
- [`wiki/`](./wiki/Home.md)

## Repository acceptance

`tests/run_tests.py` builds the host lifecycle/Architecture contract, F4 execution-domain-scope contract, compile-time negative contracts and deterministic resource-measurement suites. `CXX` selects the host compiler and optional `CXXFLAGS` are appended for validation configurations such as sanitizers. `demos/CommandLifecycle` exercises Composition/Architecture/Bootstrap, execution, finite terminal waiting, response extraction and quiescence. `demos/FakeOutboundIntegration` exercises exactly-once integration completion without inventing a Transport architecture. `demos/ExecutionDomainScope` demonstrates independent local/remote scoped dispatch in every Policy-required target form. Representative GitHub Actions definitions live under `.github/workflows`, while current Policy makes RPI400 the authoritative executable validation appliance.
