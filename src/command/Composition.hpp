#pragma once

#include <ESPressio_System.hpp>
#include <ESPressio_Threading.hpp>

#include "CommandTypes.hpp"

namespace ESPressio::Command::Composition {

struct Domain final : ESPressio::System::CompositionFramework::Domain {};

template<class TCommand>
requires ESPressio::Command::CommandType<TCommand>
struct Handler final : ESPressio::System::CompositionFramework::ExclusiveCapability<Domain> {
    using Command = TCommand;
    using RequestType = ESPressio::Command::Request<TCommand>;
    using ResponseType = ESPressio::Command::Response<TCommand>;
};

template<class TCommand>
requires ESPressio::Command::CommandType<TCommand>
using HandlerRequirement = ESPressio::System::CompositionFramework::Requirement<
    Handler<TCommand>,
    ESPressio::System::CompositionFramework::RequirementScope::SameDomain,
    ESPressio::System::CompositionFramework::ExactlyProviders<1U>
>;

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

template<class TComposition>
using WaitProvider = typename TComposition::template Select<
    WaitProviderRequirement,
    ESPressio::System::CompositionFramework::SelectUnique
>;

} // ESPressio::Command::Composition
