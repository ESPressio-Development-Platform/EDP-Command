#include <ESPressio_Command.hpp>

#include "EmptyCommandFixture.hpp"

namespace C = ESPressio::Command;
namespace T = ESPressio::Threading;
namespace CF = ESPressio::System::CompositionFramework;
namespace F = ESPressio::Command::Tests::Fixtures;

using Request = F::EmptyRequest;
using Response = F::EmptyResponse;
using Command = F::EmptyCommand;

struct HandlerA final : CF::Provider<C::Composition::Domain, CF::Offers<CF::Offer<C::Composition::Handler<Command>>>> {
    C::ExecutionResult<Response> Execute(const Request&, C::CancellationToken) noexcept {
        return C::ExecutionResult<Response>::Succeeded(Response{});
    }
};

struct HandlerB final : CF::Provider<C::Composition::Domain, CF::Offers<CF::Offer<C::Composition::Handler<Command>>>> {
    C::ExecutionResult<Response> Execute(const Request&, C::CancellationToken) noexcept {
        return C::ExecutionResult<Response>::Succeeded(Response{});
    }
};

struct WaitProvider final : CF::Provider<
    T::Domain,
    CF::Offers<CF::Offer<T::BoundedWaitWake, CF::PropertyValue<T::BoundedWaitWakeCapacity, 1U>>>
> {
    static constexpr std::size_t Capacity = 1U;
};

struct MutexProvider final : CF::Provider<
    T::Domain,
    CF::Offers<CF::Offer<T::OrdinaryMutex<C::Composition::RuntimeMutexIdentity>>>
> {
    [[nodiscard]] T::OrdinaryMutexAcquireResult Acquire() noexcept {
        return T::OrdinaryMutexAcquireResult::Acquired;
    }
    [[nodiscard]] T::OrdinaryMutexReleaseResult Release() noexcept {
        return T::OrdinaryMutexReleaseResult::Released;
    }
};

using CommandComposition = CF::Composition<C::Composition::Domain, HandlerA, HandlerB>;
using ThreadingComposition = CF::Composition<T::Domain, WaitProvider, MutexProvider>;
using Architecture = CF::Architecture<CommandComposition, ThreadingComposition>;
using Plan = C::ResourcePlan<1U, 0U, 1U>;
using InvalidBootstrap = C::Bootstrap<Command, Architecture, Plan>;

int main() {
    static_cast<void>(sizeof(InvalidBootstrap));
    return 0;
}
