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

enum class InvocationState : std::uint8_t { Queued = 0U, Executing = 1U, Completed = 2U, Cancelled = 3U };
enum class Outcome : std::uint8_t { Succeeded = 0U, Rejected = 1U, Failed = 2U, Cancelled = 3U };
enum class ExecutionFailure : std::uint8_t { ExecutorFailure = 0U, IntegrationFailure = 1U };
enum class DispatchFailure : std::uint8_t { NoCapacity = 0U, TypeUnrecognised = 1U, RuntimeUnavailable = 2U, BindingUnavailable = 3U };
enum class WaitResult : std::uint8_t { Terminal = 0U, TimedOut = 1U, Interrupted = 2U, InvalidHandle = 3U };
enum class CancellationRequestResult : std::uint8_t { Requested = 0U, AlreadyRequested = 1U, TooLate = 2U, InvalidHandle = 3U };
enum class TakeResponseStatus : std::uint8_t { Taken = 0U, NotTerminal = 1U, NoResponse = 2U, AlreadyTaken = 3U, InvalidHandle = 4U };
enum class CompletionPublicationResult : std::uint8_t { Accepted = 0U, AlreadyCompleted = 1U, Unavailable = 2U };
enum class RuntimeState : std::uint8_t { Uninitialized = 0U, Running = 1U, Quiescing = 2U, Quiescent = 3U };

struct InvocationObservation final {
    InvocationState State{InvocationState::Queued};
    bool HasOutcome{false};
    Outcome TerminalOutcome{Outcome::Succeeded};
    bool HasFailure{false};
    ExecutionFailure Failure{ExecutionFailure::ExecutorFailure};

    [[nodiscard]] bool DidSucceed() const noexcept { return HasOutcome && TerminalOutcome == Outcome::Succeeded; }
    [[nodiscard]] bool WasRejected() const noexcept { return HasOutcome && TerminalOutcome == Outcome::Rejected; }
    [[nodiscard]] bool DidFail() const noexcept { return HasOutcome && TerminalOutcome == Outcome::Failed; }
    [[nodiscard]] bool WasCancelled() const noexcept { return HasOutcome && TerminalOutcome == Outcome::Cancelled; }
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
    Outcome _outcome;
    ExecutionFailure _failure;
    std::optional<TResponse> _response;
    explicit ExecutionResult(Outcome outcome, ExecutionFailure failure) noexcept : _outcome(outcome), _failure(failure) {}
public:
    static ExecutionResult Succeeded(TResponse response) noexcept {
        ExecutionResult result(Outcome::Succeeded, ExecutionFailure::ExecutorFailure);
        result._response.emplace(ESPressio::Memory::OwnershipTransfer::Move(response));
        return result;
    }
    static ExecutionResult Rejected(TResponse response) noexcept {
        ExecutionResult result(Outcome::Rejected, ExecutionFailure::ExecutorFailure);
        result._response.emplace(ESPressio::Memory::OwnershipTransfer::Move(response));
        return result;
    }
    static ExecutionResult Failed() noexcept { return ExecutionResult(Outcome::Failed, ExecutionFailure::ExecutorFailure); }
    [[nodiscard]] Outcome GetOutcome() const noexcept { return _outcome; }
    [[nodiscard]] ExecutionFailure Failure() const noexcept { return _failure; }
    [[nodiscard]] bool HasResponse() const noexcept { return _response.has_value(); }
    TResponse TakeResponse() noexcept { return ESPressio::Memory::OwnershipTransfer::Move(_response.value()); }
};

template<>
class ExecutionResult<void> final {
    Outcome _outcome;
    explicit ExecutionResult(Outcome outcome) noexcept : _outcome(outcome) {}
public:
    static ExecutionResult Succeeded() noexcept { return ExecutionResult(Outcome::Succeeded); }
    static ExecutionResult Rejected() noexcept { return ExecutionResult(Outcome::Rejected); }
    static ExecutionResult Failed() noexcept { return ExecutionResult(Outcome::Failed); }
    [[nodiscard]] Outcome GetOutcome() const noexcept { return _outcome; }
    [[nodiscard]] ExecutionFailure Failure() const noexcept { return ExecutionFailure::ExecutorFailure; }
};

} // ESPressio::Command
