#pragma once

#include <cstddef>
#include <cstdint>
#include <concepts>
#include <exception>
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

        /// Reserves a constructed but unpublished Request for transactional inbound population.
        [[nodiscard]] auto Prepare() noexcept
        requires std::is_nothrow_default_constructible_v<Request<TCommand>> {
            return _runtime->PrepareIngress();
        }
    };

    /// Typed family-owned facade for synchronized outbound Command handoff staging.
    ///
    /// The bound Runtime supplies the same application-owned serialization domain used by Dispatch,
    /// Handle and cancellation transitions. Reservation retains only a bounded Request borrow and the
    /// eventual adapter call occurs outside Command synchronization.
    template<class TCommand, class TRuntime>
    class OutboundHandoff final {
    private:
        TRuntime* _runtime;

    public:
        explicit OutboundHandoff(TRuntime& runtime) noexcept :
            _runtime(&runtime) {
        }

        /// Reserves one bounded stage for a live immutable Request.
        [[nodiscard]] auto Prepare(const Request<TCommand>& request) noexcept {
            return _runtime->PrepareRemoteHandoff(request);
        }

        auto Prepare(Request<TCommand>&&) noexcept = delete;
    };

    /// Structurally separate outcomes from one LocalAndRemote scoped Dispatch.
    /// @tparam TLocalResult Result Type produced by the local-domain operation.
    /// @tparam TRemoteResult Result Type produced by the remote-domain operation.
    template<class TLocalResult, class TRemoteResult>
    class LocalAndRemoteDispatchResult final {
        static_assert(
            std::is_nothrow_move_constructible_v<TLocalResult> &&
            std::is_nothrow_move_constructible_v<TRemoteResult>,
            "LocalAndRemote results must be non-throwing to move"
        );
        static_assert(
            std::is_nothrow_destructible_v<TLocalResult> &&
            std::is_nothrow_destructible_v<TRemoteResult>,
            "LocalAndRemote results must be non-throwing to destroy"
        );

    private:
        // Independently produced domain results.

        /// Result produced by the local-domain Dispatch operation.
        TLocalResult _local;

        /// Result produced by the remote-domain Dispatch operation.
        TRemoteResult _remote;

    public:
        // Construction.

        LocalAndRemoteDispatchResult(
            TLocalResult local,
            TRemoteResult remote
        ) noexcept :
            _local(ESPressio::Memory::OwnershipTransfer::Move(local)),
            _remote(ESPressio::Memory::OwnershipTransfer::Move(remote)) {
        }

        /// Combined results cannot be copied because either domain result may own an exclusive capability.
        LocalAndRemoteDispatchResult(const LocalAndRemoteDispatchResult&) = delete;

        /// Combined results cannot be copy-assigned because either domain result may own an exclusive capability.
        LocalAndRemoteDispatchResult& operator=(const LocalAndRemoteDispatchResult&) = delete;

        /// Transfers both independent domain results when their Types permit movement.
        LocalAndRemoteDispatchResult(LocalAndRemoteDispatchResult&&) noexcept = default;

        /// Transfers both independent domain results when non-throwing move assignment is available.
        LocalAndRemoteDispatchResult& operator=(LocalAndRemoteDispatchResult&&) noexcept
        requires std::is_nothrow_move_assignable_v<TLocalResult> &&
            std::is_nothrow_move_assignable_v<TRemoteResult> = default;

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

    /// Invokes local admission first, then independently invokes one higher-layer remote operation.
    ///
    /// Local refusal does not suppress the remote operation and asynchronous local execution is not awaited.
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

        auto local = localOperation();
        auto remote = remoteOperation();
        return LocalAndRemoteDispatchResult<LocalResult, RemoteResult>(
            ESPressio::Memory::OwnershipTransfer::Move(local),
            ESPressio::Memory::OwnershipTransfer::Move(remote)
        );
    }

    /// Commits one synchronized typed remote-only stage outside Command serialization.
    template<CommandType TCommand, class TRuntime, class TRemoteOperation>
    [[nodiscard]] auto DispatchScoped(
        RemoteOnly,
        RemoteHandoffReservation<TCommand, TRuntime>&& reservation,
        TRemoteOperation& remoteOperation
    ) noexcept {
        return reservation.Commit(remoteOperation);
    }

    /// Performs local admission before committing one independent synchronized remote stage.
    template<
        CommandType TCommand,
        class TRuntime,
        class TLocalOperation,
        class TRemoteOperation
    >
    [[nodiscard]] auto DispatchScoped(
        LocalAndRemote,
        RemoteHandoffReservation<TCommand, TRuntime>&& reservation,
        TLocalOperation& localOperation,
        TRemoteOperation& remoteOperation
    ) noexcept {
        using LocalResult = decltype(localOperation());
        using RemoteResult = decltype(reservation.Commit(remoteOperation));
        static_assert(noexcept(localOperation()), "Local Command admission must be non-throwing");
        static_assert(!std::is_void_v<LocalResult> && !std::is_void_v<RemoteResult>);

        auto local = localOperation();
        auto remote = reservation.Commit(remoteOperation);
        return LocalAndRemoteDispatchResult<LocalResult, RemoteResult>(
            ESPressio::Memory::OwnershipTransfer::Move(local),
            ESPressio::Memory::OwnershipTransfer::Move(remote)
        );
    }

    /// Family-owned semantic correlation for one exact remote Command recipient.
    ///
    /// The Delivery identifier Type is supplied by the integration adapter so EDP-Command remains
    /// Transport-neutral. Invocation, Cancellation and TerminalResult deliveries retain independent
    /// delivery lifecycles while referring to this same semantic value.
    template<class TDeliveryIdentifier>
    requires std::equality_comparable<TDeliveryIdentifier> &&
        std::is_nothrow_copy_constructible_v<TDeliveryIdentifier> &&
        requires(const TDeliveryIdentifier& left, const TDeliveryIdentifier& right) {
            { left == right } noexcept -> std::same_as<bool>;
        }
    class CommandInvocationCorrelation final {
    private:
        System::Identity::DeviceIdentifier _source;
        System::Identity::RuntimeIncarnationId _runtime;
        TDeliveryIdentifier _invocationDelivery;

    public:
        CommandInvocationCorrelation(
            const System::Identity::DeviceIdentifier& source,
            const System::Identity::RuntimeIncarnationId& runtime,
            const TDeliveryIdentifier& invocationDelivery
        ) noexcept :
            _source(source),
            _runtime(runtime),
            _invocationDelivery(invocationDelivery) {
        }

        [[nodiscard]] const System::Identity::DeviceIdentifier& Source() const noexcept {
            return _source;
        }

        [[nodiscard]] const System::Identity::RuntimeIncarnationId& Runtime() const noexcept {
            return _runtime;
        }

        [[nodiscard]] const TDeliveryIdentifier& InvocationDelivery() const noexcept {
            return _invocationDelivery;
        }

        [[nodiscard]] friend bool operator==(
            const CommandInvocationCorrelation&,
            const CommandInvocationCorrelation&
        ) noexcept = default;
    };

    /// Move-only source-facing semantic operation over a bounded frozen remote recipient set.
    ///
    /// TBinding is adapter-owned storage embedded by value in this family surface. It retains exact
    /// recipient/correlation state and implements generation-safe Observe, finite Wait, cooperative
    /// cancellation and at-most-once Response extraction. Invocation, Cancellation and TerminalResult
    /// wire deliveries remain separate adapter concerns; releasing this object abandons observation only.
    template<CommandType TCommand, class TBinding>
    requires std::is_nothrow_move_constructible_v<TBinding> &&
        std::is_nothrow_destructible_v<TBinding>
    class RemoteCommandOperation final {
    private:
        TBinding _binding;
        bool _valid{true};

        void EnsureValid() const noexcept {
            if (!_valid) {
                std::terminate();
            }
        }

    public:
        using Command = TCommand;
        using Binding = TBinding;

        static_assert(
            requires(TBinding& binding) {
                { binding.Release() } noexcept -> std::same_as<void>;
            },
            "Remote Command binding must provide non-throwing deterministic release"
        );

        explicit RemoteCommandOperation(TBinding binding) noexcept :
            _binding(ESPressio::Memory::OwnershipTransfer::Move(binding)) {
        }

        RemoteCommandOperation(const RemoteCommandOperation&) = delete;
        RemoteCommandOperation& operator=(const RemoteCommandOperation&) = delete;

        RemoteCommandOperation(RemoteCommandOperation&& other) noexcept :
            _binding(ESPressio::Memory::OwnershipTransfer::Move(other._binding)),
            _valid(other._valid) {
            other._valid = false;
        }

        RemoteCommandOperation& operator=(RemoteCommandOperation&&) = delete;

        ~RemoteCommandOperation() noexcept {
            Release();
        }

        /// Reports whether this object still retains adapter observation capability.
        [[nodiscard]] bool IsValid() const noexcept {
            return _valid;
        }

        /// Returns the number of recipients frozen into this exact operation.
        [[nodiscard]] std::size_t RecipientCount() const noexcept {
            EnsureValid();
            static_assert(noexcept(_binding.RecipientCount()));
            return _binding.RecipientCount();
        }

        /// Returns the binding-defined exact recipient/correlation value at index.
        [[nodiscard]] decltype(auto) Recipient(std::size_t index) const noexcept {
            EnsureValid();
            static_assert(noexcept(_binding.Recipient(index)));
            return _binding.Recipient(index);
        }

        /// Returns the binding-defined semantic observation for one recipient.
        [[nodiscard]] auto Observe(std::size_t index) const noexcept {
            EnsureValid();
            static_assert(noexcept(_binding.Observe(index)));
            return _binding.Observe(index);
        }

        /// Performs one finite binding-defined wait for a recipient's semantic progress.
        [[nodiscard]] auto WaitFor(std::size_t index, Duration duration) noexcept {
            EnsureValid();
            static_assert(noexcept(_binding.WaitFor(index, duration)));
            return _binding.WaitFor(index, duration);
        }

        /// Performs one finite binding-defined wait until a monotonic deadline.
        [[nodiscard]] auto WaitUntil(
            std::size_t index,
            MonotonicTimestamp deadline
        ) noexcept {
            EnsureValid();
            static_assert(noexcept(_binding.WaitUntil(index, deadline)));
            return _binding.WaitUntil(index, deadline);
        }

        /// Requests binding-defined cooperative cancellation for exactly one recipient.
        [[nodiscard]] auto RequestCancellation(std::size_t index) noexcept {
            EnsureValid();
            static_assert(noexcept(_binding.RequestCancellation(index)));
            return _binding.RequestCancellation(index);
        }

        /// Performs binding-enforced at-most-once Response extraction for one recipient.
        [[nodiscard]] auto TakeResponse(std::size_t index) noexcept {
            EnsureValid();
            static_assert(noexcept(_binding.TakeResponse(index)));
            return _binding.TakeResponse(index);
        }

        /// Populates one caller-owned existing Response during binding-enforced at-most-once extraction.
        ///
        /// This overload preserves valid serialisable Response Types which intentionally have no
        /// default constructor. The caller owns construction; the binding owns retained remote
        /// response bytes/state and consumes them only according to its at-most-once contract.
        [[nodiscard]] auto TakeResponse(
            std::size_t index,
            Response<TCommand>& destination
        ) noexcept {
            EnsureValid();
            static_assert(noexcept(_binding.TakeResponse(index, destination)));
            return _binding.TakeResponse(index, destination);
        }

        /// Abandons source observation and deterministically releases the bounded binding once.
        void Release() noexcept {
            if (!_valid) {
                return;
            }

            _binding.Release();
            _valid = false;
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
