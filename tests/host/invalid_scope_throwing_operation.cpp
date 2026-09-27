#include <ESPressio_Command.hpp>

namespace Command = ESPressio::Command;

/// Invalid demonstration operation because scoped Dispatch operations must be non-throwing.
struct ThrowingRemoteOperation final {
    /// Returns a result but does not provide the required noexcept contract.
    [[nodiscard]] int operator()() {
        return 1;
    }
};

int main() {
    ThrowingRemoteOperation remote;
    (void)Command::DispatchScoped(
        Command::RemoteOnly{},
        remote
    );
}
