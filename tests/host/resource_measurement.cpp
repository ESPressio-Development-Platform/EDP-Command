#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <ESPressio_Command.hpp>

namespace C = ESPressio::Command;

struct Request final { std::uint32_t Value{0U}; };
struct Response final { std::uint32_t Value{0U}; };
struct Command final { using Request = ::Request; using Response = ::Response; };
struct Executor final { C::ExecutionResult<Response> Execute(const Request& request, C::CancellationToken) noexcept { return C::ExecutionResult<Response>::Succeeded(Response{request.Value}); } };

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

template<std::size_t Invocations, std::size_t Queue, std::size_t Concurrency>
void Measure(const char* name) {
    using Plan = C::ResourcePlan<Invocations, Queue, Concurrency>;
    using WaitProvider = MeasurementWaitProvider<Invocations>;
    using Runtime = C::Runtime<Command, Executor, WaitProvider, Plan>;
    std::printf("EDP_COMMAND_RESOURCE plan=%s invocations=%zu queue=%zu concurrency=%zu runtime=%zu request=%zu response=%zu\n", name, Invocations, Queue, Concurrency, sizeof(Runtime), sizeof(Request), sizeof(Response));
}

int main() {
    // ResourcePlan requires QueueCapacity + ExecutionConcurrency <= InvocationCapacity.
    // Keep each measured configuration valid while scaling the bounded topology.
    Measure<2U, 1U, 1U>("minimum");
    Measure<4U, 3U, 1U>("small");
    Measure<10U, 8U, 2U>("representative");
    Measure<20U, 16U, 4U>("high");
    return 0;
}
