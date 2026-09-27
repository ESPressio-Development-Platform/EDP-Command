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

        /// Explicitly performs local-only inbound admission for one typed Request.
        [[nodiscard]] auto Dispatch(
            LocalOnly,
            Request<TCommand> request
        ) noexcept {
            return _runtime->Dispatch(
                ESPressio::Memory::OwnershipTransfer::Move(request)
            );
        }
    };

    /// Structurally separate outcomes from one LocalAndRemote scoped Dispatch.
    /// @tparam TLocalResult Result Type produced by the local-domain operation.
    /// @tparam TRemoteResult Result Type produced by the remote-domain operation.
    template<class TLocalResult, class TRemoteResult>
    class LocalAndRemoteDispatchResult final {
    private:
        // Independently produced domain results.

        /// Result produced by the local-domain Dispatch operation.
        TLocalResult _local;

        /// Result produced by the remote-domain Dispatch operation.
        TRemoteResult _remote;

    public:
        // Construction.

        /// Invokes each independently selected domain operation exactly once.
        /// @tparam TLocalOperation Callable producing the local-domain result.
        /// @tparam TRemoteOperation Callable producing the remote-domain result.
        template<class TLocalOperation, class TRemoteOperation>
        LocalAndRemoteDispatchResult(
            TLocalOperation& localOperation,
            TRemoteOperation& remoteOperation
        ) noexcept(
            noexcept(localOperation())
            && noexcept(remoteOperation())
        ) :
            _local(localOperation()),
            _remote(remoteOperation()) {
        }

        /// Combined results cannot be copied because either domain result may own an exclusive capability.
        LocalAndRemoteDispatchResult(const LocalAndRemoteDispatchResult&) = delete;

        /// Combined results cannot be copy-assigned because either domain result may own an exclusive capability.
        LocalAndRemoteDispatchResult& operator=(const LocalAndRemoteDispatchResult&) = delete;

        /// Transfers both independent domain results when their Types permit movement.
        LocalAndRemoteDispatchResult(LocalAndRemoteDispatchResult&&) noexcept = default;

        /// Transfers both independent domain results when their Types permit move assignment.
        LocalAndRemoteDispatchResult& operator=(LocalAndRemoteDispatchResult&&) noexcept = default;

        // Domain-result access.

        /// Returns the mutable local-domain result.
        [[nodiscard]] TLocalResult& Local() noexcept {
            return _local;
        }

        /// Returns the immutable local-domain result.
        [[nodiscard]] const TLocalResult& Local() const noexcept {
            return _local;
        }

        /// Returns the mutable remote-domain result.
        [[nodiscard]] TRemoteResult& Remote() noexcept {
            return _remote;
        }

        /// Returns the immutable remote-domain result.
        [[nodiscard]] const TRemoteResult& Remote() const noexcept {
            return _remote;
        }
    };

    // Compile-time execution-domain Dispatch coordination.

    /// Invokes exactly one already-selected local Dispatch operation.
    /// @tparam TLocalOperation Callable implementing the local-domain Dispatch operation.
    template<class TLocalOperation>
    [[nodiscard]] auto DispatchScoped(
        LocalOnly,
        TLocalOperation& localOperation
    ) noexcept {
        using LocalResult = decltype(localOperation());
        static_assert(
            noexcept(localOperation()),
            "LocalOnly Dispatch operation must be non-throwing"
        );
        static_assert(
            !std::is_void_v<LocalResult>,
            "LocalOnly Dispatch requires an observable local-domain result"
        );
        return localOperation();
    }

    /// Invokes exactly one already-selected remote Dispatch operation and never invokes local Runtime admission.
    /// @tparam TRemoteOperation Callable implementing the higher integration/routing remote-domain Dispatch operation.
    template<class TRemoteOperation>
    [[nodiscard]] auto DispatchScoped(
        RemoteOnly,
        TRemoteOperation& remoteOperation
    ) noexcept {
        using RemoteResult = decltype(remoteOperation());
        static_assert(
            noexcept(remoteOperation()),
            "RemoteOnly Dispatch operation must be non-throwing"
        );
        static_assert(
            !std::is_void_v<RemoteResult>,
            "RemoteOnly Dispatch requires an observable remote-domain result"
        );
        return remoteOperation();
    }

    /// Independently invokes one already-selected local operation and one higher-layer remote operation.
    ///
    /// Invocation sequence is an implementation detail and creates no cross-domain ordering, transaction,
    /// rollback, fallback, suppression, quorum or aggregate-success contract.
    /// @tparam TLocalOperation Callable implementing the local-domain Dispatch operation.
    /// @tparam TRemoteOperation Callable implementing the higher integration/routing remote-domain Dispatch operation.
    template<class TLocalOperation, class TRemoteOperation>
    [[nodiscard]] auto DispatchScoped(
        LocalAndRemote,
        TLocalOperation& localOperation,
        TRemoteOperation& remoteOperation
    ) noexcept {
        using LocalResult = decltype(localOperation());
        using RemoteResult = decltype(remoteOperation());

        static_assert(
            noexcept(localOperation()),
            "LocalAndRemote local Dispatch operation must be non-throwing"
        );
        static_assert(
            noexcept(remoteOperation()),
            "LocalAndRemote remote Dispatch operation must be non-throwing"
        );
        static_assert(
            !std::is_void_v<LocalResult>,
            "LocalAndRemote Dispatch requires an observable local-domain result"
        );
        static_assert(
            !std::is_void_v<RemoteResult>,
            "LocalAndRemote Dispatch requires an observable remote-domain result"
        );

        return LocalAndRemoteDispatchResult<LocalResult, RemoteResult>(
            localOperation,
            remoteOperation
        );
    }

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
