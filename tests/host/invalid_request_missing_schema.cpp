#include <ESPressio_Command.hpp>

namespace C = ESPressio::Command;
namespace S = ESPressio::System;

struct InvalidRequest final {
    int Value{0};
};

struct CommandWithInvalidRequest final {
    static constexpr S::TypeIdentifier Identifier{
        S::TypeIdentifier::Storage{
            0x00U, 0xFEU, 0x05U,
            0x00U, 0x00U, 0x00U, 0x00U, 0x01U
        }
    };

    using Fields = S::FieldSet<>;
    using Family = C::Family;
    using Request = InvalidRequest;
    using Response = C::NoResponsePayload;
};

static_assert(
    C::CommandType<CommandWithInvalidRequest>,
    "Command Request Types without System schema identity must be rejected"
);

int main() {
    return 0;
}
