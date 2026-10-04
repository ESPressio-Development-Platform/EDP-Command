#pragma once

#include <array>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <type_traits>
#include <utility>

#include <ESPressio_Memory.hpp>
#include <ESPressio_Threading.hpp>

#include "CommandTypes.hpp"
#include "Handle.hpp"
#include "ResourcePlan.hpp"

namespace ESPressio::Command {

    /// Fixed-storage, externally scheduled Runtime for one typed Command.
    ///
    /// Every short mutation is serialized by the application-owned ordinary mutex. Handler execution,
    /// finite waits, wake publication and external callbacks occur outside that serialization domain.
    template<
        class TCommand,
        class THandlerProvider,
        class TWaitProvider,
        class TMutexProvider,
        class TPlan
    >
    requires CommandType<TCommand>
    class Runtime final {
    private:
        using RequestType = Request<TCommand>;
        using ResponseType = Response<TCommand>;
        using Plan = TPlan;

        struct Record final {
            alignas(RequestType) std::byte RequestStorage[sizeof(RequestType)];

            static constexpr std::size_t ResponseBytes =
                std::is_void_v<ResponseType> ? 1U : sizeof(ResponseType);
            static constexpr std::size_t ResponseAlignment =
                std::is_void_v<ResponseType> ? 1U : alignof(ResponseType);

            alignas(ResponseAlignment) std::byte ResponseStorage[ResponseBytes];

            InvocationState State{InvocationState::Reserved};
            Outcome TerminalOutcome{Outcome::Succeeded};
            ExecutionFailure Failure{ExecutionFailure::ExecutorFailure};
            std::uint32_t Generation{0U};
            std::atomic_bool CancellationRequested{false};
            bool Occupied{false};
            bool RequestLive{false};
            bool ResponseLive{false};
            bool ResponseTaken{false};
            bool HandleRetained{false};
            bool QueuedInRing{false};
            bool TerminalWakePending{false};
        };

        struct RemoteHandoffRecord final {
            const RequestType* RequestView{nullptr};
            std::uint32_t Generation{0U};
            bool Occupied{false};
        };

        struct EmptyRemoteHandoffStorage final {};

        using RemoteHandoffStorage = std::conditional_t<
            Plan::RemoteHandoffCapacity == 0U,
            EmptyRemoteHandoffStorage,
            std::array<RemoteHandoffRecord, Plan::RemoteHandoffCapacity>
        >;

        std::array<Record, Plan::InvocationCapacity> _records{};
        [[no_unique_address]] RemoteHandoffStorage _remoteHandoffs{};
        std::array<std::size_t, Plan::QueueCapacity == 0U ? 1U : Plan::QueueCapacity> _queue{};
        std::size_t _queueHead{0U};
        std::size_t _queueTail{0U};
        std::size_t _queueCount{0U};
        std::size_t _reservedQueueCount{0U};
        std::size_t _executing{0U};
        std::size_t _remoteHandoffCount{0U};

        RuntimeState _state{RuntimeState::Uninitialized};
        THandlerProvider* _handler{nullptr};
        TWaitProvider* _waitProvider{nullptr};
        TMutexProvider* _mutex{nullptr};

        static_assert(
            requires(TMutexProvider& provider) {
                { provider.Acquire() } noexcept -> std::same_as<Threading::OrdinaryMutexAcquireResult>;
                { provider.Release() } noexcept -> std::same_as<Threading::OrdinaryMutexReleaseResult>;
            },
            "Command Runtime requires an EDP-Threading ordinary mutex provider"
        );

        [[noreturn]] static void InfrastructureFailure() noexcept {
            std::terminate();
        }

        void Lock() const noexcept {
            if (_mutex->Acquire() != Threading::OrdinaryMutexAcquireResult::Acquired) {
                InfrastructureFailure();
            }
        }

        void Unlock() const noexcept {
            if (_mutex->Release() != Threading::OrdinaryMutexReleaseResult::Released) {
                InfrastructureFailure();
            }
        }

        [[nodiscard]] bool MatchesLocked(
            std::size_t index,
            std::uint32_t generation
        ) const noexcept {
            return index < _records.size()
                && _records[index].Occupied
                && _records[index].Generation == generation;
        }

        [[nodiscard]] bool MatchesRemoteHandoffLocked(
            std::size_t index,
            std::uint32_t generation
        ) const noexcept {
            return index < Plan::RemoteHandoffCapacity
                && RemoteHandoffAt(index).Occupied
                && RemoteHandoffAt(index).Generation == generation;
        }

        [[nodiscard]] RemoteHandoffRecord& RemoteHandoffAt(std::size_t index) noexcept {
            if constexpr (Plan::RemoteHandoffCapacity == 0U) {
                InfrastructureFailure();
            } else {
                return _remoteHandoffs[index];
            }
        }

        [[nodiscard]] const RemoteHandoffRecord& RemoteHandoffAt(
            std::size_t index
        ) const noexcept {
            if constexpr (Plan::RemoteHandoffCapacity == 0U) {
                InfrastructureFailure();
            } else {
                return _remoteHandoffs[index];
            }
        }

        [[nodiscard]] static bool IsTerminal(InvocationState state) noexcept {
            return state == InvocationState::Completed || state == InvocationState::Cancelled;
        }

        [[nodiscard]] std::size_t ActiveCountLocked() const noexcept {
            std::size_t count = 0U;
            for (const auto& record : _records) {
                if (record.Occupied) {
                    ++count;
                }
            }
            return count + _remoteHandoffCount;
        }

        [[nodiscard]] std::size_t ReserveRecordLocked() noexcept {
            for (std::size_t index = 0U; index < _records.size(); ++index) {
                auto& record = _records[index];
                if (
                    record.Occupied ||
                    record.Generation == std::numeric_limits<std::uint32_t>::max()
                ) {
                    continue;
                }

                ++record.Generation;
                record.Occupied = true;
                record.State = InvocationState::Reserved;
                record.TerminalOutcome = Outcome::Succeeded;
                record.Failure = ExecutionFailure::ExecutorFailure;
                record.CancellationRequested.store(false, std::memory_order_release);
                record.RequestLive = false;
                record.ResponseLive = false;
                record.ResponseTaken = false;
                record.HandleRetained = false;
                record.QueuedInRing = false;
                record.TerminalWakePending = false;
                return index;
            }

            return Plan::InvocationCapacity;
        }

        [[nodiscard]] std::size_t ReserveRemoteHandoffLocked(
            const RequestType& request
        ) noexcept {
            if constexpr (Plan::RemoteHandoffCapacity != 0U) {
                for (std::size_t index = 0U; index < Plan::RemoteHandoffCapacity; ++index) {
                    auto& record = RemoteHandoffAt(index);
                    if (
                        record.Occupied ||
                        record.Generation == std::numeric_limits<std::uint32_t>::max()
                    ) {
                        continue;
                    }

                    ++record.Generation;
                    record.RequestView = &request;
                    record.Occupied = true;
                    ++_remoteHandoffCount;
                    return index;
                }
            }

            return Plan::RemoteHandoffCapacity;
        }

        void ReleaseRemoteHandoffLocked(std::size_t index) noexcept {
            auto& record = RemoteHandoffAt(index);
            record.RequestView = nullptr;
            record.Occupied = false;
            --_remoteHandoffCount;
            if (_state == RuntimeState::Quiescing && ActiveCountLocked() == 0U) {
                _state = RuntimeState::Quiescent;
            }
        }

        void ReclaimIfPossibleLocked(std::size_t index) noexcept {
            auto& record = _records[index];
            if (
                !IsTerminal(record.State) ||
                record.HandleRetained ||
                record.QueuedInRing ||
                record.TerminalWakePending
            ) {
                return;
            }

            if (record.RequestLive) {
                Memory::ObjectLifetime::Destroy(
                    *reinterpret_cast<RequestType*>(record.RequestStorage)
                );
                record.RequestLive = false;
            }

            if constexpr (!std::is_void_v<ResponseType>) {
                if (record.ResponseLive) {
                    Memory::ObjectLifetime::Destroy(
                        *reinterpret_cast<ResponseType*>(record.ResponseStorage)
                    );
                    record.ResponseLive = false;
                }
            }

            record.Occupied = false;
            if (_state == RuntimeState::Quiescing && ActiveCountLocked() == 0U) {
                _state = RuntimeState::Quiescent;
            }
        }

        void AbortIngressLocked(std::size_t index) noexcept {
            auto& record = _records[index];
            if (record.RequestLive) {
                Memory::ObjectLifetime::Destroy(
                    *reinterpret_cast<RequestType*>(record.RequestStorage)
                );
                record.RequestLive = false;
            }

            record.Occupied = false;
            --_reservedQueueCount;
            if (_state == RuntimeState::Quiescing && ActiveCountLocked() == 0U) {
                _state = RuntimeState::Quiescent;
            }
        }

        void WakeTerminal(std::size_t index) noexcept {
            static_cast<void>(_waitProvider->Wake(index));
        }

        void CompleteTerminalWake(
            std::size_t index,
            std::uint32_t generation
        ) noexcept {
            Lock();
            if (!MatchesLocked(index, generation)) {
                InfrastructureFailure();
            }
            _records[index].TerminalWakePending = false;
            ReclaimIfPossibleLocked(index);
            Unlock();
        }

        template<class TWaitOperation>
        [[nodiscard]] WaitResult Wait(
            std::size_t index,
            std::uint32_t generation,
            TWaitOperation operation
        ) noexcept {
            Lock();
            if (!MatchesLocked(index, generation)) {
                Unlock();
                return WaitResult::InvalidHandle;
            }
            if (IsTerminal(_records[index].State)) {
                Unlock();
                return WaitResult::Terminal;
            }
            Unlock();

            const auto waitResult = operation(index);

            Lock();
            if (!MatchesLocked(index, generation)) {
                Unlock();
                return WaitResult::InvalidHandle;
            }
            if (IsTerminal(_records[index].State)) {
                Unlock();
                return WaitResult::Terminal;
            }
            Unlock();

            return waitResult == Threading::BoundedWaitWakeResult::TimedOut
                ? WaitResult::TimedOut
                : WaitResult::Interrupted;
        }

    public:
        using Command = TCommand;
        using RequestValue = RequestType;
        using ResponseValue = ResponseType;
        using ResourcePlanType = Plan;

        Runtime(
            THandlerProvider& handler,
            TWaitProvider& waitProvider,
            TMutexProvider& mutex
        ) noexcept :
            _handler(&handler),
            _waitProvider(&waitProvider),
            _mutex(&mutex) {
        }

        Runtime(const Runtime&) = delete;
        Runtime& operator=(const Runtime&) = delete;
        Runtime(Runtime&&) = delete;
        Runtime& operator=(Runtime&&) = delete;

        [[nodiscard]] InitializationResult Initialize() noexcept {
            Lock();
            if (_state != RuntimeState::Uninitialized) {
                Unlock();
                return InitializationResult::AlreadyInitialized;
            }
            _state = RuntimeState::Running;
            Unlock();
            return InitializationResult::Initialized;
        }

        [[nodiscard]] RuntimeState State() const noexcept {
            Lock();
            const auto state = _state;
            Unlock();
            return state;
        }

        [[nodiscard]] QuiesceResult BeginQuiesce() noexcept {
            Lock();
            if (_state != RuntimeState::Running) {
                Unlock();
                return QuiesceResult::NotRunning;
            }

            _state = ActiveCountLocked() == 0U
                ? RuntimeState::Quiescent
                : RuntimeState::Quiescing;
            Unlock();
            return QuiesceResult::Started;
        }

        [[nodiscard]] DispatchResult<TCommand, Runtime> Dispatch(RequestType request) noexcept {
            Lock();
            if (_state != RuntimeState::Running) {
                Unlock();
                return DispatchResult<TCommand, Runtime>(DispatchFailure::RuntimeUnavailable);
            }
            if (_queueCount + _reservedQueueCount >= Plan::QueueCapacity) {
                Unlock();
                return DispatchResult<TCommand, Runtime>(DispatchFailure::NoCapacity);
            }

            const auto index = ReserveRecordLocked();
            if (index == Plan::InvocationCapacity) {
                Unlock();
                return DispatchResult<TCommand, Runtime>(DispatchFailure::NoCapacity);
            }

            auto& record = _records[index];
            static_cast<void>(
                Memory::ObjectLifetime::MoveConstruct<RequestType>(
                    record.RequestStorage,
                    request
                )
            );
            record.RequestLive = true;
            record.State = InvocationState::Queued;
            record.HandleRetained = true;
            record.QueuedInRing = true;
            _queue[_queueTail] = index;
            _queueTail = (_queueTail + 1U) % _queue.size();
            ++_queueCount;
            const auto generation = record.Generation;
            Unlock();

            return DispatchResult<TCommand, Runtime>(*this, index, generation);
        }

        [[nodiscard]] InboundReservationResult<TCommand, Runtime> PrepareIngress() noexcept
        requires std::is_nothrow_default_constructible_v<RequestType> {
            Lock();
            if (_state != RuntimeState::Running) {
                Unlock();
                return InboundReservationResult<TCommand, Runtime>(
                    DispatchFailure::RuntimeUnavailable
                );
            }
            if (_queueCount + _reservedQueueCount >= Plan::QueueCapacity) {
                Unlock();
                return InboundReservationResult<TCommand, Runtime>(DispatchFailure::NoCapacity);
            }

            const auto index = ReserveRecordLocked();
            if (index == Plan::InvocationCapacity) {
                Unlock();
                return InboundReservationResult<TCommand, Runtime>(DispatchFailure::NoCapacity);
            }

            auto& record = _records[index];
            static_cast<void>(
                Memory::ObjectLifetime::Construct<RequestType>(record.RequestStorage)
            );
            record.RequestLive = true;
            ++_reservedQueueCount;
            const auto generation = record.Generation;
            Unlock();

            return InboundReservationResult<TCommand, Runtime>(
                *this,
                index,
                generation
            );
        }

        [[nodiscard]] RequestType& IngressRequest(
            std::size_t index,
            std::uint32_t generation
        ) noexcept {
            if (
                index >= _records.size() ||
                !_records[index].Occupied ||
                _records[index].Generation != generation ||
                _records[index].State != InvocationState::Reserved ||
                !_records[index].RequestLive
            ) {
                InfrastructureFailure();
            }

            return *reinterpret_cast<RequestType*>(_records[index].RequestStorage);
        }

        [[nodiscard]] DispatchResult<TCommand, Runtime> CommitIngress(
            std::size_t index,
            std::uint32_t generation
        ) noexcept {
            Lock();
            if (
                !MatchesLocked(index, generation) ||
                _records[index].State != InvocationState::Reserved
            ) {
                Unlock();
                return DispatchResult<TCommand, Runtime>(DispatchFailure::BindingUnavailable);
            }

            if (_state != RuntimeState::Running) {
                AbortIngressLocked(index);
                Unlock();
                return DispatchResult<TCommand, Runtime>(DispatchFailure::RuntimeUnavailable);
            }

            auto& record = _records[index];
            --_reservedQueueCount;
            record.State = InvocationState::Queued;
            record.HandleRetained = true;
            record.QueuedInRing = true;
            _queue[_queueTail] = index;
            _queueTail = (_queueTail + 1U) % _queue.size();
            ++_queueCount;
            Unlock();

            return DispatchResult<TCommand, Runtime>(*this, index, generation);
        }

        void AbortIngress(
            std::size_t index,
            std::uint32_t generation
        ) noexcept {
            Lock();
            if (
                MatchesLocked(index, generation) &&
                _records[index].State == InvocationState::Reserved
            ) {
                AbortIngressLocked(index);
            }
            Unlock();
        }

        /// Reserves one bounded synchronized stage for a caller-owned outbound Request borrow.
        [[nodiscard]] RemoteHandoffReservationResult<TCommand, Runtime> PrepareRemoteHandoff(
            const RequestType& request
        ) noexcept {
            Lock();
            if (_state != RuntimeState::Running) {
                Unlock();
                return RemoteHandoffReservationResult<TCommand, Runtime>(
                    DispatchFailure::RuntimeUnavailable
                );
            }

            const auto index = ReserveRemoteHandoffLocked(request);
            if (index == Plan::RemoteHandoffCapacity) {
                Unlock();
                return RemoteHandoffReservationResult<TCommand, Runtime>(
                    DispatchFailure::NoCapacity
                );
            }

            const auto generation = RemoteHandoffAt(index).Generation;
            Unlock();
            return RemoteHandoffReservationResult<TCommand, Runtime>(
                *this,
                index,
                generation
            );
        }

        RemoteHandoffReservationResult<TCommand, Runtime> PrepareRemoteHandoff(
            RequestType&&
        ) noexcept = delete;

        /// Calls the selected bounded remote adapter outside Command synchronization and releases the stage.
        template<class TRemoteOperation>
        [[nodiscard]] auto CommitRemoteHandoff(
            std::size_t index,
            std::uint32_t generation,
            TRemoteOperation& remoteOperation
        ) noexcept {
            using RemoteResult = decltype(
                remoteOperation(std::declval<const RequestType&>())
            );
            static_assert(
                noexcept(remoteOperation(std::declval<const RequestType&>())),
                "Command remote handoff must be non-throwing"
            );
            static_assert(
                !std::is_void_v<RemoteResult> &&
                std::is_nothrow_move_constructible_v<RemoteResult> &&
                std::is_nothrow_destructible_v<RemoteResult>,
                "Command remote handoff requires a non-throwing move-only-capable result"
            );

            Lock();
            if (!MatchesRemoteHandoffLocked(index, generation)) {
                InfrastructureFailure();
            }
            const auto* request = RemoteHandoffAt(index).RequestView;
            Unlock();

            auto result = remoteOperation(*request);

            Lock();
            if (!MatchesRemoteHandoffLocked(index, generation)) {
                InfrastructureFailure();
            }
            ReleaseRemoteHandoffLocked(index);
            Unlock();
            return result;
        }

        /// Releases one valid outbound stage without invoking its adapter.
        void AbortRemoteHandoff(
            std::size_t index,
            std::uint32_t generation
        ) noexcept {
            Lock();
            if (MatchesRemoteHandoffLocked(index, generation)) {
                ReleaseRemoteHandoffLocked(index);
            }
            Unlock();
        }

        [[nodiscard]] ExecutionAttemptResult ExecuteOne() noexcept {
            Lock();
            if (_queueCount == 0U || _executing >= Plan::ExecutionConcurrency) {
                Unlock();
                return ExecutionAttemptResult::NotExecuted;
            }

            const auto index = _queue[_queueHead];
            _queueHead = (_queueHead + 1U) % _queue.size();
            --_queueCount;
            auto& record = _records[index];
            record.QueuedInRing = false;

            if (!record.Occupied || record.State != InvocationState::Queued) {
                ReclaimIfPossibleLocked(index);
                Unlock();
                return ExecutionAttemptResult::NotExecuted;
            }

            record.State = InvocationState::Executing;
            ++_executing;
            const auto generation = record.Generation;
            auto* request = reinterpret_cast<RequestType*>(record.RequestStorage);
            CancellationToken cancellation(record.CancellationRequested);
            Unlock();

            auto result = _handler->Execute(*request, cancellation);

            Lock();
            if (
                !MatchesLocked(index, generation) ||
                _records[index].State != InvocationState::Executing ||
                _executing == 0U
            ) {
                InfrastructureFailure();
            }
            --_executing;

            auto& completed = _records[index];
            if (completed.CancellationRequested.load(std::memory_order_acquire)) {
                completed.State = InvocationState::Cancelled;
                completed.TerminalOutcome = Outcome::Cancelled;
            } else {
                completed.TerminalOutcome = result.GetOutcome();
                completed.Failure = result.Failure();
                if constexpr (!std::is_void_v<ResponseType>) {
                    if (result.GetOutcome() != Outcome::Failed && result.HasResponse()) {
                        auto response = result.TakeResponse();
                        static_cast<void>(
                            Memory::ObjectLifetime::MoveConstruct<ResponseType>(
                                completed.ResponseStorage,
                                response
                            )
                        );
                        completed.ResponseLive = true;
                    }
                }
                completed.State = InvocationState::Completed;
            }
            completed.TerminalWakePending = true;
            Unlock();

            WakeTerminal(index);
            CompleteTerminalWake(index, generation);
            return ExecutionAttemptResult::Executed;
        }

        [[nodiscard]] InvocationObservation Observe(
            std::size_t index,
            std::uint32_t generation,
            bool& valid
        ) const noexcept {
            Lock();
            valid = MatchesLocked(index, generation);
            if (!valid) {
                Unlock();
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
            Unlock();
            return observation;
        }

        [[nodiscard]] WaitResult WaitFor(
            std::size_t index,
            std::uint32_t generation,
            Duration duration
        ) noexcept {
            return Wait(
                index,
                generation,
                [&](std::size_t slot) noexcept {
                    return _waitProvider->WaitFor(slot, duration);
                }
            );
        }

        [[nodiscard]] WaitResult WaitUntil(
            std::size_t index,
            std::uint32_t generation,
            MonotonicTimestamp deadline
        ) noexcept {
            return Wait(
                index,
                generation,
                [&](std::size_t slot) noexcept {
                    return _waitProvider->WaitUntil(slot, deadline);
                }
            );
        }

        [[nodiscard]] CancellationRequestResult RequestCancellation(
            std::size_t index,
            std::uint32_t generation
        ) noexcept {
            bool publishWake = false;

            Lock();
            if (!MatchesLocked(index, generation)) {
                Unlock();
                return CancellationRequestResult::InvalidHandle;
            }

            auto& record = _records[index];
            if (IsTerminal(record.State)) {
                Unlock();
                return CancellationRequestResult::TooLate;
            }
            if (record.CancellationRequested.exchange(true, std::memory_order_acq_rel)) {
                Unlock();
                return CancellationRequestResult::AlreadyRequested;
            }

            if (record.State == InvocationState::Queued) {
                record.State = InvocationState::Cancelled;
                record.TerminalOutcome = Outcome::Cancelled;
                record.TerminalWakePending = true;
                publishWake = true;
            }
            Unlock();

            if (publishWake) {
                WakeTerminal(index);
                CompleteTerminalWake(index, generation);
            }
            return CancellationRequestResult::Requested;
        }

        template<class TResponseValue = ResponseType>
        requires (!std::is_void_v<TResponseValue>)
        [[nodiscard]] TakeResponseStatus TakeResponse(
            std::size_t index,
            std::uint32_t generation,
            void* destination
        ) noexcept {
            Lock();
            if (!MatchesLocked(index, generation)) {
                Unlock();
                return TakeResponseStatus::InvalidHandle;
            }

            auto& record = _records[index];
            if (!IsTerminal(record.State)) {
                Unlock();
                return TakeResponseStatus::NotTerminal;
            }
            if (!record.ResponseLive) {
                Unlock();
                return TakeResponseStatus::NoResponse;
            }
            if (record.ResponseTaken) {
                Unlock();
                return TakeResponseStatus::AlreadyTaken;
            }

            auto& response = *reinterpret_cast<TResponseValue*>(record.ResponseStorage);
            static_cast<void>(
                Memory::ObjectLifetime::MoveConstruct<TResponseValue>(destination, response)
            );
            Memory::ObjectLifetime::Destroy(response);
            record.ResponseLive = false;
            record.ResponseTaken = true;
            Unlock();
            return TakeResponseStatus::Taken;
        }

        void Release(std::size_t index, std::uint32_t generation) noexcept {
            Lock();
            if (MatchesLocked(index, generation)) {
                _records[index].HandleRetained = false;
                ReclaimIfPossibleLocked(index);
            }
            Unlock();
        }
    };

} // ESPressio::Command
