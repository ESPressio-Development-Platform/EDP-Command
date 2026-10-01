#include <ESPressio_Command.hpp>

namespace C = ESPressio::Command;
namespace S = ESPressio::System;

struct UnsupportedValue final {
    void* Pointer{nullptr};
};

struct InvalidRequest final {
    static constexpr S::TypeIdentifier Identifier{
        S::TypeIdentifier::Storage{0x00U,0xFEU,0x06U,0x00U,0x00U,0x00U,0x00U,0x01U}
    };
    UnsupportedValue Value{};
    using Fields = S::FieldSet<S::FieldBinding<&InvalidRequest::Value, 1U>>;
};

struct CommandWithInvalidRequest final {
    static constexpr S::TypeIdentifier Identifier{
        S::TypeIdentifier::Storage{0x00U,0xFEU,0x06U,0x00U,0x00U,0x00U,0x00U,0x02U}
    };
    using Fields = S::FieldSet<>;
    using Family = C::Family;
    using Request = InvalidRequest;
    using Response = C::NoResponsePayload;
};

static_assert(S::SchemaType<InvalidRequest>);
static_assert(C::CommandType<CommandWithInvalidRequest>, "Non-serialisable Request must be rejected");
int main() { return 0; }
