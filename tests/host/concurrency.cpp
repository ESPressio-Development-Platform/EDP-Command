#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <thread>

#include <ESPressio_Command.hpp>

namespace C = ESPressio::Command;
namespace T = ESPressio::Threading;
namespace S = ESPressio::System;
namespace CF = ESPressio::System::CompositionFramework;

struct ConcurrentRequest final {
    static constexpr S::TypeIdentifier Identifier{
        S::TypeIdentifier::Storage{
            0x00U, 0xFEU, 0x05U,
            0x00U, 0x00U, 0x00U, 0x00U, 0x01U
        }
    };

    std::int32_t Value{0};

    using Fields = S::FieldSet<
        S::FieldBinding<&ConcurrentRequest::Value, 0U>
    >;
};

struct ConcurrentResponse final {
    static constexpr S::TypeIdentifier Identifier{
        S::TypeIdentifier::Storage{
            0x00U, 0xFEU, 0x05U,
            0x00U, 0x00U, 0x00U, 0x00U, 0x02U
        }
    };

    std::int32_t Value{0};

    using Fields = S::FieldSet<
        S::FieldBinding<&ConcurrentResponse::Value, 0U>
    >;
};

struct ConcurrentCommand final {
    static constexpr S::TypeIdentifier Identifier{
        S::TypeIdentifier::Storage{
            0x00U, 0xFEU, 0x05U,
            0x00U, 0x00U, 0x00U, 0x00U, 0x03U
        }
    };

    using Fields = S::FieldSet<>;
    using Family = C::Family;
    using Request = ConcurrentRequest;
    using Response = ConcurrentResponse;
};

struct ConcurrentHandler final : CF::Provider<
    C::Composition::Domain,
    CF::Offers<CF::Offer<C::Composition::Handler<ConcurrentCommand>>>
> {
    std::atomic_uint32_t Entered{0U};
    std::atomic_bool Release{false};

    C::ExecutionResult<ConcurrentResponse> Execute(
        const ConcurrentRequest& request,
        C::CancellationToken cancellation
    ) noexcept {
        Entered.fetch_add(1U, std::memory_order_acq_rel);
        while (!Release.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        if (cancellation.IsRequested()) {
            return C::ExecutionResult<ConcurrentResponse>::Failed();
        }
        return C::ExecutionResult<ConcurrentResponse>::Succeeded(
            ConcurrentResponse{request.Value}
        );
    }
};

struct ConcurrentWaitProvider final : CF::Provider<
    T::Domain,
    CF::Offers<
        CF::Offer<
            T::BoundedWaitWake,
            CF::PropertyValue<T::BoundedWaitWakeCapacity, 4U>
        >
    >
> {
    static constexpr std::size_t Capacity = 4U;
    std::atomic_uint32_t WakeCount{0U};

    [[nodiscard]] T::BoundedWaitWakeResult WaitFor(std::size_t slot, C::Duration) noexcept {
        return slot < Capacity
            ? T::BoundedWaitWakeResult::TimedOut
            : T::BoundedWaitWakeResult::InvalidSlot;
    }

    [[nodiscard]] T::BoundedWaitWakeResult WaitUntil(
        std::size_t slot,
        C::MonotonicTimestamp
    ) noexcept {
        return slot < Capacity
            ? T::BoundedWaitWakeResult::TimedOut
            : T::BoundedWaitWakeResult::InvalidSlot;
    }

    [[nodiscard]] T::BoundedWaitWakeResult Wake(std::size_t slot) noexcept {
        if (slot >= Capacity) {
            return T::BoundedWaitWakeResult::InvalidSlot;
        }
        WakeCount.fetch_add(1U, std::memory_order_acq_rel);
        return T::BoundedWaitWakeResult::Woken;
    }
};

struct ConcurrentMutex final : CF::Provider<
    T::Domain,
    CF::Offers<CF::Offer<T::OrdinaryMutex<C::Composition::RuntimeMutexIdentity>>>
> {
    std::mutex Native;

    [[nodiscard]] T::OrdinaryMutexAcquireResult Acquire() noexcept {
        Native.lock();
        return T::OrdinaryMutexAcquireResult::Acquired;
    }

    [[nodiscard]] T::OrdinaryMutexReleaseResult Release() noexcept {
        Native.unlock();
        return T::OrdinaryMutexReleaseResult::Released;
    }
};

using CommandComposition = CF::Composition<C::Composition::Domain, ConcurrentHandler>;
using ThreadingComposition = CF::Composition<
    T::Domain,
    ConcurrentWaitProvider,
    ConcurrentMutex
>;
using Architecture = CF::Architecture<CommandComposition, ThreadingComposition>;
using Plan = C::ResourcePlan<4U, 2U, 2U, 2U>;
using Bootstrap = C::Bootstrap<ConcurrentCommand, Architecture, Plan>;

int main() {
    ConcurrentHandler handler;
    ConcurrentWaitProvider waits;
    ConcurrentMutex mutex;
    Bootstrap bootstrap(handler, waits, mutex);
    auto& runtime = bootstrap.RuntimeInstance();

    assert(bootstrap.Initialize() == C::InitializationResult::Initialized);

    auto firstDispatch = runtime.Dispatch(ConcurrentRequest{11});
    auto secondDispatch = runtime.Dispatch(ConcurrentRequest{22});
    assert(firstDispatch.Accepted());
    assert(secondDispatch.Accepted());
    auto first = firstDispatch.TakeHandle();
    auto second = secondDispatch.TakeHandle();

    C::ExecutionAttemptResult firstExecution{C::ExecutionAttemptResult::NotExecuted};
    C::ExecutionAttemptResult secondExecution{C::ExecutionAttemptResult::NotExecuted};
    std::thread firstWorker([&]() noexcept {
        firstExecution = runtime.ExecuteOne();
    });
    std::thread secondWorker([&]() noexcept {
        secondExecution = runtime.ExecuteOne();
    });

    std::size_t spinCount = 0U;
    while (
        handler.Entered.load(std::memory_order_acquire) != 2U &&
        spinCount < 10'000'000U
    ) {
        ++spinCount;
        std::this_thread::yield();
    }

    const bool bothHandlersEntered =
        handler.Entered.load(std::memory_order_acquire) == 2U;
    if (bothHandlersEntered) {
        assert(first.RequestCancellation() == C::CancellationRequestResult::Requested);
    }
    handler.Release.store(true, std::memory_order_release);
    firstWorker.join();
    secondWorker.join();

    assert(bothHandlersEntered);
    assert(firstExecution == C::ExecutionAttemptResult::Executed);
    assert(secondExecution == C::ExecutionAttemptResult::Executed);
    assert(waits.WakeCount.load(std::memory_order_acquire) == 2U);

    bool valid = false;
    const auto firstState = first.State(valid);
    assert(valid);
    assert(firstState.WasCancelled());
    const auto secondState = second.State(valid);
    assert(valid);
    assert(secondState.DidSucceed());
    auto response = second.TakeResponse();
    assert(response.HasValue());
    assert(response.Take().Value == 22);

    first.Release();
    second.Release();
    assert(runtime.BeginQuiesce() == C::QuiesceResult::Started);
    assert(runtime.State() == C::RuntimeState::Quiescent);
}
