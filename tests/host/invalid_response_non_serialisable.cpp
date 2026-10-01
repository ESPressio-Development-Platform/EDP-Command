#include <ESPressio_Command.hpp>

namespace C = ESPressio::Command;
namespace S = ESPressio::System;

struct UnsupportedValue final {
    void* Pointer{nullptr};
};

struct InvalidResponse final {
    static constexpr S::TypeIdentifier Identifier{
        S::TypeIdentifier::Storage{0x00U,0xFEU,0x07U,0x00U,0x00U,0x00U,0x00U,0x01U}
    };
    UnsupportedValue Value{};
    using Fields = S::FieldSet<S::FieldBinding<&InvalidResponse::Value, 1U>>;
};

struct CommandWithInvalidResponse final {
    static constexpr S::TypeIdentifier Identifier{
        S::TypeIdentifier::Storage{0x00U,0xFEU,0x07U,0x00U,0x00U,0x00U,0x00U,0x02U}
    };
    using Fields = S::FieldSet<>;
    using Family = C::Family;
    using Request = C::NoRequestPayload;
    using Response = InvalidResponse;
};

static_assert(S::SchemaType<InvalidResponse>);
static_assert(C::CommandType<CommandWithInvalidResponse>, "Non-serialisable Response must be rejected");
int main() { return 0; }
