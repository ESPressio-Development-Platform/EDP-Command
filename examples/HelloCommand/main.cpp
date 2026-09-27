#include <cstdio>
#include <ESPressio_Command.hpp>

namespace Command = ESPressio::Command;
namespace Composition = ESPressio::System::CompositionFramework;

struct AddRequest {
    int Left;
    int Right;
};

struct AddResponse {
    int Value;
};

struct Add final {
    using Request = AddRequest;
    using Response = AddResponse;
};

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

using ApplicationCommands = Composition::Composition<
    Command::Composition::Domain,
    AddHandler
>;

using AddBootstrap = Command::Bootstrap<
    Add,
    ApplicationCommands,
    Command::ResourcePlan<4, 3, 1>
>;

int main() {
    // Application Bootstrap owns persistent providers before constructing Command runtime wiring.
    AddHandler addHandler;
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
    if (!runtime.ExecuteOne()) {
        return 3;
    }

    auto response = handle.TakeResponse();
    if (!response.HasValue()) {
        return 4;
    }

    std::printf("20 + 22 = %d\n", response.Take().Value);
    return 0;
}
