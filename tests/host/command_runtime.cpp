#include <cassert>
#include <type_traits>
#include <ESPressio_Command.hpp>

namespace C = ESPressio::Command;
namespace T = ESPressio::Threading;
namespace CF = ESPressio::System::CompositionFramework;

struct EchoRequest { int Value; };
struct EchoResponse { int Value; };
struct Echo { using Request = EchoRequest; using Response = EchoResponse; };
struct EchoHandler : CF::Provider<C::Composition::Domain, CF::Offers<CF::Offer<C::Composition::Handler<Echo>>>> {
    C::ExecutionResult<EchoResponse> Execute(const EchoRequest& request, C::CancellationToken cancellation) noexcept {
        if (cancellation.IsRequested()) return C::ExecutionResult<EchoResponse>::Failed();
        return C::ExecutionResult<EchoResponse>::Succeeded(EchoResponse{request.Value});
    }
};
struct FakeWaitProvider : CF::Provider<T::Domain, CF::Offers<CF::Offer<T::BoundedWaitWake, CF::PropertyValue<T::BoundedWaitWakeCapacity, 4U>>>> {
    static constexpr std::size_t Capacity = 4U;
    std::size_t WakeCount{0U};
    T::BoundedWaitWakeResult NextWaitResult{T::BoundedWaitWakeResult::TimedOut};
    T::BoundedWaitWakeResult WaitFor(std::size_t slot, C::Duration) noexcept { return slot < Capacity ? NextWaitResult : T::BoundedWaitWakeResult::InvalidSlot; }
    T::BoundedWaitWakeResult WaitUntil(std::size_t slot, C::MonotonicTimestamp) noexcept { return slot < Capacity ? NextWaitResult : T::BoundedWaitWakeResult::InvalidSlot; }
    bool Wake(std::size_t slot) noexcept { if (slot >= Capacity) return false; ++WakeCount; return true; }
};

using CommandComposition = CF::Composition<C::Composition::Domain, EchoHandler>;
using ThreadingComposition = CF::Composition<T::Domain, FakeWaitProvider>;
using ApplicationArchitecture = CF::Architecture<CommandComposition, ThreadingComposition>;
using Plan = C::ResourcePlan<4, 3, 1>;
using CommandBootstrap = C::Bootstrap<Echo, ApplicationArchitecture, Plan>;
using Runtime = CommandBootstrap::RuntimeType;

static_assert(ApplicationArchitecture::IsValid);
static_assert(ApplicationArchitecture::template MatchCount<C::Composition::HandlerRequirement<Echo>> == 1U);
static_assert(ApplicationArchitecture::template MatchCount<C::Composition::WaitProviderRequirement> == 1U);
static_assert(std::is_same_v<C::Composition::HandlerProvider<Echo, ApplicationArchitecture>, EchoHandler>);
static_assert(std::is_same_v<C::Composition::WaitProvider<ApplicationArchitecture>, FakeWaitProvider>);
static_assert(!std::is_copy_constructible_v<C::Handle<Echo, Runtime>>);

int main() {
    EchoHandler handler; FakeWaitProvider waits; CommandBootstrap bootstrap(handler, waits);
    auto& runtime = bootstrap.RuntimeInstance(); assert(bootstrap.Initialize());
    auto dispatch = runtime.Dispatch(EchoRequest{42}); assert(dispatch.Accepted()); auto handle = dispatch.TakeHandle();
    assert(handle.WaitFor(C::Duration::NoWait()) == C::WaitResult::TimedOut);
    waits.NextWaitResult = T::BoundedWaitWakeResult::ProviderFailure;
    assert(handle.WaitFor(C::Duration::NoWait()) == C::WaitResult::Interrupted);
    waits.NextWaitResult = T::BoundedWaitWakeResult::TimedOut;
    assert(runtime.ExecuteOne()); assert(waits.WakeCount == 1U);
    assert(handle.WaitFor(C::Duration::NoWait()) == C::WaitResult::Terminal);
    assert(handle.WaitFor(C::Duration::NoWait()) == C::WaitResult::Terminal);
    auto response = handle.TakeResponse(); assert(response.Status() == C::TakeResponseStatus::Taken); assert(response.Take().Value == 42);
    assert(handle.RequestCancellation() == C::CancellationRequestResult::TooLate);

    auto cancelledDispatch = runtime.Dispatch(EchoRequest{7}); auto cancelled = cancelledDispatch.TakeHandle();
    assert(cancelled.RequestCancellation() == C::CancellationRequestResult::Requested);
    assert(waits.WakeCount == 2U);
    assert(cancelled.RequestCancellation() == C::CancellationRequestResult::TooLate);
    assert(cancelled.WaitFor(C::Duration::NoWait()) == C::WaitResult::Terminal);

    C::Handle<Echo, Runtime> invalid;
    assert(invalid.WaitFor(C::Duration::NoWait()) == C::WaitResult::InvalidHandle);
    assert(invalid.RequestCancellation() == C::CancellationRequestResult::InvalidHandle);
    assert(runtime.BeginQuiesce());
    auto rejected = runtime.Dispatch(EchoRequest{1}); assert(!rejected.Accepted()); assert(rejected.Failure() == C::DispatchFailure::RuntimeUnavailable);
}
