#pragma once

#include <ESPressio_Command.hpp>

namespace ESPressio::Command::Tests::Fixtures {

    namespace System = ESPressio::System;

    struct EmptyRequest final {
        static constexpr System::TypeIdentifier Identifier{
            System::TypeIdentifier::Storage{
                0x00U, 0xFEU, 0x03U,
                0x00U, 0x00U, 0x00U, 0x00U, 0x01U
            }
        };

        using Fields = System::FieldSet<>;
    };

    struct EmptyResponse final {
        static constexpr System::TypeIdentifier Identifier{
            System::TypeIdentifier::Storage{
                0x00U, 0xFEU, 0x03U,
                0x00U, 0x00U, 0x00U, 0x00U, 0x02U
            }
        };

        using Fields = System::FieldSet<>;
    };

    struct EmptyCommand final {
        static constexpr System::TypeIdentifier Identifier{
            System::TypeIdentifier::Storage{
                0x00U, 0xFEU, 0x03U,
                0x00U, 0x00U, 0x00U, 0x00U, 0x03U
            }
        };

        using Fields = System::FieldSet<>;
        using Family = ESPressio::Command::Family;
        using Request = EmptyRequest;
        using Response = EmptyResponse;
    };

    static_assert(ESPressio::Command::CommandType<EmptyCommand>);

} // ESPressio::Command::Tests::Fixtures
