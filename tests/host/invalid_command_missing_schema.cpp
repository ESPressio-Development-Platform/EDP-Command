#include <ESPressio_Command.hpp>

namespace C = ESPressio::Command;

struct AnonymousCommand final {
    using Request = C::NoRequestPayload;
    using Response = C::NoResponsePayload;
};

static_assert(
    C::CommandType<AnonymousCommand>,
    "Anonymous Command declarations without Primitive schema identity must be rejected"
);

int main() {
    return 0;
}
