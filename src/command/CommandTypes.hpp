#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <ESPressio_Clock.hpp>
#include <ESPressio_Memory.hpp>

namespace ESPressio::Command {
using Duration = ESPressio::Clock::Duration;
using MonotonicTimestamp = ESPressio::Clock::MonotonicTimestamp;

enum class InvocationState : std::uint8_t { Queued, Executing, Completed, Cancelled };
enum class CompletionStatus : std::uint8_t { Succeeded, Rejected, Failed };
enum class ExecutionFailure : std::uint8_t { ExecutorFailure, IntegrationFailure };
enum class DispatchFailure : std::uint8_t { NoCapacity, TypeUnrecognised, RuntimeUnavailable, BindingUnavailable };
enum class WaitResult : std::uint8_t { Terminal, TimedOut, Interrupted, InvalidHandle };
enum class CancellationRequestResult : std::uint8_t { Requested, AlreadyRequested, TooLate, InvalidHandle };
enum class TakeResponseStatus : std::uint8_t { Taken, NotTerminal, NoResponse, AlreadyTaken, InvalidHandle };
enum class RuntimeState : std::uint8_t { Uninitialized, Running, Quiescing, Quiescent };

struct InvocationObservation final {
    InvocationState State{InvocationState::Queued};
    bool HasCompletion{false};
    CompletionStatus Completion{CompletionStatus::Succeeded};
    bool HasFailure{false};
    ExecutionFailure Failure{ExecutionFailure::ExecutorFailure};
};

template<class TCommand> using Request = typename TCommand::Request;
template<class TCommand> using Response = typename TCommand::Response;
template<class TCommand> concept CommandType = requires { typename TCommand::Request; typename TCommand::Response; };

class CancellationToken final {
    const bool* _requested{nullptr};
public:
    explicit CancellationToken(const bool& requested) noexcept : _requested(&requested) {}
    [[nodiscard]] bool IsRequested() const noexcept { return _requested != nullptr && *_requested; }
};

template<class TResponse>
class ExecutionResult final {
    CompletionStatus _status;
    ExecutionFailure _failure;
    std::optional<TResponse> _response;
    explicit ExecutionResult(CompletionStatus status, ExecutionFailure failure) noexcept : _status(status), _failure(failure) {}
public:
    static ExecutionResult Succeeded(TResponse response) noexcept {
        ExecutionResult result(CompletionStatus::Succeeded, ExecutionFailure::ExecutorFailure);
        result._response.emplace(ESPressio::Memory::OwnershipTransfer::Move(response));
        return result;
    }
    static ExecutionResult Rejected(TResponse response) noexcept {
        ExecutionResult result(CompletionStatus::Rejected, ExecutionFailure::ExecutorFailure);
        result._response.emplace(ESPressio::Memory::OwnershipTransfer::Move(response));
        return result;
    }
    static ExecutionResult Failed() noexcept { return ExecutionResult(CompletionStatus::Failed, ExecutionFailure::ExecutorFailure); }
    [[nodiscard]] CompletionStatus Status() const noexcept { return _status; }
    [[nodiscard]] ExecutionFailure Failure() const noexcept { return _failure; }
    [[nodiscard]] bool HasResponse() const noexcept { return _response.has_value(); }
    TResponse TakeResponse() noexcept { return ESPressio::Memory::OwnershipTransfer::Move(_response.value()); }
};

template<>
class ExecutionResult<void> final {
    CompletionStatus _status;
    explicit ExecutionResult(CompletionStatus status) noexcept : _status(status) {}
public:
    static ExecutionResult Succeeded() noexcept { return ExecutionResult(CompletionStatus::Succeeded); }
    static ExecutionResult Rejected() noexcept { return ExecutionResult(CompletionStatus::Rejected); }
    static ExecutionResult Failed() noexcept { return ExecutionResult(CompletionStatus::Failed); }
    [[nodiscard]] CompletionStatus Status() const noexcept { return _status; }
    [[nodiscard]] ExecutionFailure Failure() const noexcept { return ExecutionFailure::ExecutorFailure; }
};

} // ESPressio::Command
