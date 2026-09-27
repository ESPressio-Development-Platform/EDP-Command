#include <ESPressio_Command.hpp>

namespace Command = ESPressio::Command;

/// Invalid demonstration operation because scoped Dispatch requires an observable result.
struct VoidRemoteOperation final {
    /// Performs a response-less remote operation, which F4 deliberately rejects.
    void operator()() noexcept {
    }
};

int main() {
    VoidRemoteOperation remote;
    Command::DispatchScoped(
        Command::RemoteOnly{},
        remote
    );
}
