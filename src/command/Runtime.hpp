#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <ESPressio_Memory.hpp>
#include <ESPressio_Threading.hpp>

#include "CommandTypes.hpp"
#include "ResourcePlan.hpp"

namespace ESPressio::Command {

    /// Forward declaration of the exclusive invocation Handle.
    /// @tparam TCommand Command declaration represented by the Handle.
    /// @tparam TRuntime Runtime type owning the invocation.
    template<class TCommand, class TRuntime>
    class Handle;

    /// Forward declaration of the typed dispatch result.
    /// @tparam TCommand Command declaration represented by the dispatch.
    /// @tparam TRuntime Runtime type owning an admitted invocation.
    template<class TCommand, class TRuntime>
    class DispatchResult;

    /// Fixed-storage Runtime for one typed Command and its statically resolved providers.
    /// @tparam TCommand Command declaration executed by this Runtime.
    /// @tparam THandlerProvider Application-owned Handler provider for TCommand.
    /// @tparam TWaitProvider Application-owned bounded wait/wake provider.
    /// @tparam TPlan Compile-time Command resource plan.
    template<class TCommand, class THandlerProvider, class TWaitProvider, class TPlan>
    requires CommandType<TCommand>
    class Runtime final {
    private:
        // Private Runtime type vocabulary used by storage declarations.

        /// Request type declared by the Command.
        using RequestType = Request<TCommand>;

        /// Response type declared by the Command.
        using ResponseType = Response<TCommand>;

        /// Compile-time ResourcePlan applied to this Runtime.
        using Plan = TPlan;

        // Invocation record storage and lifecycle state.

        /// Fixed storage and observable state for one invocation slot.
        struct Record final {
            /// Raw storage containing the live Request while RequestLive is true.
            alignas(RequestType) std::byte RequestStorage[sizeof(RequestType)];

            /// Number of bytes reserved for a Response, retaining valid storage for void Responses.
            static constexpr std::size_t ResponseBytes = std::is_void_v<ResponseType> ? 1U : sizeof(ResponseType);

            /// Alignment reserved for a Response, retaining valid alignment for void Responses.
            static constexpr std::size_t ResponseAlignment = std::is_void_v<ResponseType> ? 1U : alignof(ResponseType);

            /// Raw storage containing the live Response while ResponseLive is true.
            alignas(ResponseAlignment) std::byte ResponseStorage[ResponseBytes];

            /// Current invocation lifecycle state.
            InvocationState State{InvocationState::Queued};

            /// Semantic terminal outcome, meaningful after terminal transition.
            Outcome TerminalOutcome{Outcome::Succeeded};

            /// Failure detail, meaningful when TerminalOutcome is Failed.
            ExecutionFailure Failure{ExecutionFailure::ExecutorFailure};

            /// Non-zero reuse generation used to reject stale Handles.
            std::uint32_t Generation{0U};

            /// Indicates whether this record currently belongs to an invocation.
            bool Occupied{false};

            /// Indicates whether RequestStorage contains a live Request.
            bool RequestLive{false};

            /// Indicates whether ResponseStorage contains a live Response.
            bool ResponseLive{false};

            /// Indicates whether the retained Response has already been extracted.
            bool ResponseTaken{false};

            /// Cooperative cancellation predicate observed by the Handler.
            bool CancellationRequested{false};

            /// Indicates whether an exclusive Handle still retains this record.
            bool HandleRetained{false};
        };

        // Fixed Runtime storage and queue bookkeeping.

        /// Fixed invocation-record storage defined by the ResourcePlan.
        std::array<Record, Plan::InvocationCapacity> _records{};

        /// Fixed queue of record indices; one inert element keeps a zero-capacity array well formed.
        std::array<std::size_t, Plan::QueueCapacity == 0U ? 1U : Plan::QueueCapacity> _queue{};

        /// Current queue head index.
        std::size_t _queueHead{0U};

        /// Current queue tail index.
        std::size_t _queueTail{0U};

        /// Number of queued record indices.
        std::size_t _queueCount{0U};

        /// Number of invocations synchronously executing through this Runtime.
        std::size_t _executing{0U};

        // Runtime lifecycle and borrowed provider bindings.

        /// Current Runtime lifecycle state.
        RuntimeState _state{RuntimeState::Uninitialized};

        /// Borrowed application-owned Handler provider.
        THandlerProvider* _handler{nullptr};

        /// Borrowed application-owned bounded wait/wake provider.
        TWaitProvider* _waitProvider{nullptr};

        // Invocation identity and lifecycle helpers.

        /// Returns true when index and generation identify the currently occupied record.
        [[nodiscard]] bool Matches(std::size_t index, std::uint32_t generation) const noexcept {
            return index < _records.size()
                && _records[index].Occupied
                && _records[index].Generation == generation;
        }

        /// Returns true when the supplied invocation state is terminal.
        [[nodiscard]] static bool IsTerminal(InvocationState state) noexcept {
            return state == InvocationState::Completed || state == InvocationState::Cancelled;
        }

        /// Publishes terminal wake-up for the record's stable wait slot.
        void WakeTerminal(std::size_t index) noexcept {
            static_cast<void>(_waitProvider->Wake(index));
        }

        /// Destroys terminal record payloads and frees the slot once Handle retention has ended.
        void ReclaimIfPossible(std::size_t index) noexcept {
            auto& record = _records[index];
            if (!IsTerminal(record.State) || record.HandleRetained) {
                return;
            }

            if (record.RequestLive) {
                ESPressio::Memory::ObjectLifetime::Destroy(
                    *reinterpret_cast<RequestType*>(record.RequestStorage)
                );
                record.RequestLive = false;
            }

            if constexpr (!std::is_void_v<ResponseType>) {
                if (record.ResponseLive) {
                    ESPressio::Memory::ObjectLifetime::Destroy(
                        *reinterpret_cast<ResponseType*>(record.ResponseStorage)
                    );
                    record.ResponseLive = false;
                }
            }

            record.Occupied = false;
            if (_state == RuntimeState::Quiescing && ActiveCount() == 0U) {
                _state = RuntimeState::Quiescent;
            }
        }

        /// Counts currently occupied invocation records, including terminal records retained by Handles.
        [[nodiscard]] std::size_t ActiveCount() const noexcept {
            std::size_t count = 0U;
            for (const auto& record : _records) {
                if (record.Occupied) {
                    ++count;
                }
            }
            return count;
        }

        /// Performs a finite provider wait while preserving authoritative terminal-state precedence.
        /// @tparam TWaitOperation Callable implementing the concrete finite provider wait.
        template<class TWaitOperation>
        [[nodiscard]] WaitResult Wait(
            std::size_t index,
            std::uint32_t generation,
            TWaitOperation operation
        ) noexcept {
            if (!Matches(index, generation)) {
                return WaitResult::InvalidHandle;
            }
            if (IsTerminal(_records[index].State)) {
                return WaitResult::Terminal;
            }

            const auto waitResult = operation(index);
            if (!Matches(index, generation)) {
                return WaitResult::InvalidHandle;
            }
            if (IsTerminal(_records[index].State)) {
                return WaitResult::Terminal;
            }
            if (waitResult == ESPressio::Threading::BoundedWaitWakeResult::TimedOut) {
                return WaitResult::TimedOut;
            }
            return WaitResult::Interrupted;
        }

    public:
        // Public Runtime type vocabulary.

        /// Command declaration executed by this Runtime.
        using Command = TCommand;

        // Construction and lifecycle.

        /// Constructs Runtime wiring by borrowing the resolved application providers.
        Runtime(THandlerProvider& handler, TWaitProvider& waitProvider) noexcept :
            _handler(&handler),
            _waitProvider(&waitProvider) {
        }

        Runtime(const Runtime&) = delete;
        Runtime& operator=(const Runtime&) = delete;

        /// Initializes an uninitialized Runtime and opens Command admission.
        [[nodiscard]] InitializationResult Initialize() noexcept {
            if (_state != RuntimeState::Uninitialized) {
                return InitializationResult::AlreadyInitialized;
            }

            _state = RuntimeState::Running;
            return InitializationResult::Initialized;
        }

        /// Returns the current Runtime lifecycle state.
        [[nodiscard]] RuntimeState State() const noexcept {
            return _state;
        }

        /// Closes admission and begins deterministic Runtime quiescence.
        [[nodiscard]] QuiesceResult BeginQuiesce() noexcept {
            if (_state != RuntimeState::Running) {
                return QuiesceResult::NotRunning;
            }

            _state = ActiveCount() == 0U
                ? RuntimeState::Quiescent
                : RuntimeState::Quiescing;
            return QuiesceResult::Started;
        }

        // Admission and execution.

        /// Attempts to transactionally admit one Request into bounded Runtime storage.
        [[nodiscard]] DispatchResult<TCommand, Runtime> Dispatch(RequestType request) noexcept {
            if (_state != RuntimeState::Running) {
                return DispatchResult<TCommand, Runtime>(DispatchFailure::RuntimeUnavailable);
            }
            if (_queueCount >= Plan::QueueCapacity) {
                return DispatchResult<TCommand, Runtime>(DispatchFailure::NoCapacity);
            }

            std::size_t index = Plan::InvocationCapacity;
            for (std::size_t candidate = 0U; candidate < _records.size(); ++candidate) {
                if (!_records[candidate].Occupied) {
                    index = candidate;
                    break;
                }
            }
            if (index == Plan::InvocationCapacity) {
                return DispatchResult<TCommand, Runtime>(DispatchFailure::NoCapacity);
            }

            auto& record = _records[index];
            ++record.Generation;
            if (record.Generation == 0U) {
                ++record.Generation;
            }
            record.Occupied = true;
            record.State = InvocationState::Queued;
            record.CancellationRequested = false;
            record.ResponseTaken = false;
            record.ResponseLive = false;
            record.HandleRetained = true;
            static_cast<void>(
                ESPressio::Memory::ObjectLifetime::MoveConstruct<RequestType>(
                    record.RequestStorage,
                    request
                )
            );
            record.RequestLive = true;
            _queue[_queueTail] = index;
            _queueTail = (_queueTail + 1U) % _queue.size();
            ++_queueCount;
            return DispatchResult<TCommand, Runtime>(
                *this,
                index,
                record.Generation
            );
        }

        /// Attempts to synchronously execute at most one queued invocation.
        [[nodiscard]] ExecutionAttemptResult ExecuteOne() noexcept {
            if (_queueCount == 0U || _executing >= Plan::ExecutionConcurrency) {
                return ExecutionAttemptResult::NotExecuted;
            }

            const auto index = _queue[_queueHead];
            _queueHead = (_queueHead + 1U) % _queue.size();
            --_queueCount;
            auto& record = _records[index];
            if (!record.Occupied || record.State != InvocationState::Queued) {
                return ExecutionAttemptResult::NotExecuted;
            }

            if (record.CancellationRequested) {
                record.State = InvocationState::Cancelled;
                record.TerminalOutcome = Outcome::Cancelled;
                WakeTerminal(index);
                ReclaimIfPossible(index);
                return ExecutionAttemptResult::Executed;
            }

            record.State = InvocationState::Executing;
            ++_executing;
            auto& request = *reinterpret_cast<RequestType*>(record.RequestStorage);
            CancellationToken cancellation(record.CancellationRequested);
            auto result = _handler->Execute(
                request,
                cancellation
            );
            --_executing;

            if (record.CancellationRequested) {
                record.State = InvocationState::Cancelled;
                record.TerminalOutcome = Outcome::Cancelled;
            } else {
                record.TerminalOutcome = result.GetOutcome();
                record.Failure = result.Failure();
                if constexpr (!std::is_void_v<ResponseType>) {
                    if (result.GetOutcome() != Outcome::Failed && result.HasResponse()) {
                        auto response = result.TakeResponse();
                        static_cast<void>(
                            ESPressio::Memory::ObjectLifetime::MoveConstruct<ResponseType>(
                                record.ResponseStorage,
                                response
                            )
                        );
                        record.ResponseLive = true;
                    }
                }
                record.State = InvocationState::Completed;
            }

            WakeTerminal(index);
            ReclaimIfPossible(index);
            return ExecutionAttemptResult::Executed;
        }

        // Invocation observation and finite waiting.

        /// Returns an observable value snapshot for a matching invocation identity.
        [[nodiscard]] InvocationObservation Observe(
            std::size_t index,
            std::uint32_t generation,
            bool& valid
        ) const noexcept {
            valid = Matches(index, generation);
            if (!valid) {
                return {};
            }

            const auto& record = _records[index];
            InvocationObservation observation{};
            observation.State = record.State;
            if (IsTerminal(record.State)) {
                observation.HasOutcome = true;
                observation.TerminalOutcome = record.TerminalOutcome;
                observation.HasFailure = record.TerminalOutcome == Outcome::Failed;
                observation.Failure = record.Failure;
            }
            return observation;
        }

        /// Performs a finite wait for the supplied duration.
        [[nodiscard]] WaitResult WaitFor(
            std::size_t index,
            std::uint32_t generation,
            Duration duration
        ) noexcept {
            return Wait(
                index,
                generation,
                [&](std::size_t slot) noexcept {
                    return _waitProvider->WaitFor(
                        slot,
                        duration
                    );
                }
            );
        }

        /// Performs a finite wait until the supplied monotonic deadline.
        [[nodiscard]] WaitResult WaitUntil(
            std::size_t index,
            std::uint32_t generation,
            MonotonicTimestamp deadline
        ) noexcept {
            return Wait(
                index,
                generation,
                [&](std::size_t slot) noexcept {
                    return _waitProvider->WaitUntil(
                        slot,
                        deadline
                    );
                }
            );
        }

        // Cancellation and response ownership.

        /// Requests cooperative cancellation for a matching invocation identity.
        [[nodiscard]] CancellationRequestResult RequestCancellation(
            std::size_t index,
            std::uint32_t generation
        ) noexcept {
            if (!Matches(index, generation)) {
                return CancellationRequestResult::InvalidHandle;
            }

            auto& record = _records[index];
            if (IsTerminal(record.State)) {
                return CancellationRequestResult::TooLate;
            }
            if (record.CancellationRequested) {
                return CancellationRequestResult::AlreadyRequested;
            }

            record.CancellationRequested = true;
            if (record.State == InvocationState::Queued) {
                record.State = InvocationState::Cancelled;
                record.TerminalOutcome = Outcome::Cancelled;
                WakeTerminal(index);
            }
            return CancellationRequestResult::Requested;
        }

        /// Transfers a retained non-void Response into caller-provided raw storage.
        /// @tparam TResponseValue Response type selected for extraction.
        template<class TResponseValue = ResponseType>
        requires (!std::is_void_v<TResponseValue>)
        [[nodiscard]] TakeResponseStatus TakeResponse(
            std::size_t index,
            std::uint32_t generation,
            void* destination
        ) noexcept {
            if (!Matches(index, generation)) {
                return TakeResponseStatus::InvalidHandle;
            }

            auto& record = _records[index];
            if (!IsTerminal(record.State)) {
                return TakeResponseStatus::NotTerminal;
            }
            if (!record.ResponseLive) {
                return TakeResponseStatus::NoResponse;
            }
            if (record.ResponseTaken) {
                return TakeResponseStatus::AlreadyTaken;
            }

            auto& response = *reinterpret_cast<TResponseValue*>(record.ResponseStorage);
            static_cast<void>(
                ESPressio::Memory::ObjectLifetime::MoveConstruct<TResponseValue>(
                    destination,
                    response
                )
            );
            ESPressio::Memory::ObjectLifetime::Destroy(response);
            record.ResponseLive = false;
            record.ResponseTaken = true;
            return TakeResponseStatus::Taken;
        }

        // Handle retention.

        /// Releases Handle retention for a matching invocation and reclaims it when terminal.
        void Release(std::size_t index, std::uint32_t generation) noexcept {
            if (!Matches(index, generation)) {
                return;
            }

            _records[index].HandleRetained = false;
            ReclaimIfPossible(index);
        }
    };

} // ESPressio::Command
