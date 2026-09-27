#include <cstdio>

#include <ESPressio_Command.hpp>
#include <ESPressio_Platform_FreeRTOS.hpp>

namespace Command = ESPressio::Command;
namespace Threading = ESPressio::Threading;
namespace Composition = ESPressio::System::CompositionFramework;

struct Request final {
    int Value;
};

struct Response final {
    int Value;
};

struct DoubleCommand final {
    using Request = ::Request;
    using Response = ::Response;
};

struct DoubleHandler final : Composition::Provider<
    Command::Composition::Domain,
    Composition::Offers<Composition::Offer<Command::Composition::Handler<DoubleCommand>>>
> {
    Command::ExecutionResult<Response> Execute(
        const Request& request,
        Command::CancellationToken
    ) noexcept {
        return Command::ExecutionResult<Response>::Succeeded(Response{request.Value * 2});
    }
};

using WaitProvider = Threading::BoundedWaitWakeProvider<
    ESPressio::Platform::FreeRTOS::Synchronization::SignalProvider,
    4U
>;
using CommandComposition = Composition::Composition<Command::Composition::Domain, DoubleHandler>;
using ThreadingComposition = Composition::Composition<Threading::Domain, WaitProvider>;
using ApplicationArchitecture = Composition::Architecture<CommandComposition, ThreadingComposition>;
using CommandBootstrap = Command::Bootstrap<
    DoubleCommand,
    ApplicationArchitecture,
    Command::ResourcePlan<4U, 3U, 1U>
>;

int main() {
    DoubleHandler handler;
    WaitProvider waits;
    CommandBootstrap bootstrap(handler, waits);

    if (bootstrap.Initialize() != Command::InitializationResult::Initialized) {
        return 1;
    }

    auto& runtime = bootstrap.RuntimeInstance();
    auto dispatch = runtime.Dispatch(Request{21});
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

    std::printf("response=%d\n", response.Take().Value);
    handle.Release();
    static_cast<void>(runtime.BeginQuiesce());
    return 0;
}
