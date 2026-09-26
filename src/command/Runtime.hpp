#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <ESPressio_Memory.hpp>

#include "CommandTypes.hpp"
#include "ResourcePlan.hpp"

namespace ESPressio::Command {

template<class TCommand, class TRuntime> class Handle;
template<class TCommand, class TRuntime> class DispatchResult;

template<class TCommand, class TExecutor, class TPlan>
requires CommandType<TCommand>
class Runtime final {
public:
    using Command = TCommand;
    using RequestType = Request<TCommand>;
    using ResponseType = Response<TCommand>;
    using Plan = TPlan;

private:
    struct Record final {
        alignas(RequestType) std::byte RequestStorage[sizeof(RequestType)];
        static constexpr std::size_t ResponseBytes = std::is_void_v<ResponseType> ? 1U : sizeof(ResponseType);
        static constexpr std::size_t ResponseAlignment = std::is_void_v<ResponseType> ? 1U : alignof(ResponseType);
        alignas(ResponseAlignment) std::byte ResponseStorage[ResponseBytes];
        InvocationState State{InvocationState::Queued};
        CompletionStatus Completion{CompletionStatus::Succeeded};
        ExecutionFailure Failure{ExecutionFailure::ExecutorFailure};
        std::uint32_t Generation{0U};
        bool Occupied{false};
        bool RequestLive{false};
        bool ResponseLive{false};
        bool ResponseTaken{false};
        bool CancellationRequested{false};
        bool HandleRetained{false};
    };

    std::array<Record, Plan::InvocationCapacity> _records{};
    std::array<std::size_t, Plan::QueueCapacity == 0U ? 1U : Plan::QueueCapacity> _queue{};
    std::size_t _queueHead{0U};
    std::size_t _queueTail{0U};
    std::size_t _queueCount{0U};
    std::size_t _executing{0U};
    RuntimeState _state{RuntimeState::Uninitialized};
    TExecutor* _executor{nullptr};

    [[nodiscard]] bool Matches(std::size_t index, std::uint32_t generation) const noexcept {
        return index < _records.size() && _records[index].Occupied && _records[index].Generation == generation;
    }

    void ReclaimIfPossible(std::size_t index) noexcept {
        auto& record = _records[index];
        const bool terminal = record.State == InvocationState::Completed || record.State == InvocationState::Cancelled;
        if (!terminal || record.HandleRetained) return;
        if (record.RequestLive) {
            ESPressio::Memory::ObjectLifetime::Destroy(*reinterpret_cast<RequestType*>(record.RequestStorage));
            record.RequestLive = false;
        }
        if constexpr (!std::is_void_v<ResponseType>) {
            if (record.ResponseLive) {
                ESPressio::Memory::ObjectLifetime::Destroy(*reinterpret_cast<ResponseType*>(record.ResponseStorage));
                record.ResponseLive = false;
            }
        }
        record.Occupied = false;
        if (_state == RuntimeState::Quiescing && ActiveCount() == 0U) _state = RuntimeState::Quiescent;
    }

    [[nodiscard]] std::size_t ActiveCount() const noexcept {
        std::size_t count = 0U;
        for (const auto& record : _records) if (record.Occupied) ++count;
        return count;
    }

public:
    explicit Runtime(TExecutor& executor) noexcept : _executor(&executor) {}
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;

    [[nodiscard]] bool Initialize() noexcept {
        if (_state != RuntimeState::Uninitialized) return false;
        _state = RuntimeState::Running;
        return true;
    }

    [[nodiscard]] RuntimeState State() const noexcept { return _state; }

    [[nodiscard]] bool BeginQuiesce() noexcept {
        if (_state != RuntimeState::Running) return false;
        _state = ActiveCount() == 0U ? RuntimeState::Quiescent : RuntimeState::Quiescing;
        return true;
    }

    [[nodiscard]] DispatchResult<TCommand, Runtime> Dispatch(RequestType request) noexcept {
        if (_state != RuntimeState::Running) return DispatchResult<TCommand, Runtime>(DispatchFailure::RuntimeUnavailable);
        if (_queueCount >= Plan::QueueCapacity) return DispatchResult<TCommand, Runtime>(DispatchFailure::NoCapacity);

        std::size_t index = Plan::InvocationCapacity;
        for (std::size_t candidate = 0U; candidate < _records.size(); ++candidate) {
            if (!_records[candidate].Occupied) { index = candidate; break; }
        }
        if (index == Plan::InvocationCapacity) return DispatchResult<TCommand, Runtime>(DispatchFailure::NoCapacity);

        auto& record = _records[index];
        ++record.Generation;
        if (record.Generation == 0U) ++record.Generation;
        record.Occupied = true;
        record.State = InvocationState::Queued;
        record.CancellationRequested = false;
        record.ResponseTaken = false;
        record.ResponseLive = false;
        record.HandleRetained = true;
        static_cast<void>(ESPressio::Memory::ObjectLifetime::MoveConstruct<RequestType>(record.RequestStorage, request));
        record.RequestLive = true;

        _queue[_queueTail] = index;
        _queueTail = (_queueTail + 1U) % _queue.size();
        ++_queueCount;
        return DispatchResult<TCommand, Runtime>(*this, index, record.Generation);
    }

    [[nodiscard]] bool ExecuteOne() noexcept {
        if (_queueCount == 0U || _executing >= Plan::ExecutionConcurrency) return false;
        const auto index = _queue[_queueHead];
        _queueHead = (_queueHead + 1U) % _queue.size();
        --_queueCount;
        auto& record = _records[index];
        if (!record.Occupied || record.State != InvocationState::Queued) return false;
        if (record.CancellationRequested) {
            record.State = InvocationState::Cancelled;
            ReclaimIfPossible(index);
            return true;
        }

        record.State = InvocationState::Executing;
        ++_executing;
        auto& request = *reinterpret_cast<RequestType*>(record.RequestStorage);
        CancellationToken cancellation(record.CancellationRequested);
        auto result = _executor->Execute(request, cancellation);
        --_executing;

        if (record.CancellationRequested) {
            record.State = InvocationState::Cancelled;
        } else {
            record.Completion = result.Status();
            record.Failure = result.Failure();
            if constexpr (!std::is_void_v<ResponseType>) {
                if (result.Status() != CompletionStatus::Failed && result.HasResponse()) {
                    auto response = result.TakeResponse();
                    static_cast<void>(ESPressio::Memory::ObjectLifetime::MoveConstruct<ResponseType>(record.ResponseStorage, response));
                    record.ResponseLive = true;
                }
            }
            record.State = InvocationState::Completed;
        }
        ReclaimIfPossible(index);
        return true;
    }

    [[nodiscard]] InvocationObservation Observe(std::size_t index, std::uint32_t generation, bool& valid) const noexcept {
        valid = Matches(index, generation);
        if (!valid) return {};
        const auto& record = _records[index];
        InvocationObservation observation{};
        observation.State = record.State;
        if (record.State == InvocationState::Completed) {
            observation.HasCompletion = true;
            observation.Completion = record.Completion;
            observation.HasFailure = record.Completion == CompletionStatus::Failed;
            observation.Failure = record.Failure;
        }
        return observation;
    }

    [[nodiscard]] CancellationRequestResult RequestCancellation(std::size_t index, std::uint32_t generation) noexcept {
        if (!Matches(index, generation)) return CancellationRequestResult::InvalidHandle;
        auto& record = _records[index];
        if (record.State == InvocationState::Completed || record.State == InvocationState::Cancelled)
            return CancellationRequestResult::AlreadyTerminal;
        if (record.CancellationRequested) return CancellationRequestResult::AlreadyRequested;
        record.CancellationRequested = true;
        if (record.State == InvocationState::Queued) record.State = InvocationState::Cancelled;
        return CancellationRequestResult::Accepted;
    }

    template<class R = ResponseType>
    requires (!std::is_void_v<R>)
    [[nodiscard]] TakeResponseStatus TakeResponse(std::size_t index, std::uint32_t generation, void* destination) noexcept {
        if (!Matches(index, generation)) return TakeResponseStatus::InvalidHandle;
        auto& record = _records[index];
        if (record.State != InvocationState::Completed && record.State != InvocationState::Cancelled) return TakeResponseStatus::NotTerminal;
        if (!record.ResponseLive) return TakeResponseStatus::NoResponse;
        if (record.ResponseTaken) return TakeResponseStatus::AlreadyTaken;
        auto& response = *reinterpret_cast<R*>(record.ResponseStorage);
        static_cast<void>(ESPressio::Memory::ObjectLifetime::MoveConstruct<R>(destination, response));
        ESPressio::Memory::ObjectLifetime::Destroy(response);
        record.ResponseLive = false;
        record.ResponseTaken = true;
        return TakeResponseStatus::Taken;
    }

    void Release(std::size_t index, std::uint32_t generation) noexcept {
        if (!Matches(index, generation)) return;
        _records[index].HandleRetained = false;
        ReclaimIfPossible(index);
    }
};

} // ESPressio::Command
