#pragma once

#include <cstddef>
#include <cstdint>
#include <exception>
#include <type_traits>

#include <ESPressio_Memory.hpp>

#include "CommandTypes.hpp"

namespace ESPressio::Command {
    /**
     * @brief Forward declaration of the typed dispatch operation result.
     *
     * @tparam TCommand Command type represented by the dispatch.
     * @tparam TRuntime Runtime type owning the admitted invocation.
     */
    template<class TCommand, class TRuntime>
    class DispatchResult;

    template<class TCommand, class TRuntime>
    class RemoteHandoffReservationResult;

    /**
     * @brief Move-only owner for a Response extracted from a terminal invocation.
     *
     * @tparam TResponse Response type stored by the result.
     */
    template<class TResponse>
    class TakeResponseResult final {
    private:
        TakeResponseStatus _status;
        alignas(TResponse) std::byte _storage[sizeof(TResponse)];
        bool _live{false};

    public:
        /**
         * @brief Creates a response-extraction result with no live Response.
         */
        explicit TakeResponseResult(TakeResponseStatus status) noexcept :
            _status(status) {
        }

        TakeResponseResult(const TakeResponseResult&) = delete;
        TakeResponseResult& operator=(const TakeResponseResult&) = delete;

        /**
         * @brief Transfers any embedded Response from another result.
         */
        TakeResponseResult(TakeResponseResult&& other) noexcept :
            _status(other._status) {
            if (other._live) {
                auto& value = *reinterpret_cast<TResponse*>(other._storage);
                static_cast<void>(
                    ESPressio::Memory::ObjectLifetime::MoveConstruct<TResponse>(
                        _storage,
                        value
                    )
                );
                ESPressio::Memory::ObjectLifetime::Destroy(value);
                _live = true;
                other._live = false;
            }
        }

        /**
         * @brief Destroys any Response still retained by this result.
         */
        ~TakeResponseResult() {
            if (_live) {
                ESPressio::Memory::ObjectLifetime::Destroy(
                    *reinterpret_cast<TResponse*>(_storage)
                );
            }
        }

        /**
         * @brief Returns the operational extraction status.
         */
        [[nodiscard]] TakeResponseStatus Status() const noexcept {
            return _status;
        }

        /**
         * @brief Reports whether this result currently owns a Response value.
         */
        [[nodiscard]] bool HasValue() const noexcept {
            return _live;
        }

        /**
         * @brief Marks the embedded storage as containing a live Response.
         *
         * A valid TResponse must already have been constructed in Storage().
         */
        void MarkLive() noexcept {
            _live = true;
        }

        /**
         * @brief Returns raw storage used by the Runtime extraction path.
         */
        [[nodiscard]] void* Storage() noexcept {
            return _storage;
        }

        /**
         * @brief Transfers the embedded Response to the caller.
         */
        TResponse Take() noexcept {
            auto& value = *reinterpret_cast<TResponse*>(_storage);
            TResponse result(ESPressio::Memory::OwnershipTransfer::Move(value));
            ESPressio::Memory::ObjectLifetime::Destroy(value);
            _live = false;
            return result;
        }
    };

    /**
     * @brief Exclusive move-only capability for one admitted Command invocation.
     *
     * @tparam TCommand Command type represented by the invocation.
     * @tparam TRuntime Runtime type owning the invocation record.
     */
    template<class TCommand, class TRuntime>
    class Handle final {
    private:
        TRuntime* _runtime{nullptr};
        std::size_t _index{0U};
        std::uint32_t _generation{0U};

        friend class DispatchResult<TCommand, TRuntime>;

        /**
         * @brief Creates a retained Handle for a successful dispatch.
         */
        Handle(
            TRuntime& runtime,
            std::size_t index,
            std::uint32_t generation
        ) noexcept :
            _runtime(&runtime),
            _index(index),
            _generation(generation) {
        }

    public:
        Handle() noexcept = default;
        Handle(const Handle&) = delete;
        Handle& operator=(const Handle&) = delete;

        /**
         * @brief Transfers exclusive invocation retention from another Handle.
         */
        Handle(Handle&& other) noexcept :
            _runtime(other._runtime),
            _index(other._index),
            _generation(other._generation) {
            other._runtime = nullptr;
        }

        /**
         * @brief Releases any current invocation then transfers another Handle.
         */
        Handle& operator=(Handle&& other) noexcept {
            if (this == &other) {
                return *this;
            }

            Release();
            _runtime = other._runtime;
            _index = other._index;
            _generation = other._generation;
            other._runtime = nullptr;
            return *this;
        }

        /**
         * @brief Releases this Handle's invocation retention.
         */
        ~Handle() {
            Release();
        }

        /**
         * @brief Reports whether this Handle still identifies a live Runtime record.
         */
        [[nodiscard]] bool IsValid() const noexcept {
            if (_runtime == nullptr) {
                return false;
            }

            bool valid = false;
            static_cast<void>(_runtime->Observe(_index, _generation, valid));
            return valid;
        }

        /**
         * @brief Returns a value snapshot of the invocation state.
         *
         * @param valid Receives whether the Handle identity was valid.
         */
        [[nodiscard]] InvocationObservation State(bool& valid) const noexcept {
            if (_runtime == nullptr) {
                valid = false;
                return {};
            }

            return _runtime->Observe(_index, _generation, valid);
        }

        /**
         * @brief Waits for at most the supplied finite duration.
         */
        [[nodiscard]] WaitResult WaitFor(Duration duration) noexcept {
            return _runtime == nullptr
                ? WaitResult::InvalidHandle
                : _runtime->WaitFor(_index, _generation, duration);
        }

        /**
         * @brief Waits until the supplied finite monotonic deadline.
         */
        [[nodiscard]] WaitResult WaitUntil(MonotonicTimestamp deadline) noexcept {
            return _runtime == nullptr
                ? WaitResult::InvalidHandle
                : _runtime->WaitUntil(_index, _generation, deadline);
        }

        /**
         * @brief Requests cooperative cancellation of the invocation.
         */
        [[nodiscard]] CancellationRequestResult RequestCancellation() noexcept {
            return _runtime == nullptr
                ? CancellationRequestResult::InvalidHandle
                : _runtime->RequestCancellation(_index, _generation);
        }

        /**
         * @brief Extracts a non-void Response from a terminal invocation.
         *
         * @tparam R Response type selected from TCommand.
         */
        template<class R = Response<TCommand>>
        requires (!std::is_void_v<R>)
        [[nodiscard]] TakeResponseResult<R> TakeResponse() noexcept {
            if (_runtime == nullptr) {
                return TakeResponseResult<R>(TakeResponseStatus::InvalidHandle);
            }

            alignas(R) std::byte temporary[sizeof(R)];
            const auto status = _runtime->template TakeResponse<R>(
                _index,
                _generation,
                temporary
            );
            TakeResponseResult<R> result(status);

            if (status == TakeResponseStatus::Taken) {
                auto& value = *reinterpret_cast<R*>(temporary);
                static_cast<void>(
                    ESPressio::Memory::ObjectLifetime::MoveConstruct<R>(
                        result.Storage(),
                        value
                    )
                );
                ESPressio::Memory::ObjectLifetime::Destroy(value);
                result.MarkLive();
            }

            return result;
        }

        /**
         * @brief Releases invocation retention and invalidates this Handle.
         */
        void Release() noexcept {
            if (_runtime == nullptr) {
                return;
            }

            _runtime->Release(_index, _generation);
            _runtime = nullptr;
        }
    };

    /**
     * @brief Move-only result of attempting to dispatch a typed Command.
     *
     * @tparam TCommand Command type represented by the dispatch.
     * @tparam TRuntime Runtime type owning a successfully admitted invocation.
     */
    template<class TCommand, class TRuntime>
    class DispatchResult final {
    private:
        bool _accepted{false};
        DispatchFailure _failure{DispatchFailure::NoCapacity};
        Handle<TCommand, TRuntime> _handle{};

    public:
        /**
         * @brief Creates a rejected dispatch result.
         */
        explicit DispatchResult(DispatchFailure failure) noexcept :
            _failure(failure) {
        }

        /**
         * @brief Creates an accepted dispatch result retaining its Handle.
         */
        DispatchResult(
            TRuntime& runtime,
            std::size_t index,
            std::uint32_t generation
        ) noexcept :
            _accepted(true),
            _handle(runtime, index, generation) {
        }

        DispatchResult(const DispatchResult&) = delete;
        DispatchResult& operator=(const DispatchResult&) = delete;
        DispatchResult(DispatchResult&&) noexcept = default;

        /**
         * @brief Reports whether dispatch admitted an invocation.
         */
        [[nodiscard]] bool Accepted() const noexcept {
            return _accepted;
        }

        /**
         * @brief Returns the rejection reason when dispatch was not accepted.
         */
        [[nodiscard]] DispatchFailure Failure() const noexcept {
            return _failure;
        }

        /**
         * @brief Transfers the exclusive Handle for an accepted invocation.
         */
        Handle<TCommand, TRuntime> TakeHandle() noexcept {
            return ESPressio::Memory::OwnershipTransfer::Move(_handle);
        }
    };

    /// Move-only owner of one constructed but unpublished inbound Command Request.
    ///
    /// The reservation guarantees one invocation record and one queue entitlement. A decoder may
    /// populate Value() without holding Command synchronization. Commit is the only operation that
    /// publishes the Request to the execution queue; destruction aborts an uncommitted reservation.
    ///
    /// @tparam TCommand Command declaration represented by the reservation.
    /// @tparam TRuntime Runtime type owning the unpublished invocation record.
    template<class TCommand, class TRuntime>
    class InboundReservation final {
    private:
        TRuntime* _runtime{nullptr};
        std::size_t _index{0U};
        std::uint32_t _generation{0U};

        template<class, class>
        friend class InboundReservationResult;

        InboundReservation(
            TRuntime& runtime,
            std::size_t index,
            std::uint32_t generation
        ) noexcept :
            _runtime(&runtime),
            _index(index),
            _generation(generation) {
        }

    public:
        InboundReservation() noexcept = default;
        InboundReservation(const InboundReservation&) = delete;
        InboundReservation& operator=(const InboundReservation&) = delete;

        InboundReservation(InboundReservation&& other) noexcept :
            _runtime(other._runtime),
            _index(other._index),
            _generation(other._generation) {
            other._runtime = nullptr;
        }

        InboundReservation& operator=(InboundReservation&& other) noexcept {
            if (this == &other) {
                return *this;
            }

            Abort();
            _runtime = other._runtime;
            _index = other._index;
            _generation = other._generation;
            other._runtime = nullptr;
            return *this;
        }

        ~InboundReservation() noexcept {
            Abort();
        }

        /// Reports whether this object still owns an unpublished Runtime reservation.
        [[nodiscard]] bool IsValid() const noexcept {
            return _runtime != nullptr;
        }

        /// Returns the exclusively owned unpublished Request destination for transactional population.
        [[nodiscard]] Request<TCommand>& Value() noexcept {
            return _runtime->IngressRequest(_index, _generation);
        }

        /// Atomically publishes the populated Request to the execution queue and returns its Handle.
        [[nodiscard]] DispatchResult<TCommand, TRuntime> Commit() noexcept {
            if (_runtime == nullptr) {
                return DispatchResult<TCommand, TRuntime>(DispatchFailure::BindingUnavailable);
            }

            auto* runtime = _runtime;
            _runtime = nullptr;
            return runtime->CommitIngress(_index, _generation);
        }

        /// Releases the unpublished Request and every reserved entitlement without publishing it.
        void Abort() noexcept {
            if (_runtime == nullptr) {
                return;
            }

            _runtime->AbortIngress(_index, _generation);
            _runtime = nullptr;
        }
    };

    /// Move-only result of attempting to reserve inbound Command admission backing.
    ///
    /// @tparam TCommand Command declaration represented by the reservation.
    /// @tparam TRuntime Runtime type owning a successful reservation.
    template<class TCommand, class TRuntime>
    class InboundReservationResult final {
    private:
        bool _accepted{false};
        DispatchFailure _failure{DispatchFailure::NoCapacity};
        InboundReservation<TCommand, TRuntime> _reservation{};

    public:
        explicit InboundReservationResult(DispatchFailure failure) noexcept :
            _failure(failure) {
        }

        InboundReservationResult(
            TRuntime& runtime,
            std::size_t index,
            std::uint32_t generation
        ) noexcept :
            _accepted(true),
            _reservation(runtime, index, generation) {
        }

        InboundReservationResult(const InboundReservationResult&) = delete;
        InboundReservationResult& operator=(const InboundReservationResult&) = delete;
        InboundReservationResult(InboundReservationResult&&) noexcept = default;
        InboundReservationResult& operator=(InboundReservationResult&&) = delete;

        /// Reports whether exact unpublished admission backing was reserved.
        [[nodiscard]] bool Accepted() const noexcept {
            return _accepted;
        }

        /// Returns the reservation failure when Accepted() is false.
        [[nodiscard]] DispatchFailure Failure() const noexcept {
            return _failure;
        }

        /// Transfers exclusive ownership of the unpublished admission reservation.
        [[nodiscard]] InboundReservation<TCommand, TRuntime> TakeReservation() && noexcept {
            return ESPressio::Memory::OwnershipTransfer::Move(_reservation);
        }
    };

    /// Move-only owner of one synchronized outbound Request handoff stage.
    ///
    /// The stage retains only a bounded borrow of the caller-owned Request. Commit invokes the selected
    /// remote adapter exactly once outside Command synchronization; destruction releases an uncommitted
    /// stage without calling the adapter. The Request must remain alive and immutable through Commit/Abort.
    template<class TCommand, class TRuntime>
    class RemoteHandoffReservation final {
    private:
        TRuntime* _runtime{nullptr};
        std::size_t _index{0U};
        std::uint32_t _generation{0U};

        template<class, class>
        friend class RemoteHandoffReservationResult;

        RemoteHandoffReservation(
            TRuntime& runtime,
            std::size_t index,
            std::uint32_t generation
        ) noexcept :
            _runtime(&runtime),
            _index(index),
            _generation(generation) {
        }

    public:
        RemoteHandoffReservation() noexcept = default;
        RemoteHandoffReservation(const RemoteHandoffReservation&) = delete;
        RemoteHandoffReservation& operator=(const RemoteHandoffReservation&) = delete;

        RemoteHandoffReservation(RemoteHandoffReservation&& other) noexcept :
            _runtime(other._runtime),
            _index(other._index),
            _generation(other._generation) {
            other._runtime = nullptr;
        }

        RemoteHandoffReservation& operator=(RemoteHandoffReservation&& other) noexcept {
            if (this == &other) {
                return *this;
            }

            Abort();
            _runtime = other._runtime;
            _index = other._index;
            _generation = other._generation;
            other._runtime = nullptr;
            return *this;
        }

        ~RemoteHandoffReservation() noexcept {
            Abort();
        }

        /// Reports whether this object still owns a synchronized outbound stage.
        [[nodiscard]] bool IsValid() const noexcept {
            return _runtime != nullptr;
        }

        /// Performs one bounded adapter call outside Command synchronization and releases the stage.
        template<class TRemoteOperation>
        [[nodiscard]] auto Commit(TRemoteOperation& remoteOperation) noexcept {
            if (_runtime == nullptr) {
                std::terminate();
            }

            auto* runtime = _runtime;
            _runtime = nullptr;
            return runtime->CommitRemoteHandoff(
                _index,
                _generation,
                remoteOperation
            );
        }

        /// Releases this stage without invoking the remote adapter.
        void Abort() noexcept {
            if (_runtime == nullptr) {
                return;
            }

            _runtime->AbortRemoteHandoff(_index, _generation);
            _runtime = nullptr;
        }
    };

    /// Move-only result of attempting to reserve a synchronized outbound Request handoff stage.
    template<class TCommand, class TRuntime>
    class RemoteHandoffReservationResult final {
    private:
        bool _accepted{false};
        DispatchFailure _failure{DispatchFailure::NoCapacity};
        RemoteHandoffReservation<TCommand, TRuntime> _reservation{};

    public:
        explicit RemoteHandoffReservationResult(DispatchFailure failure) noexcept :
            _failure(failure) {
        }

        RemoteHandoffReservationResult(
            TRuntime& runtime,
            std::size_t index,
            std::uint32_t generation
        ) noexcept :
            _accepted(true),
            _reservation(runtime, index, generation) {
        }

        RemoteHandoffReservationResult(const RemoteHandoffReservationResult&) = delete;
        RemoteHandoffReservationResult& operator=(const RemoteHandoffReservationResult&) = delete;
        RemoteHandoffReservationResult(RemoteHandoffReservationResult&&) noexcept = default;
        RemoteHandoffReservationResult& operator=(RemoteHandoffReservationResult&&) = delete;

        /// Reports whether an exact outbound stage was reserved.
        [[nodiscard]] bool Accepted() const noexcept {
            return _accepted;
        }

        /// Returns the reservation failure when Accepted() is false.
        [[nodiscard]] DispatchFailure Failure() const noexcept {
            return _failure;
        }

        /// Transfers exclusive ownership of the synchronized outbound stage.
        [[nodiscard]] RemoteHandoffReservation<TCommand, TRuntime> TakeReservation() && noexcept {
            return ESPressio::Memory::OwnershipTransfer::Move(_reservation);
        }
    };
}
