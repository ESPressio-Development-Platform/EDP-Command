#include <Arduino.h>

#include <ESPressio_Command.hpp>
#include <ESPressio_Platform_FreeRTOS.hpp>

namespace Command = ESPressio::Command;
namespace Threading = ESPressio::Threading;
namespace Composition = ESPressio::System::CompositionFramework;

struct Request final {
    int Value;
    explicit Request(int value) noexcept : Value(value) {}
    Request(Request&&) noexcept = default;
    Request(const Request&) = delete;
    ~Request() noexcept = default;
};

struct Response final {
    int Value;
    explicit Response(int value) noexcept : Value(value) {}
    Response(Response&&) noexcept = default;
    Response(const Response&) = delete;
    ~Response() noexcept = default;
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
using Architecture = Composition::Architecture<CommandComposition, ThreadingComposition>;
using Bootstrap = Command::Bootstrap<DoubleCommand, Architecture, Command::ResourcePlan<4U, 3U, 1U>>;

DoubleHandler Handler;
WaitProvider Waits;
Bootstrap CommandRuntime(Handler, Waits);

void setup() {
    Serial.begin(115200);
    if (CommandRuntime.Initialize() != Command::InitializationResult::Initialized) {
        return;
    }

    auto& runtime = CommandRuntime.RuntimeInstance();
    auto dispatch = runtime.Dispatch(Request{21});
    if (!dispatch.Accepted()) {
        return;
    }

    auto handle = dispatch.TakeHandle();
    if (runtime.ExecuteOne() != Command::ExecutionAttemptResult::Executed) {
        return;
    }

    auto response = handle.TakeResponse();
    if (!response.HasValue()) {
        return;
    }

    Serial.printf("response=%d\n", response.Take().Value);
    handle.Release();
    static_cast<void>(runtime.BeginQuiesce());
}

void loop() {
}
