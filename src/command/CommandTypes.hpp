#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>

#include <ESPressio_Clock.hpp>
#include <ESPressio_Memory.hpp>

namespace ESPressio::Command {

    /// Canonical finite duration used by Command waiting operations.
    using Duration = ESPressio::Clock::Duration;

    /// Canonical monotonic timestamp used by Command deadline waiting operations.
    using MonotonicTimestamp = ESPressio::Clock::MonotonicTimestamp;

    /// Identifies the current lifecycle state of one admitted invocation.
    enum class InvocationState : std::uint8_t {
        Queued = 0U,
        Executing = 1U,
        Completed = 2U,
        Cancelled = 3U
    };

    /// Identifies how a terminal invocation semantically finished.
    enum class Outcome : std::uint8_t {
        Succeeded = 0U,
        Rejected = 1U,
        Failed = 2U,
        Cancelled = 3U
    };

    /// Identifies the Command-domain cause of a failed terminal outcome.
    enum class ExecutionFailure : std::uint8_t {
        ExecutorFailure = 0U,
        IntegrationFailure = 1U
    };

    /// Identifies why dispatch could not admit a Command invocation.
    enum class DispatchFailure : std::uint8_t {
        NoCapacity = 0U,
        TypeUnrecognised = 1U,
        RuntimeUnavailable = 2U,
        BindingUnavailable = 3U
    };

    /// Identifies the result of a finite Handle wait operation.
    enum class WaitResult : std::uint8_t {
        Terminal = 0U,
        TimedOut = 1U,
        Interrupted = 2U,
        InvalidHandle = 3U
    };

    /// Identifies the result of requesting cooperative cancellation.
    enum class CancellationRequestResult : std::uint8_t {
        Requested = 0U,
        AlreadyRequested = 1U,
        TooLate = 2U,
        InvalidHandle = 3U
    };

    /// Identifies the result of extracting a retained typed response.
    enum class TakeResponseStatus : std::uint8_t {
        Taken = 0U,
        NotTerminal = 1U,
        NoResponse = 2U,
        AlreadyTaken = 3U,
        InvalidHandle = 4U
    };

    /// Identifies the result of attempting exactly-once integration completion publication.
    enum class CompletionPublicationResult : std::uint8_t {
        Accepted = 0U,
        AlreadyCompleted = 1U,
        Unavailable = 2U
    };

    /// Identifies the lifecycle state of a Command Runtime.
    enum class RuntimeState : std::uint8_t {
        Uninitialized = 0U,
        Running = 1U,
        Quiescing = 2U,
        Quiescent = 3U
    };

    /// Identifies the result of Runtime initialization.
    enum class InitializationResult : std::uint8_t {
        Initialized = 0U,
        AlreadyInitialized = 1U
    };

    /// Identifies whether one queued invocation was executed by an execution attempt.
    enum class ExecutionAttemptResult : std::uint8_t {
        Executed = 0U,
        NotExecuted = 1U
    };

    /// Identifies the result of beginning Runtime quiescence.
    enum class QuiesceResult : std::uint8_t {
        Started = 0U,
        NotRunning = 1U
    };

    /// Compile-time Dispatch policy permitting only the local Command execution domain.
    struct LocalOnly final {};

    /// Compile-time Dispatch policy permitting only the remote Command execution domain.
    struct RemoteOnly final {};

    /// Compile-time Dispatch policy permitting independent local and remote Command execution domains.
    struct LocalAndRemote final {};

    /// Identifies a supported compile-time Command execution-domain Dispatch policy.
    /// @tparam TScope Candidate execution-domain policy Type.
    template<class TScope>
    concept ExecutionDomainScope =
        std::is_same_v<std::remove_cvref_t<TScope>, LocalOnly>
        || std::is_same_v<std::remove_cvref_t<TScope>, RemoteOnly>
        || std::is_same_v<std::remove_cvref_t<TScope>, LocalAndRemote>;

    /// Snapshot of one invocation's observable lifecycle and terminal semantics.
    struct InvocationObservation final {

        // Lifecycle observation.

        /// Current lifecycle state of the invocation.
        InvocationState State{InvocationState::Queued};

        // Terminal outcome observation.

        /// Indicates whether TerminalOutcome is meaningful for this observation.
        bool HasOutcome{false};

        /// Semantic terminal outcome; meaningful only when HasOutcome is true.
        Outcome TerminalOutcome{Outcome::Succeeded};

        // Failure-detail observation.

        /// Indicates whether Failure contains terminal failure detail.
        bool HasFailure{false};

        /// Failure cause; meaningful only when HasFailure is true.
        ExecutionFailure Failure{ExecutionFailure::ExecutorFailure};

        // Terminal-outcome predicates.

        /// Returns true when the invocation terminated successfully.
        [[nodiscard]] bool DidSucceed() const noexcept {
            return HasOutcome && TerminalOutcome == Outcome::Succeeded;
        }

        /// Returns true when the invocation terminated by rejection.
        [[nodiscard]] bool WasRejected() const noexcept {
            return HasOutcome && TerminalOutcome == Outcome::Rejected;
        }

        /// Returns true when the invocation terminated by failure.
        [[nodiscard]] bool DidFail() const noexcept {
            return HasOutcome && TerminalOutcome == Outcome::Failed;
        }

        /// Returns true when the invocation terminated by cancellation.
        [[nodiscard]] bool WasCancelled() const noexcept {
            return HasOutcome && TerminalOutcome == Outcome::Cancelled;
        }
    };

    /// Resolves the Request type declared by TCommand.
    /// @tparam TCommand Command declaration whose Request type is required.
    template<class TCommand>
    using Request = typename TCommand::Request;

    /// Resolves the Response type declared by TCommand.
    /// @tparam TCommand Command declaration whose Response type is required.
    template<class TCommand>
    using Response = typename TCommand::Response;

    /// Requires a Command declaration to expose Request and Response types.
    /// @tparam TCommand Candidate Command declaration.
    template<class TCommand>
    concept CommandType = requires {
        typename TCommand::Request;
        typename TCommand::Response;
    };

    /// Read-only cooperative cancellation view supplied to a Command Handler.
    class CancellationToken final {
    private:
        // Borrowed cancellation state.

        /// Points to the Runtime-owned cancellation-request predicate.
        const bool* _requested{nullptr};

    public:
        // Construction.

        /// Borrows the Runtime-owned cancellation-request predicate.
        explicit CancellationToken(const bool& requested) noexcept :
            _requested(&requested) {
        }

        // Cancellation observation.

        /// Returns true when cancellation has been requested.
        [[nodiscard]] bool IsRequested() const noexcept {
            return _requested != nullptr && *_requested;
        }
    };

    /// Typed result returned by a non-void Command Handler.
    /// @tparam TResponse Response object produced by successful or rejected execution.
    template<class TResponse>
    class ExecutionResult final {
    private:
        // Terminal semantics.

        /// Semantic outcome selected by the Handler.
        Outcome _outcome;

        /// Failure detail when the selected outcome is Failed.
        ExecutionFailure _failure;

        // Optional response payload.

        /// Response retained for successful or rejected execution.
        std::optional<TResponse> _response;

        // Internal construction.

        /// Constructs a result with the selected semantic outcome and failure detail.
        explicit ExecutionResult(Outcome outcome, ExecutionFailure failure) noexcept :
            _outcome(outcome),
            _failure(failure) {
        }

    public:
        // Result factories.

        /// Creates a successful execution result and takes ownership of response.
        static ExecutionResult Succeeded(TResponse response) noexcept {
            ExecutionResult result(
                Outcome::Succeeded,
                ExecutionFailure::ExecutorFailure
            );
            result._response.emplace(ESPressio::Memory::OwnershipTransfer::Move(response));
            return result;
        }

        /// Creates a rejected execution result and takes ownership of response.
        static ExecutionResult Rejected(TResponse response) noexcept {
            ExecutionResult result(
                Outcome::Rejected,
                ExecutionFailure::ExecutorFailure
            );
            result._response.emplace(ESPressio::Memory::OwnershipTransfer::Move(response));
            return result;
        }

        /// Creates a failed execution result.
        static ExecutionResult Failed() noexcept {
            return ExecutionResult(
                Outcome::Failed,
                ExecutionFailure::ExecutorFailure
            );
        }

        // Result observation.

        /// Returns the semantic terminal outcome selected by the Handler.
        [[nodiscard]] Outcome GetOutcome() const noexcept {
            return _outcome;
        }

        /// Returns failure detail associated with this execution result.
        [[nodiscard]] ExecutionFailure Failure() const noexcept {
            return _failure;
        }

        /// Returns true when this result retains a response payload.
        [[nodiscard]] bool HasResponse() const noexcept {
            return _response.has_value();
        }

        // Response ownership transfer.

        /// Transfers the retained response to the caller.
        TResponse TakeResponse() noexcept {
            return ESPressio::Memory::OwnershipTransfer::Move(_response.value());
        }
    };

    /// Typed result returned by a void-response Command Handler.
    template<>
    class ExecutionResult<void> final {
    private:
        // Terminal semantics.

        /// Semantic outcome selected by the Handler.
        Outcome _outcome;

        // Internal construction.

        /// Constructs a void result with the selected semantic outcome.
        explicit ExecutionResult(Outcome outcome) noexcept :
            _outcome(outcome) {
        }

    public:
        // Result factories.

        /// Creates a successful void execution result.
        static ExecutionResult Succeeded() noexcept {
            return ExecutionResult(Outcome::Succeeded);
        }

        /// Creates a rejected void execution result.
        static ExecutionResult Rejected() noexcept {
            return ExecutionResult(Outcome::Rejected);
        }

        /// Creates a failed void execution result.
        static ExecutionResult Failed() noexcept {
            return ExecutionResult(Outcome::Failed);
        }

        // Result observation.

        /// Returns the semantic terminal outcome selected by the Handler.
        [[nodiscard]] Outcome GetOutcome() const noexcept {
            return _outcome;
        }

        /// Returns the executor failure detail associated with a failed void result.
        [[nodiscard]] ExecutionFailure Failure() const noexcept {
            return ExecutionFailure::ExecutorFailure;
        }
    };

} // ESPressio::Command
