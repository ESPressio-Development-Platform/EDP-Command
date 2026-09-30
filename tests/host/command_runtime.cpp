#include <cassert>
#include <cstdint>
#include <type_traits>

#include <ESPressio_Command.hpp>

namespace C = ESPressio::Command;
namespace T = ESPressio::Threading;
namespace S = ESPressio::System;
namespace CF = ESPressio::System::CompositionFramework;

struct EchoRequest final {
    static constexpr S::TypeIdentifier Identifier{
        S::TypeIdentifier::Storage{
            0x00U, 0xFEU, 0x01U,
            0x00U, 0x00U, 0x00U, 0x00U, 0x01U
        }
    };

    std::int32_t Value{0};

    using Fields = S::FieldSet<
        S::FieldBinding<&EchoRequest::Value, 0U>
    >;
};

struct EchoResponse final {
    static constexpr S::TypeIdentifier Identifier{
        S::TypeIdentifier::Storage{
            0x00U, 0xFEU, 0x01U,
            0x00U, 0x00U, 0x00U, 0x00U, 0x02U
        }
    };

    std::int32_t Value{0};

    using Fields = S::FieldSet<
        S::FieldBinding<&EchoResponse::Value, 0U>
    >;
};

struct Echo final {
    static constexpr S::TypeIdentifier Identifier{
        S::TypeIdentifier::Storage{
            0x00U, 0xFEU, 0x01U,
            0x00U, 0x00U, 0x00U, 0x00U, 0x03U
        }
    };

    using Fields = S::FieldSet<>;
    using Family = C::Family;
    using Request = EchoRequest;
    using Response = EchoResponse;
};

static_assert(C::CommandType<Echo>);

struct EchoHandler final : CF::Provider<C::Composition::Domain, CF::Offers<CF::Offer<C::Composition::Handler<Echo>>>> {
    C::ExecutionResult<EchoResponse> Execute(const EchoRequest& request, C::CancellationToken cancellation) noexcept {
        if (cancellation.IsRequested()) {
            return C::ExecutionResult<EchoResponse>::Failed();
        }
        return C::ExecutionResult<EchoResponse>::Succeeded(EchoResponse{request.Value});
    }
};

struct FakeWaitProvider final : CF::Provider<T::Domain, CF::Offers<CF::Offer<T::BoundedWaitWake, CF::PropertyValue<T::BoundedWaitWakeCapacity, 4U>>>> {
    static constexpr std::size_t Capacity = 4U;
    std::size_t WakeCount{0U};
    T::BoundedWaitWakeResult NextWaitResult{T::BoundedWaitWakeResult::TimedOut};

    [[nodiscard]] T::BoundedWaitWakeResult WaitFor(std::size_t slot, C::Duration) noexcept {
        return slot < Capacity ? NextWaitResult : T::BoundedWaitWakeResult::InvalidSlot;
    }

    [[nodiscard]] T::BoundedWaitWakeResult WaitUntil(std::size_t slot, C::MonotonicTimestamp) noexcept {
        return slot < Capacity ? NextWaitResult : T::BoundedWaitWakeResult::InvalidSlot;
    }

    [[nodiscard]] T::BoundedWaitWakeResult Wake(std::size_t slot) noexcept {
        if (slot >= Capacity) {
            return T::BoundedWaitWakeResult::InvalidSlot;
        }
        ++WakeCount;
        return T::BoundedWaitWakeResult::Woken;
    }
};

using CommandComposition = CF::Composition<C::Composition::Domain, EchoHandler>;
using ThreadingComposition = CF::Composition<T::Domain, FakeWaitProvider>;
using ApplicationArchitecture = CF::Architecture<CommandComposition, ThreadingComposition>;
using Plan = C::ResourcePlan<4U, 3U, 1U>;
using CommandBootstrap = C::Bootstrap<Echo, ApplicationArchitecture, Plan>;
using Runtime = CommandBootstrap::RuntimeType;

static_assert(ApplicationArchitecture::IsValid);
static_assert(ApplicationArchitecture::template MatchCount<C::Composition::HandlerRequirement<Echo>> == 1U);
static_assert(ApplicationArchitecture::template MatchCount<C::Composition::WaitProviderRequirement> == 1U);
static_assert(std::is_same_v<C::Composition::HandlerProvider<Echo, ApplicationArchitecture>, EchoHandler>);
static_assert(std::is_same_v<C::Composition::WaitProvider<ApplicationArchitecture>, FakeWaitProvider>);
static_assert(!std::is_copy_constructible_v<C::Handle<Echo, Runtime>>);

int main() {
    const C::Duration noWait{};
    EchoHandler handler;
    FakeWaitProvider waits;
    CommandBootstrap bootstrap(handler, waits);
    auto& runtime = bootstrap.RuntimeInstance();

    assert(bootstrap.Initialize() == C::InitializationResult::Initialized);

    auto dispatch = runtime.Dispatch(EchoRequest{42});
    assert(dispatch.Accepted());
    auto handle = dispatch.TakeHandle();
    assert(handle.WaitFor(noWait) == C::WaitResult::TimedOut);

    waits.NextWaitResult = T::BoundedWaitWakeResult::ProviderFailure;
    assert(handle.WaitFor(noWait) == C::WaitResult::Interrupted);
    waits.NextWaitResult = T::BoundedWaitWakeResult::TimedOut;

    assert(runtime.ExecuteOne() == C::ExecutionAttemptResult::Executed);
    assert(waits.WakeCount == 1U);
    assert(handle.WaitFor(noWait) == C::WaitResult::Terminal);
    assert(handle.WaitFor(noWait) == C::WaitResult::Terminal);

    bool valid = false;
    const auto completedObservation = handle.State(valid);
    assert(valid);
    assert(completedObservation.DidSucceed());
    assert(!completedObservation.WasRejected());
    assert(!completedObservation.DidFail());
    assert(!completedObservation.WasCancelled());

    auto response = handle.TakeResponse();
    assert(response.Status() == C::TakeResponseStatus::Taken);
    assert(response.Take().Value == 42);
    assert(handle.RequestCancellation() == C::CancellationRequestResult::TooLate);

    auto cancelledDispatch = runtime.Dispatch(EchoRequest{7});
    auto cancelled = cancelledDispatch.TakeHandle();
    assert(cancelled.RequestCancellation() == C::CancellationRequestResult::Requested);
    assert(waits.WakeCount == 2U);
    assert(cancelled.RequestCancellation() == C::CancellationRequestResult::TooLate);
    assert(cancelled.WaitFor(noWait) == C::WaitResult::Terminal);

    const auto cancelledObservation = cancelled.State(valid);
    assert(valid);
    assert(cancelledObservation.WasCancelled());
    assert(!cancelledObservation.DidSucceed());

    C::Handle<Echo, Runtime> invalid;
    assert(invalid.WaitFor(noWait) == C::WaitResult::InvalidHandle);
    assert(invalid.RequestCancellation() == C::CancellationRequestResult::InvalidHandle);

    assert(runtime.BeginQuiesce() == C::QuiesceResult::Started);
    auto rejected = runtime.Dispatch(EchoRequest{1});
    assert(!rejected.Accepted());
    assert(rejected.Failure() == C::DispatchFailure::RuntimeUnavailable);
}
