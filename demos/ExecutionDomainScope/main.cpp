#include <cassert>
#include <cstddef>
#include <cstdint>

#include <ESPressio_Command.hpp>

namespace Command = ESPressio::Command;

/// Demo-local local-domain outcome.
enum class LocalOutcome : std::uint8_t {
    Accepted = 0U,
    NoCapacity = 1U
};

/// Demo-local remote-domain outcome.
enum class RemoteOutcome : std::uint8_t {
    Accepted = 0U,
    DestinationUnavailable = 1U
};

/// Represents one already-selected local Command Dispatch operation.
struct LocalOperation final {
    /// Counts local-domain invocation attempts.
    std::size_t* Count{nullptr};
    std::uint8_t* Sequence{nullptr};

    /// Result returned by this demonstration operation.
    LocalOutcome Result{LocalOutcome::Accepted};

    /// Executes the selected local-domain operation.
    [[nodiscard]] LocalOutcome operator()() noexcept {
        ++(*Count);
        *Sequence = *Sequence == 0U ? 1U : 0xFFU;
        return Result;
    }
};

/// Represents one already-selected higher-layer remote Dispatch operation.
struct RemoteOperation final {
    /// Counts remote-domain invocation attempts.
    std::size_t* Count{nullptr};
    std::uint8_t* Sequence{nullptr};

    /// Result returned by this demonstration operation.
    RemoteOutcome Result{RemoteOutcome::Accepted};

    /// Executes the selected remote-domain operation.
    [[nodiscard]] RemoteOutcome operator()() noexcept {
        ++(*Count);
        *Sequence = *Sequence == 1U ? 2U : 0xFFU;
        return Result;
    }
};

int main() {
    std::size_t localCount = 0U;
    std::size_t remoteCount = 0U;
    std::uint8_t sequence = 0U;

    LocalOperation local{&localCount, &sequence, LocalOutcome::NoCapacity};
    RemoteOperation remote{&remoteCount, &sequence, RemoteOutcome::Accepted};

    auto result = Command::DispatchScoped(
        Command::LocalAndRemote{},
        local,
        remote
    );

    assert(localCount == 1U);
    assert(remoteCount == 1U);
    assert(sequence == 2U);
    assert(result.Local() == LocalOutcome::NoCapacity);
    assert(result.Remote() == RemoteOutcome::Accepted);
}
