#pragma once

#include <cstddef>

namespace ESPressio::Command {

    /// Compile-time bounded resource plan for one Command Runtime.
    ///
    /// @tparam TInvocationCapacity Maximum simultaneously retained invocation records.
    /// @tparam TQueueCapacity Maximum invocations admitted to the execution queue.
    /// @tparam TExecutionConcurrency Maximum invocations permitted to execute concurrently.
    /// @tparam TRemoteHandoffCapacity Maximum simultaneous synchronized outbound handoff stages.
    template<
        std::size_t TInvocationCapacity,
        std::size_t TQueueCapacity,
        std::size_t TExecutionConcurrency,
        std::size_t TRemoteHandoffCapacity = 0U
    >
    struct ResourcePlan final {
        // Compile-time topology validation.

        static_assert(
            TInvocationCapacity > 0U,
            "Command invocation capacity must be positive"
        );

        static_assert(
            TExecutionConcurrency > 0U,
            "Command execution concurrency must be positive"
        );

        static_assert(
            TQueueCapacity + TExecutionConcurrency <= TInvocationCapacity,
            "Command queue capacity plus execution concurrency must not exceed invocation capacity"
        );

        // Planned capacities.

        /// Maximum number of invocation records retained by the Runtime.
        static constexpr std::size_t InvocationCapacity = TInvocationCapacity;

        /// Maximum number of queued invocation indices retained by the Runtime.
        static constexpr std::size_t QueueCapacity = TQueueCapacity;

        /// Maximum number of invocations permitted to execute concurrently.
        static constexpr std::size_t ExecutionConcurrency = TExecutionConcurrency;

        /// Maximum number of concurrent outbound Request borrows staged for bounded adapter calls.
        static constexpr std::size_t RemoteHandoffCapacity = TRemoteHandoffCapacity;

        // Compile-time resource measurement helpers.

        /// Calculates storage occupied by all invocation records.
        ///
        /// @tparam TRecord Runtime invocation-record type being measured.
        template<class TRecord>
        static consteval std::size_t InvocationBytes() noexcept {
            return sizeof(TRecord) * InvocationCapacity;
        }

        /// Calculates storage occupied by the bounded queue indices.
        ///
        /// @tparam TQueueIndex Queue-index type being measured.
        template<class TQueueIndex>
        static consteval std::size_t QueueBytes() noexcept {
            return sizeof(TQueueIndex) * QueueCapacity;
        }

        /// Calculates storage occupied by all synchronized outbound handoff records.
        ///
        /// @tparam TRemoteHandoffRecord Runtime handoff-record type being measured.
        template<class TRemoteHandoffRecord>
        static consteval std::size_t RemoteHandoffBytes() noexcept {
            return sizeof(TRemoteHandoffRecord) * RemoteHandoffCapacity;
        }

        /// Calculates total core invocation-record and queue-index storage.
        ///
        /// @tparam TRecord Runtime invocation-record type being measured.
        /// @tparam TQueueIndex Queue-index type being measured.
        template<class TRecord, class TQueueIndex>
        static consteval std::size_t TotalCoreBytes() noexcept {
            return InvocationBytes<TRecord>() + QueueBytes<TQueueIndex>();
        }
    };

} // ESPressio::Command
