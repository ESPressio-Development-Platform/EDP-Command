#include <cstdint>
#include <cstdio>

#include <ESPressio_Command.hpp>
#include <ESPressio_Platform_FreeRTOS.hpp>

namespace Command = ESPressio::Command;
namespace Threading = ESPressio::Threading;
namespace System = ESPressio::System;
namespace Composition = ESPressio::System::CompositionFramework;

struct AddRequest final {
    static constexpr System::TypeIdentifier Identifier{
        System::TypeIdentifier::Storage{
            0x00U, 0xFEU, 0x20U,
            0x00U, 0x00U, 0x00U, 0x00U, 0x01U
        }
    };

    std::int32_t Left{0};
    std::int32_t Right{0};

    using Fields = System::FieldSet<
        System::FieldBinding<&AddRequest::Left, 0U>,
        System::FieldBinding<&AddRequest::Right, 1U>
    >;
};

struct AddResponse final {
    static constexpr System::TypeIdentifier Identifier{
        System::TypeIdentifier::Storage{
            0x00U, 0xFEU, 0x20U,
            0x00U, 0x00U, 0x00U, 0x00U, 0x02U
        }
    };

    std::int32_t Value{0};

    using Fields = System::FieldSet<
        System::FieldBinding<&AddResponse::Value, 0U>
    >;
};

struct Add final {
    static constexpr System::TypeIdentifier Identifier{
        System::TypeIdentifier::Storage{
            0x00U, 0xFEU, 0x20U,
            0x00U, 0x00U, 0x00U, 0x00U, 0x03U
        }
    };

    using Fields = System::FieldSet<>;
    using Family = Command::Family;
    using Request = AddRequest;
    using Response = AddResponse;
};

static_assert(Command::CommandType<Add>);

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
using CommandMutex = Threading::OrdinaryMutexProvider<
    Command::Composition::RuntimeMutexIdentity,
    ESPressio::Platform::FreeRTOS::Synchronization::MutexProvider
>;
using ApplicationCommands = Composition::Composition<Command::Composition::Domain, AddHandler>;
using ApplicationThreading = Composition::Composition<Threading::Domain, WaitProvider, CommandMutex>;
using ApplicationArchitecture = Composition::Architecture<ApplicationCommands, ApplicationThreading>;
using AddBootstrap = Command::Bootstrap<
    Add,
    ApplicationArchitecture,
    Command::ResourcePlan<4U, 3U, 1U>
>;

int main() {
    AddHandler addHandler;
    WaitProvider waits;
    CommandMutex mutex;
    AddBootstrap addCommand(addHandler, waits, mutex);

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

    std::printf("20 + 22 = %d\n", static_cast<int>(response.Take().Value));
    return 0;
}
