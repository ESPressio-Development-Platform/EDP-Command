#pragma once

#include <cstdint>
#include <ESPressio_Primitives.hpp>

namespace ESPressio::Command {

    /// Version of the Command-family Mesh wire operation vocabulary.
    inline static constexpr std::uint16_t WireOperationVersion = 1U;

    /// Stable Command-family semantic operations carried by Mesh.
    enum class WireOperation : std::uint16_t {
        Invocation = 1U,
        Cancellation = 2U,
        TerminalResult = 3U
    };

    static_assert(
        static_cast<std::uint16_t>(WireOperation::Invocation) != 0U &&
        WireOperation::Invocation != WireOperation::Cancellation &&
        WireOperation::Invocation != WireOperation::TerminalResult &&
        WireOperation::Cancellation != WireOperation::TerminalResult
    );


    /// Command-family planner contract type.
    ///
    /// The current Command Runtime/Bootstrap implementation predates direct family-Topology
    /// planning. The semantic family association is nevertheless authoritative and stable;
    /// Planner remains the canonical family planner Type as the deployment layer is reconciled.
    struct Planner final {};


    /// Primitive family tag for typed Command operations.
    struct Family final {

        // Primitive-family identity.

        /// ESPressio-governed Primitive family identity: Authority 1, Command family 1.
        inline static constexpr Primitives::PrimitiveFamilyIdentifier Identifier{
            System::TypeAuthorityIdentifier{
                System::TypeAuthorityIdentifier::Storage{0x00U, 0x00U, 0x01U}
            },
            0x01U
        };

        /// Canonical family planner association.
        using Planner = Command::Planner;

    };


    static_assert(
        Primitives::PrimitiveFamilyType<Family>,
        "Command::Family must satisfy the canonical Primitive family contract"
    );

} // ESPressio::Command
