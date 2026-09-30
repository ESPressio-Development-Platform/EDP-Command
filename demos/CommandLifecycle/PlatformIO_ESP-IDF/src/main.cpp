#include <cstdint>
#include <cstdio>

#include <ESPressio_Command.hpp>
#include <ESPressio_Platform_FreeRTOS.hpp>

namespace Command = ESPressio::Command;
namespace Threading = ESPressio::Threading;
namespace System = ESPressio::System;
namespace Composition = ESPressio::System::CompositionFramework;

struct Request final {
    static constexpr System::TypeIdentifier Identifier{
        System::TypeIdentifier::Storage{
            0x00U, 0xFEU, 0x10U,
            0x00U, 0x00U, 0x00U, 0x00U, 0x01U
        }
    };

    std::int32_t Value;

    using Fields = System::FieldSet<
        System::FieldBinding<&Request::Value, 0U>
    >;

    explicit Request(std::int32_t value) noexcept : Value(value) {}
    Request(Request&&) noexcept = default;
    Request(const Request&) = delete;
    ~Request() noexcept = default;
};

struct Response final {
    static constexpr System::TypeIdentifier Identifier{
        System::TypeIdentifier::Storage{
            0x00U, 0xFEU, 0x10U,
            0x00U, 0x00U, 0x00U, 0x00U, 0x02U
        }
    };

    std::int32_t Value;

    using Fields = System::FieldSet<
        System::FieldBinding<&Response::Value, 0U>
    >;

    explicit Response(std::int32_t value) noexcept : Value(value) {}
    Response(Response&&) noexcept = default;
    Response(const Response&) = delete;
    ~Response() noexcept = default;
};

struct DoubleCommand final {
    static constexpr System::TypeIdentifier Identifier{
        System::TypeIdentifier::Storage{
            0x00U, 0xFEU, 0x10U,
            0x00U, 0x00U, 0x00U, 0x00U, 0x03U
        }
    };

    using Fields = System::FieldSet<>;
    using Family = Command::Family;
    using Request = ::Request;
    using Response = ::Response;
};

static_assert(Command::CommandType<DoubleCommand>);

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

extern "C" void app_main() {
    DoubleHandler handler;
    WaitProvider waits;
    Bootstrap commandRuntime(handler, waits);

    if (commandRuntime.Initialize() != Command::InitializationResult::Initialized) {
        return;
    }

    auto& runtime = commandRuntime.RuntimeInstance();
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

    std::printf("response=%d\n", static_cast<int>(response.Take().Value));
    handle.Release();
    static_cast<void>(runtime.BeginQuiesce());
}
