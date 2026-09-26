#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <ESPressio_Clock.hpp>

namespace ESPressio::Command {

using Duration = ESPressio::Clock::Duration;
using MonotonicTimestamp = ESPressio::Clock::MonotonicTimestamp;

enum class InvocationState : std::uint8_t { Queued, Executing, Completed, Cancelled };
enum class CompletionStatus : std::uint8_t { Succeeded, Rejected, Failed };
enum class ExecutionFailure : std::uint8_t { ExecutorFailure, IntegrationFailure };
enum class DispatchFailure : std::uint8_t { NoCapacity, TypeUnrecognised, RuntimeUnavailable, BindingUnavailable };
enum class WaitResult : std::uint8_t { Finished, TimedOut, InvalidHandle, ProviderFailure };
enum class CancellationRequestResult : std::uint8_t { Accepted, AlreadyRequested, AlreadyTerminal, InvalidHandle };
enum class TakeResponseStatus : std::uint8_t { Taken, NotTerminal, NoResponse, AlreadyTaken, InvalidHandle };
enum class RuntimeState : std::uint8_t { Uninitialized, Running, Quiescing, Quiescent };

struct InvocationObservation final {
    InvocationState State{InvocationState::Queued};
    bool HasCompletion{false};
    CompletionStatus Completion{CompletionStatus::Succeeded};
    bool HasFailure{false};
    ExecutionFailure Failure{ExecutionFailure::ExecutorFailure};
};

template<class TCommand>
using Request = typename TCommand::Request;

template<class TCommand>
using Response = typename TCommand::Response;

template<class TCommand>
concept CommandType = requires {
    typename TCommand::Request;
    typename TCommand::Response;
};

class CancellationToken final {
    const bool* _requested{nullptr};
public:
    explicit CancellationToken(const bool& requested) noexcept : _requested(&requested) {}
    [[nodiscard]] bool IsRequested() const noexcept { return _requested != nullptr && *_requested; }
};

template<class TResponse>
struct ExecutionResult final {
    CompletionStatus Status{CompletionStatus::Failed};
    ExecutionFailure Failure{ExecutionFailure::ExecutorFailure};
    TResponse ResponseValue;
};

template<>
struct ExecutionResult<void> final {
    CompletionStatus Status{CompletionStatus::Failed};
    ExecutionFailure Failure{ExecutionFailure::ExecutorFailure};
};

} // ESPressio::Command
