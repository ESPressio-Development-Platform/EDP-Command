#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "CommandTypes.hpp"

namespace ESPressio::Command {

    /// Typed inbound admission facade. Remote correlation remains owned by the adapter.
    ///
    /// @tparam TCommand Command type admitted by this facade.
    /// @tparam TRuntime Runtime type receiving the admitted Command.
    template<class TCommand, class TRuntime>
    class InboundAdmission final {
    private:
        // Borrowed runtime binding.

        /// Runtime that owns local Command admission and lifecycle state.
        TRuntime* _runtime;

    public:
        // Construction.

        /// Binds this facade to the local Runtime used for admission.
        explicit InboundAdmission(TRuntime& runtime) noexcept :
            _runtime(&runtime) {
        }

        // Admission operations.

        /// Transfers one typed Request into the local Runtime admission path.
        [[nodiscard]] auto Dispatch(Request<TCommand> request) noexcept {
            return _runtime->Dispatch(
                ESPressio::Memory::OwnershipTransfer::Move(request)
            );
        }
    };

    /// Invocation-specific outbound completion capability. It is semantically single-use.
    ///
    /// @tparam TResponse Response type published by successful or rejected completion.
    template<class TResponse>
    class OutboundCompletion final {
    public:
        // Completion callback contracts.

        /// Callback used to publish successful or rejected completion with a Response.
        using SuccessFn = CompletionPublicationResult (*)(void*, Outcome, TResponse*) noexcept;

        /// Callback used to publish failed completion with its failure detail.
        using FailureFn = CompletionPublicationResult (*)(void*, ExecutionFailure) noexcept;

        /// Callback used to publish cancelled completion.
        using CancelFn = CompletionPublicationResult (*)(void*) noexcept;

    private:
        // Invocation-specific publication binding and single-use state.

        /// Adapter-owned invocation context supplied to publication callbacks.
        void* _context{nullptr};

        /// Callback that publishes success or rejection.
        SuccessFn _success{nullptr};

        /// Callback that publishes failure.
        FailureFn _failure{nullptr};

        /// Callback that publishes cancellation.
        CancelFn _cancel{nullptr};

        /// Records whether this completion capability has already attempted publication.
        bool _used{false};

    public:
        // Construction and ownership.

        /// Creates a completion capability over adapter-owned invocation callbacks.
        OutboundCompletion(
            void* context,
            SuccessFn success,
            FailureFn failure,
            CancelFn cancel
        ) noexcept :
            _context(context),
            _success(success),
            _failure(failure),
            _cancel(cancel) {
        }

        /// Completion capabilities cannot be copied because publication is single-use.
        OutboundCompletion(const OutboundCompletion&) = delete;

        /// Completion capabilities cannot be copy-assigned because publication is single-use.
        OutboundCompletion& operator=(const OutboundCompletion&) = delete;

        // Terminal publication operations.

        /// Publishes a successful terminal Outcome and transfers the supplied Response to the adapter callback.
        [[nodiscard]] CompletionPublicationResult Succeeded(TResponse response) noexcept {
            if (_used) { return CompletionPublicationResult::AlreadyCompleted; }
            if (_success == nullptr) { return CompletionPublicationResult::Unavailable; }

            _used = true;
            return _success(
                _context,
                Outcome::Succeeded,
                &response
            );
        }

        /// Publishes a rejected terminal Outcome and transfers the supplied Response to the adapter callback.
        [[nodiscard]] CompletionPublicationResult Rejected(TResponse response) noexcept {
            if (_used) { return CompletionPublicationResult::AlreadyCompleted; }
            if (_success == nullptr) { return CompletionPublicationResult::Unavailable; }

            _used = true;
            return _success(
                _context,
                Outcome::Rejected,
                &response
            );
        }

        /// Publishes terminal integration failure.
        [[nodiscard]] CompletionPublicationResult Failed() noexcept {
            if (_used) { return CompletionPublicationResult::AlreadyCompleted; }
            if (_failure == nullptr) { return CompletionPublicationResult::Unavailable; }

            _used = true;
            return _failure(
                _context,
                ExecutionFailure::IntegrationFailure
            );
        }

        /// Publishes terminal cancellation.
        [[nodiscard]] CompletionPublicationResult Cancelled() noexcept {
            if (_used) { return CompletionPublicationResult::AlreadyCompleted; }
            if (_cancel == nullptr) { return CompletionPublicationResult::Unavailable; }

            _used = true;
            return _cancel(_context);
        }
    };

    /// Void-Response specialization of the invocation-specific outbound completion capability.
    template<>
    class OutboundCompletion<void> final {
    public:
        // Completion callback contracts.

        /// Callback used to publish successful or rejected completion without a Response payload.
        using SuccessFn = CompletionPublicationResult (*)(void*, Outcome) noexcept;

        /// Callback used to publish failed completion with its failure detail.
        using FailureFn = CompletionPublicationResult (*)(void*, ExecutionFailure) noexcept;

        /// Callback used to publish cancelled completion.
        using CancelFn = CompletionPublicationResult (*)(void*) noexcept;

    private:
        // Invocation-specific publication binding and single-use state.

        /// Adapter-owned invocation context supplied to publication callbacks.
        void* _context{nullptr};

        /// Callback that publishes success or rejection.
        SuccessFn _success{nullptr};

        /// Callback that publishes failure.
        FailureFn _failure{nullptr};

        /// Callback that publishes cancellation.
        CancelFn _cancel{nullptr};

        /// Records whether this completion capability has already attempted publication.
        bool _used{false};

    public:
        // Construction.

        /// Creates a void-Response completion capability over adapter-owned invocation callbacks.
        OutboundCompletion(
            void* context,
            SuccessFn success,
            FailureFn failure,
            CancelFn cancel
        ) noexcept :
            _context(context),
            _success(success),
            _failure(failure),
            _cancel(cancel) {
        }

        // Terminal publication operations.

        /// Publishes a successful terminal Outcome.
        [[nodiscard]] CompletionPublicationResult Succeeded() noexcept {
            if (_used) { return CompletionPublicationResult::AlreadyCompleted; }
            if (_success == nullptr) { return CompletionPublicationResult::Unavailable; }

            _used = true;
            return _success(
                _context,
                Outcome::Succeeded
            );
        }

        /// Publishes a rejected terminal Outcome.
        [[nodiscard]] CompletionPublicationResult Rejected() noexcept {
            if (_used) { return CompletionPublicationResult::AlreadyCompleted; }
            if (_success == nullptr) { return CompletionPublicationResult::Unavailable; }

            _used = true;
            return _success(
                _context,
                Outcome::Rejected
            );
        }

        /// Publishes terminal integration failure.
        [[nodiscard]] CompletionPublicationResult Failed() noexcept {
            if (_used) { return CompletionPublicationResult::AlreadyCompleted; }
            if (_failure == nullptr) { return CompletionPublicationResult::Unavailable; }

            _used = true;
            return _failure(
                _context,
                ExecutionFailure::IntegrationFailure
            );
        }

        /// Publishes terminal cancellation.
        [[nodiscard]] CompletionPublicationResult Cancelled() noexcept {
            if (_used) { return CompletionPublicationResult::AlreadyCompleted; }
            if (_cancel == nullptr) { return CompletionPublicationResult::Unavailable; }

            _used = true;
            return _cancel(_context);
        }
    };

    /// Narrow outbound invocation view supplied to an integration adapter.
    ///
    /// @tparam TCommand Command whose Request is being integrated.
    /// @tparam TCompletion Invocation-specific completion capability type.
    template<class TCommand, class TCompletion>
    struct OutboundInvocation final {
        // Invocation capabilities.

        /// Borrowed Request owned by the Command invocation record.
        const Request<TCommand>& RequestView;

        /// Cooperative cancellation observation capability for this invocation.
        CancellationToken Cancellation;

        /// Single-use terminal completion publication capability.
        TCompletion Completion;
    };

} // ESPressio::Command
