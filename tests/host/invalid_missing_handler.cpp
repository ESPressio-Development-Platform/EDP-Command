#include <ESPressio_Command.hpp>

#include "EmptyCommandFixture.hpp"

namespace C = ESPressio::Command;
namespace T = ESPressio::Threading;
namespace CF = ESPressio::System::CompositionFramework;
namespace F = ESPressio::Command::Tests::Fixtures;

using Command = F::EmptyCommand;

struct WaitProvider final : CF::Provider<
    T::Domain,
    CF::Offers<CF::Offer<T::BoundedWaitWake, CF::PropertyValue<T::BoundedWaitWakeCapacity, 1U>>>
> {
    static constexpr std::size_t Capacity = 1U;
};

using CommandComposition = CF::Composition<C::Composition::Domain>;
using ThreadingComposition = CF::Composition<T::Domain, WaitProvider>;
using Architecture = CF::Architecture<CommandComposition, ThreadingComposition>;
using Plan = C::ResourcePlan<1U, 0U, 1U>;
using InvalidBootstrap = C::Bootstrap<Command, Architecture, Plan>;

int main() {
    static_cast<void>(sizeof(InvalidBootstrap));
    return 0;
}
