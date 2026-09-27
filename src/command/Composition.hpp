#pragma once

#include <ESPressio_System.hpp>

#include "CommandTypes.hpp"

namespace ESPressio::Command::Composition {

/// Composition domain for application-owned Command handler capabilities.
struct Domain final : ESPressio::System::CompositionFramework::Domain {};

/// Exclusive application handler capability for one locally executable Command Type.
///
/// A concrete Bootstrap-owned provider offers this capability through EDP-System
/// Composition. The capability itself is intentionally declarative: the provider's
/// concrete Type supplies the Execute(request, cancellation) operation consumed by
/// Command Runtime after compile-time provider resolution.
template<class TCommand>
requires ESPressio::Command::CommandType<TCommand>
struct Handler final : ESPressio::System::CompositionFramework::ExclusiveCapability<Domain> {
    using Command = TCommand;
    using RequestType = ESPressio::Command::Request<TCommand>;
    using ResponseType = ESPressio::Command::Response<TCommand>;
};

/// Exactly-one handler requirement for a locally executable Command Type.
template<class TCommand>
requires ESPressio::Command::CommandType<TCommand>
using HandlerRequirement = ESPressio::System::CompositionFramework::Requirement<
    Handler<TCommand>,
    ESPressio::System::CompositionFramework::RequirementScope::SameDomain,
    ESPressio::System::CompositionFramework::ExactlyProviders<1U>
>;

/// Resolves the concrete provider Type supplying the unique handler for TCommand.
template<class TCommand, class TComposition>
requires ESPressio::Command::CommandType<TCommand>
using HandlerProvider = typename TComposition::template Select<
    HandlerRequirement<TCommand>,
    ESPressio::System::CompositionFramework::SelectUnique
>;

} // ESPressio::Command::Composition
