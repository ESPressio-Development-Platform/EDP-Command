#include <cassert>
#include <cstdint>
#include <type_traits>
#include <utility>

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

inline bool RuntimeMutexHeld = false;

struct EchoHandler final : CF::Provider<C::Composition::Domain, CF::Offers<CF::Offer<C::Composition::Handler<Echo>>>> {
    void* HookContext{nullptr};
    void (*OnExecute)(void*) noexcept {nullptr};

    C::ExecutionResult<EchoResponse> Execute(const EchoRequest& request, C::CancellationToken cancellation) noexcept {
        assert(!RuntimeMutexHeld);
        if (OnExecute != nullptr) {
            OnExecute(HookContext);
        }
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
        assert(!RuntimeMutexHeld);
        return slot < Capacity ? NextWaitResult : T::BoundedWaitWakeResult::InvalidSlot;
    }

    [[nodiscard]] T::BoundedWaitWakeResult WaitUntil(std::size_t slot, C::MonotonicTimestamp) noexcept {
        assert(!RuntimeMutexHeld);
        return slot < Capacity ? NextWaitResult : T::BoundedWaitWakeResult::InvalidSlot;
    }

    [[nodiscard]] T::BoundedWaitWakeResult Wake(std::size_t slot) noexcept {
        assert(!RuntimeMutexHeld);
        if (slot >= Capacity) {
            return T::BoundedWaitWakeResult::InvalidSlot;
        }
        ++WakeCount;
        return T::BoundedWaitWakeResult::Woken;
    }
};

struct FakeMutex final : CF::Provider<
    T::Domain,
    CF::Offers<CF::Offer<T::OrdinaryMutex<C::Composition::RuntimeMutexIdentity>>>
> {
    std::size_t AcquireCount{0U};
    std::size_t ReleaseCount{0U};

    [[nodiscard]] T::OrdinaryMutexAcquireResult Acquire() noexcept {
        if (RuntimeMutexHeld) {
            return T::OrdinaryMutexAcquireResult::ProviderFailure;
        }
        RuntimeMutexHeld = true;
        ++AcquireCount;
        return T::OrdinaryMutexAcquireResult::Acquired;
    }

    [[nodiscard]] T::OrdinaryMutexReleaseResult Release() noexcept {
        if (!RuntimeMutexHeld) {
            return T::OrdinaryMutexReleaseResult::ProviderFailure;
        }
        RuntimeMutexHeld = false;
        ++ReleaseCount;
        return T::OrdinaryMutexReleaseResult::Released;
    }
};

using CommandComposition = CF::Composition<C::Composition::Domain, EchoHandler>;
using ThreadingComposition = CF::Composition<T::Domain, FakeWaitProvider, FakeMutex>;
using ApplicationArchitecture = CF::Architecture<CommandComposition, ThreadingComposition>;
using Plan = C::ResourcePlan<4U, 3U, 1U, 4U>;
using CommandBootstrap = C::Bootstrap<Echo, ApplicationArchitecture, Plan>;
using Runtime = CommandBootstrap::RuntimeType;

static_assert(ApplicationArchitecture::IsValid);
static_assert(ApplicationArchitecture::template MatchCount<C::Composition::HandlerRequirement<Echo>> == 1U);
static_assert(ApplicationArchitecture::template MatchCount<C::Composition::WaitProviderRequirement> == 1U);
static_assert(ApplicationArchitecture::template MatchCount<C::Composition::RuntimeMutexRequirement> == 1U);
static_assert(std::is_same_v<C::Composition::HandlerProvider<Echo, ApplicationArchitecture>, EchoHandler>);
static_assert(std::is_same_v<C::Composition::WaitProvider<ApplicationArchitecture>, FakeWaitProvider>);
static_assert(std::is_same_v<C::Composition::RuntimeMutexProvider<ApplicationArchitecture>, FakeMutex>);
static_assert(!std::is_copy_constructible_v<C::Handle<Echo, Runtime>>);

void CancelDuringExecution(void* context) noexcept {
    auto& handle = *static_cast<C::Handle<Echo, Runtime>*>(context);
    assert(handle.RequestCancellation() == C::CancellationRequestResult::Requested);
}

struct LocalAdmissionOperation final {
    int* Sequence{nullptr};

    [[nodiscard]] bool operator()() noexcept {
        assert(*Sequence == 0);
        *Sequence = 1;
        return true;
    }
};

struct BoundedRemoteOperation final {
    int* Sequence{nullptr};

    [[nodiscard]] std::int32_t operator()(const EchoRequest& request) noexcept {
        assert(!RuntimeMutexHeld);
        assert(*Sequence == 1);
        *Sequence = 2;
        return request.Value;
    }
};

int main() {
    const C::Duration noWait{};
    EchoHandler handler;
    FakeWaitProvider waits;
    FakeMutex mutex;
    CommandBootstrap bootstrap(handler, waits, mutex);
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

    // Inbound population owns constructed, unpublished backing until explicit commit.
    {
        auto prepared = runtime.PrepareIngress();
        assert(prepared.Accepted());
        auto reservation = std::move(prepared).TakeReservation();
        reservation.Value().Value = 73;
        auto committed = reservation.Commit();
        assert(committed.Accepted());
        auto inbound = committed.TakeHandle();
        assert(runtime.ExecuteOne() == C::ExecutionAttemptResult::Executed);
        auto inboundResponse = inbound.TakeResponse();
        assert(inboundResponse.HasValue());
        assert(inboundResponse.Take().Value == 73);
    }
    assert(waits.WakeCount == 2U);

    // Outbound handoff stages are bounded and invoke the adapter only after local admission.
    EchoRequest remoteRequest{31};
    C::OutboundHandoff<Echo, Runtime> outbound(runtime);
    auto preparedHandoff = outbound.Prepare(remoteRequest);
    assert(preparedHandoff.Accepted());
    auto handoff = std::move(preparedHandoff).TakeReservation();
    int sequence = 0;
    LocalAdmissionOperation localOperation{&sequence};
    BoundedRemoteOperation remoteOperation{&sequence};
    auto scoped = C::DispatchScoped(
        C::LocalAndRemote{},
        std::move(handoff),
        localOperation,
        remoteOperation
    );
    assert(scoped.Local());
    assert(scoped.Remote() == 31);
    assert(sequence == 2);

    {
        auto firstStage = outbound.Prepare(remoteRequest);
        auto secondStage = outbound.Prepare(remoteRequest);
        auto thirdStage = outbound.Prepare(remoteRequest);
        auto fourthStage = outbound.Prepare(remoteRequest);
        auto exhaustedStage = outbound.Prepare(remoteRequest);
        assert(firstStage.Accepted());
        assert(secondStage.Accepted());
        assert(thirdStage.Accepted());
        assert(fourthStage.Accepted());
        assert(!exhaustedStage.Accepted());
        assert(exhaustedStage.Failure() == C::DispatchFailure::NoCapacity);
    }

    // Destruction aborts an unpublished ingress reservation without consuming queue capacity.
    {
        auto prepared = runtime.PrepareIngress();
        assert(prepared.Accepted());
        auto reservation = std::move(prepared).TakeReservation();
        reservation.Value().Value = 99;
    }

    // Handler execution is outside Command serialization and cancellation is data-race-safe.
    auto executingDispatch = runtime.Dispatch(EchoRequest{8});
    auto executing = executingDispatch.TakeHandle();
    handler.HookContext = &executing;
    handler.OnExecute = &CancelDuringExecution;
    assert(runtime.ExecuteOne() == C::ExecutionAttemptResult::Executed);
    handler.OnExecute = nullptr;
    assert(executing.WaitFor(noWait) == C::WaitResult::Terminal);
    const auto executingObservation = executing.State(valid);
    assert(valid);
    assert(executingObservation.WasCancelled());
    assert(waits.WakeCount == 3U);

    auto cancelledDispatch = runtime.Dispatch(EchoRequest{7});
    auto cancelled = cancelledDispatch.TakeHandle();
    assert(cancelled.RequestCancellation() == C::CancellationRequestResult::Requested);
    assert(waits.WakeCount == 4U);
    assert(cancelled.RequestCancellation() == C::CancellationRequestResult::TooLate);
    assert(cancelled.WaitFor(noWait) == C::WaitResult::Terminal);

    const auto cancelledObservation = cancelled.State(valid);
    assert(valid);
    assert(cancelledObservation.WasCancelled());
    assert(!cancelledObservation.DidSucceed());

    C::Handle<Echo, Runtime> invalid;
    assert(invalid.WaitFor(noWait) == C::WaitResult::InvalidHandle);
    assert(invalid.RequestCancellation() == C::CancellationRequestResult::InvalidHandle);

    // Drain the cancelled queue tombstone, release all Handles, then prove an admitted
    // handoff stage delays quiescence but may complete without running a Handler.
    assert(runtime.ExecuteOne() == C::ExecutionAttemptResult::NotExecuted);
    cancelled.Release();
    executing.Release();
    handle.Release();
    auto quiescePrepared = outbound.Prepare(remoteRequest);
    assert(quiescePrepared.Accepted());
    auto quiesceStage = std::move(quiescePrepared).TakeReservation();
    assert(runtime.BeginQuiesce() == C::QuiesceResult::Started);
    assert(runtime.State() == C::RuntimeState::Quiescing);
    sequence = 1;
    assert(quiesceStage.Commit(remoteOperation) == 31);
    assert(runtime.State() == C::RuntimeState::Quiescent);

    auto rejected = runtime.Dispatch(EchoRequest{1});
    assert(!rejected.Accepted());
    assert(rejected.Failure() == C::DispatchFailure::RuntimeUnavailable);
    assert(!RuntimeMutexHeld);
    assert(mutex.AcquireCount == mutex.ReleaseCount);
}
