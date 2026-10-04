#include <cstddef>
#include <cstdint>
#include <cstdio>

#include <ESPressio_Command.hpp>

namespace C = ESPressio::Command;
namespace S = ESPressio::System;

struct Request final {
    static constexpr S::TypeIdentifier Identifier{
        S::TypeIdentifier::Storage{
            0x00U, 0xFEU, 0x04U,
            0x00U, 0x00U, 0x00U, 0x00U, 0x01U
        }
    };

    std::uint32_t Value{0U};

    using Fields = S::FieldSet<
        S::FieldBinding<&Request::Value, 0U>
    >;
};

struct Response final {
    static constexpr S::TypeIdentifier Identifier{
        S::TypeIdentifier::Storage{
            0x00U, 0xFEU, 0x04U,
            0x00U, 0x00U, 0x00U, 0x00U, 0x02U
        }
    };

    std::uint32_t Value{0U};

    using Fields = S::FieldSet<
        S::FieldBinding<&Response::Value, 0U>
    >;
};

struct Command final {
    static constexpr S::TypeIdentifier Identifier{
        S::TypeIdentifier::Storage{
            0x00U, 0xFEU, 0x04U,
            0x00U, 0x00U, 0x00U, 0x00U, 0x03U
        }
    };

    using Fields = S::FieldSet<>;
    using Family = C::Family;
    using Request = ::Request;
    using Response = ::Response;
};

static_assert(C::CommandType<Command>);

struct Executor final {
    C::ExecutionResult<Response> Execute(const Request& request, C::CancellationToken) noexcept {
        return C::ExecutionResult<Response>::Succeeded(Response{request.Value});
    }
};

struct MeasurementMutex final {
    [[nodiscard]] ESPressio::Threading::OrdinaryMutexAcquireResult Acquire() noexcept {
        return ESPressio::Threading::OrdinaryMutexAcquireResult::Acquired;
    }

    [[nodiscard]] ESPressio::Threading::OrdinaryMutexReleaseResult Release() noexcept {
        return ESPressio::Threading::OrdinaryMutexReleaseResult::Released;
    }
};

template<std::size_t TCapacity>
struct MeasurementWaitProvider final {
    static constexpr std::size_t Capacity = TCapacity;

    [[nodiscard]] ESPressio::Threading::BoundedWaitWakeResult WaitFor(
        std::size_t slot,
        C::Duration
    ) noexcept {
        return slot < Capacity
            ? ESPressio::Threading::BoundedWaitWakeResult::TimedOut
            : ESPressio::Threading::BoundedWaitWakeResult::InvalidSlot;
    }

    [[nodiscard]] ESPressio::Threading::BoundedWaitWakeResult WaitUntil(
        std::size_t slot,
        C::MonotonicTimestamp
    ) noexcept {
        return slot < Capacity
            ? ESPressio::Threading::BoundedWaitWakeResult::TimedOut
            : ESPressio::Threading::BoundedWaitWakeResult::InvalidSlot;
    }

    [[nodiscard]] bool Wake(std::size_t slot) noexcept { return slot < Capacity; }
};

template<
    std::size_t Invocations,
    std::size_t Queue,
    std::size_t Concurrency,
    std::size_t RemoteHandoffs
>
void Measure(const char* name) {
    using Plan = C::ResourcePlan<Invocations, Queue, Concurrency, RemoteHandoffs>;
    using WaitProvider = MeasurementWaitProvider<Invocations>;
    using Runtime = C::Runtime<Command, Executor, WaitProvider, MeasurementMutex, Plan>;
    std::printf("EDP_COMMAND_RESOURCE plan=%s invocations=%zu queue=%zu concurrency=%zu remote_handoffs=%zu runtime=%zu request=%zu response=%zu\n", name, Invocations, Queue, Concurrency, RemoteHandoffs, sizeof(Runtime), sizeof(Request), sizeof(Response));
}

int main() {
    // ResourcePlan requires QueueCapacity + ExecutionConcurrency <= InvocationCapacity.
    // Keep each measured configuration valid while scaling the bounded topology.
    Measure<2U, 1U, 1U, 0U>("minimum-local");
    Measure<4U, 3U, 1U, 2U>("small-mesh");
    Measure<10U, 8U, 2U, 4U>("representative-mesh");
    Measure<20U, 16U, 4U, 8U>("high-mesh");
    return 0;
}
