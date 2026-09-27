#include <cstdio>

#include <ESPressio_Command.hpp>
#include <ESPressio_Platform_FreeRTOS.hpp>

namespace Command = ESPressio::Command;
namespace Threading = ESPressio::Threading;
namespace Composition = ESPressio::System::CompositionFramework;

struct AddRequest final {
    int Left;
    int Right;
};

struct AddResponse final {
    int Value;
};

struct Add final {
    using Request = AddRequest;
    using Response = AddResponse;
};

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

using WaitProvider = Threading::BoundedWaitWakeProvider<
    ESPressio::Platform::FreeRTOS::Synchronization::SignalProvider,
    4U
>;
using ApplicationCommands = Composition::Composition<Command::Composition::Domain, AddHandler>;
using ApplicationThreading = Composition::Composition<Threading::Domain, WaitProvider>;
using ApplicationArchitecture = Composition::Architecture<ApplicationCommands, ApplicationThreading>;
using AddBootstrap = Command::Bootstrap<
    Add,
    ApplicationArchitecture,
    Command::ResourcePlan<4U, 3U, 1U>
>;

int main() {
    AddHandler addHandler;
    WaitProvider waits;
    AddBootstrap addCommand(addHandler, waits);

    if (addCommand.Initialize() != Command::InitializationResult::Initialized) {
        return 1;
    }

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
    if (!response.HasValue()) {
        return 5;
    }

    std::printf("20 + 22 = %d\n", response.Take().Value);
    return 0;
}
