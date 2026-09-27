#include <Arduino.h>
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

    /// Result returned by this demonstration operation.
    LocalOutcome Result{LocalOutcome::Accepted};

    /// Executes the selected local-domain operation.
    [[nodiscard]] LocalOutcome operator()() noexcept {
        ++(*Count);
        return Result;
    }
};

/// Represents one already-selected higher-layer remote Dispatch operation.
struct RemoteOperation final {
    /// Counts remote-domain invocation attempts.
    std::size_t* Count{nullptr};

    /// Result returned by this demonstration operation.
    RemoteOutcome Result{RemoteOutcome::Accepted};

    /// Executes the selected remote-domain operation.
    [[nodiscard]] RemoteOutcome operator()() noexcept {
        ++(*Count);
        return Result;
    }
};

void setup() {
    Serial.begin(115200);

    std::size_t localCount = 0U;
    std::size_t remoteCount = 0U;
    LocalOperation local{&localCount, LocalOutcome::NoCapacity};
    RemoteOperation remote{&remoteCount, RemoteOutcome::Accepted};

    auto result = Command::DispatchScoped(
        Command::LocalAndRemote{},
        local,
        remote
    );

    Serial.printf(
        "local=%u remote=%u local_calls=%u remote_calls=%u\n",
        static_cast<unsigned>(result.Local()),
        static_cast<unsigned>(result.Remote()),
        static_cast<unsigned>(localCount),
        static_cast<unsigned>(remoteCount)
    );
}

void loop() {
}
