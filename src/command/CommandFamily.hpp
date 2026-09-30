#pragma once

#include <ESPressio_Primitives.hpp>

namespace ESPressio::Command {

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
