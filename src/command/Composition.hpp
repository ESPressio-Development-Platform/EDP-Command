#pragma once

#include <ESPressio_System.hpp>
#include <ESPressio_Threading.hpp>

#include "CommandTypes.hpp"

namespace ESPressio::Command::Composition {

    /// Composition domain containing Command-owned capabilities.
    struct Domain final : ESPressio::System::CompositionFramework::Domain {};

    /// Exclusive application capability that executes one specific Command type.
    ///
    /// @tparam TCommand Command type executed by the satisfying provider.
    template<class TCommand>
    requires ESPressio::Command::CommandType<TCommand>
    struct Handler final : ESPressio::System::CompositionFramework::ExclusiveCapability<Domain> {
        // Command vocabulary exposed to provider implementations.

        /// Command type associated with this Handler capability.
        using Command = TCommand;

        /// Request type accepted by this Command.
        using RequestType = ESPressio::Command::Request<TCommand>;

        /// Response type produced by this Command.
        using ResponseType = ESPressio::Command::Response<TCommand>;
    };

    /// Requirement selecting exactly one Handler provider for a Command.
    ///
    /// @tparam TCommand Command whose exclusive Handler must be resolved.
    template<class TCommand>
    requires ESPressio::Command::CommandType<TCommand>
    using HandlerRequirement = ESPressio::System::CompositionFramework::Requirement<
        Handler<TCommand>,
        ESPressio::System::CompositionFramework::RequirementScope::SameDomain,
        ESPressio::System::CompositionFramework::ExactlyProviders<1U>
    >;

    /// Resolves the unique Handler provider for a Command from an Architecture or Composition.
    ///
    /// @tparam TCommand Command whose Handler provider is required.
    /// @tparam TComposition Composition-capable type from which the provider is selected.
    template<class TCommand, class TComposition>
    requires ESPressio::Command::CommandType<TCommand>
    using HandlerProvider = typename TComposition::template Select<
        HandlerRequirement<TCommand>,
        ESPressio::System::CompositionFramework::SelectUnique
    >;

    /// Exactly one externally-owned EDP-Threading bounded wait/wake provider is required
    /// by a Command Bootstrap. Capacity is checked by Bootstrap against InvocationCapacity.
    using WaitProviderRequirement = ESPressio::System::CompositionFramework::Requirement<
        ESPressio::Threading::BoundedWaitWake,
        ESPressio::System::CompositionFramework::RequirementScope::ExternalDomain,
        ESPressio::System::CompositionFramework::ExactlyProviders<1U>
    >;

    /// Resolves the unique external bounded wait/wake provider from an Architecture.
    ///
    /// @tparam TComposition Composition-capable type from which the provider is selected.
    template<class TComposition>
    using WaitProvider = typename TComposition::template Select<
        WaitProviderRequirement,
        ESPressio::System::CompositionFramework::SelectUnique
    >;

    /// Semantic identity separating Command Runtime serialization from unrelated mutexes.
    struct RuntimeMutexIdentity final {};

    /// Exactly one application-owned ordinary mutex serializes short Command Runtime transitions.
    using RuntimeMutexRequirement = ESPressio::System::CompositionFramework::Requirement<
        ESPressio::Threading::OrdinaryMutex<RuntimeMutexIdentity>,
        ESPressio::System::CompositionFramework::RequirementScope::ExternalDomain,
        ESPressio::System::CompositionFramework::ExactlyProviders<1U>
    >;

    /// Resolves the unique external Command Runtime mutex provider from an Architecture.
    template<class TComposition>
    using RuntimeMutexProvider = typename TComposition::template Select<
        RuntimeMutexRequirement,
        ESPressio::System::CompositionFramework::SelectUnique
    >;

} // ESPressio::Command::Composition
