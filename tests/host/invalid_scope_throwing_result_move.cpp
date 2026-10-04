#include <cstdint>

#include <ESPressio_Command.hpp>

namespace C = ESPressio::Command;

struct ThrowingMoveResult final {
    ThrowingMoveResult() noexcept = default;
    ThrowingMoveResult(const ThrowingMoveResult&) = delete;
    ThrowingMoveResult& operator=(const ThrowingMoveResult&) = delete;
    ThrowingMoveResult(ThrowingMoveResult&&) noexcept(false) {
    }
};

struct LocalOperation final {
    [[nodiscard]] ThrowingMoveResult operator()() noexcept {
        return ThrowingMoveResult{};
    }
};

struct RemoteOperation final {
    [[nodiscard]] std::uint8_t operator()() noexcept {
        return 0U;
    }
};

int main() {
    LocalOperation local;
    RemoteOperation remote;
    auto invalid = C::DispatchScoped(C::LocalAndRemote{}, local, remote);
    static_cast<void>(invalid);
}
