#pragma once

#include <cstddef>

namespace ESPressio::Command {

template<std::size_t TInvocationCapacity, std::size_t TQueueCapacity, std::size_t TExecutionConcurrency>
struct ResourcePlan final {
    static_assert(TInvocationCapacity > 0U, "Command invocation capacity must be positive");
    static_assert(TExecutionConcurrency > 0U, "Command execution concurrency must be positive");
    static_assert(TQueueCapacity + TExecutionConcurrency <= TInvocationCapacity,
        "Command queue capacity plus execution concurrency must not exceed invocation capacity");

    static constexpr std::size_t InvocationCapacity = TInvocationCapacity;
    static constexpr std::size_t QueueCapacity = TQueueCapacity;
    static constexpr std::size_t ExecutionConcurrency = TExecutionConcurrency;

    template<class TRecord>
    static consteval std::size_t InvocationBytes() noexcept { return sizeof(TRecord) * InvocationCapacity; }

    template<class TQueueIndex>
    static consteval std::size_t QueueBytes() noexcept { return sizeof(TQueueIndex) * QueueCapacity; }

    template<class TRecord, class TQueueIndex>
    static consteval std::size_t TotalCoreBytes() noexcept {
        return InvocationBytes<TRecord>() + QueueBytes<TQueueIndex>();
    }
};

} // ESPressio::Command
