#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "CommandTypes.hpp"

namespace ESPressio::Command {

/// Typed inbound admission facade. Remote correlation remains owned by the adapter.
template<class TCommand, class TRuntime>
class InboundAdmission final {
    TRuntime* _runtime;
public:
    explicit InboundAdmission(TRuntime& runtime) noexcept : _runtime(&runtime) {}
    [[nodiscard]] auto Dispatch(Request<TCommand> request) noexcept {
        return _runtime->Dispatch(ESPressio::Memory::OwnershipTransfer::Move(request));
    }
};

/// Invocation-specific outbound completion capability. It is semantically single-use.
template<class TResponse>
class OutboundCompletion final {
public:
    using SuccessFn = CompletionPublicationResult (*)(void*, Outcome, TResponse*) noexcept;
    using FailureFn = CompletionPublicationResult (*)(void*, ExecutionFailure) noexcept;
    using CancelFn = CompletionPublicationResult (*)(void*) noexcept;
private:
    void* _context{nullptr};
    SuccessFn _success{nullptr};
    FailureFn _failure{nullptr};
    CancelFn _cancel{nullptr};
    bool _used{false};
public:
    OutboundCompletion(void* context, SuccessFn success, FailureFn failure, CancelFn cancel) noexcept :
        _context(context), _success(success), _failure(failure), _cancel(cancel) {}
    OutboundCompletion(const OutboundCompletion&) = delete;
    OutboundCompletion& operator=(const OutboundCompletion&) = delete;

    [[nodiscard]] CompletionPublicationResult Succeeded(TResponse response) noexcept {
        if (_used) return CompletionPublicationResult::AlreadyCompleted;
        if (_success == nullptr) return CompletionPublicationResult::Unavailable;
        _used = true;
        return _success(_context, Outcome::Succeeded, &response);
    }
    [[nodiscard]] CompletionPublicationResult Rejected(TResponse response) noexcept {
        if (_used) return CompletionPublicationResult::AlreadyCompleted;
        if (_success == nullptr) return CompletionPublicationResult::Unavailable;
        _used = true;
        return _success(_context, Outcome::Rejected, &response);
    }
    [[nodiscard]] CompletionPublicationResult Failed() noexcept {
        if (_used) return CompletionPublicationResult::AlreadyCompleted;
        if (_failure == nullptr) return CompletionPublicationResult::Unavailable;
        _used = true;
        return _failure(_context, ExecutionFailure::IntegrationFailure);
    }
    [[nodiscard]] CompletionPublicationResult Cancelled() noexcept {
        if (_used) return CompletionPublicationResult::AlreadyCompleted;
        if (_cancel == nullptr) return CompletionPublicationResult::Unavailable;
        _used = true;
        return _cancel(_context);
    }
};

template<>
class OutboundCompletion<void> final {
public:
    using SuccessFn = CompletionPublicationResult (*)(void*, Outcome) noexcept;
    using FailureFn = CompletionPublicationResult (*)(void*, ExecutionFailure) noexcept;
    using CancelFn = CompletionPublicationResult (*)(void*) noexcept;
private:
    void* _context{nullptr};
    SuccessFn _success{nullptr};
    FailureFn _failure{nullptr};
    CancelFn _cancel{nullptr};
    bool _used{false};
public:
    OutboundCompletion(void* context, SuccessFn success, FailureFn failure, CancelFn cancel) noexcept :
        _context(context), _success(success), _failure(failure), _cancel(cancel) {}
    [[nodiscard]] CompletionPublicationResult Succeeded() noexcept {
        if (_used) return CompletionPublicationResult::AlreadyCompleted;
        if (_success == nullptr) return CompletionPublicationResult::Unavailable;
        _used = true;
        return _success(_context, Outcome::Succeeded);
    }
    [[nodiscard]] CompletionPublicationResult Rejected() noexcept {
        if (_used) return CompletionPublicationResult::AlreadyCompleted;
        if (_success == nullptr) return CompletionPublicationResult::Unavailable;
        _used = true;
        return _success(_context, Outcome::Rejected);
    }
    [[nodiscard]] CompletionPublicationResult Failed() noexcept {
        if (_used) return CompletionPublicationResult::AlreadyCompleted;
        if (_failure == nullptr) return CompletionPublicationResult::Unavailable;
        _used = true;
        return _failure(_context, ExecutionFailure::IntegrationFailure);
    }
    [[nodiscard]] CompletionPublicationResult Cancelled() noexcept {
        if (_used) return CompletionPublicationResult::AlreadyCompleted;
        if (_cancel == nullptr) return CompletionPublicationResult::Unavailable;
        _used = true;
        return _cancel(_context);
    }
};

template<class TCommand, class TCompletion>
struct OutboundInvocation final {
    const Request<TCommand>& RequestView;
    CancellationToken Cancellation;
    TCompletion Completion;
};

} // ESPressio::Command
