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
    using SuccessFn = bool (*)(void*, CompletionStatus, TResponse*) noexcept;
    using FailureFn = bool (*)(void*, ExecutionFailure) noexcept;
    using CancelFn = bool (*)(void*) noexcept;
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

    [[nodiscard]] bool Succeeded(TResponse response) noexcept {
        if (_used || _success == nullptr) return false; _used = true;
        return _success(_context, CompletionStatus::Succeeded, &response);
    }
    [[nodiscard]] bool Rejected(TResponse response) noexcept {
        if (_used || _success == nullptr) return false; _used = true;
        return _success(_context, CompletionStatus::Rejected, &response);
    }
    [[nodiscard]] bool Failed() noexcept {
        if (_used || _failure == nullptr) return false; _used = true;
        return _failure(_context, ExecutionFailure::IntegrationFailure);
    }
    [[nodiscard]] bool Cancelled() noexcept {
        if (_used || _cancel == nullptr) return false; _used = true;
        return _cancel(_context);
    }
};

template<>
class OutboundCompletion<void> final {
public:
    using SuccessFn = bool (*)(void*, CompletionStatus) noexcept;
    using FailureFn = bool (*)(void*, ExecutionFailure) noexcept;
    using CancelFn = bool (*)(void*) noexcept;
private:
    void* _context{nullptr}; SuccessFn _success{nullptr}; FailureFn _failure{nullptr}; CancelFn _cancel{nullptr}; bool _used{false};
public:
    OutboundCompletion(void* context, SuccessFn success, FailureFn failure, CancelFn cancel) noexcept : _context(context), _success(success), _failure(failure), _cancel(cancel) {}
    [[nodiscard]] bool Succeeded() noexcept { if (_used || !_success) return false; _used=true; return _success(_context, CompletionStatus::Succeeded); }
    [[nodiscard]] bool Rejected() noexcept { if (_used || !_success) return false; _used=true; return _success(_context, CompletionStatus::Rejected); }
    [[nodiscard]] bool Failed() noexcept { if (_used || !_failure) return false; _used=true; return _failure(_context, ExecutionFailure::IntegrationFailure); }
    [[nodiscard]] bool Cancelled() noexcept { if (_used || !_cancel) return false; _used=true; return _cancel(_context); }
};

template<class TCommand, class TCompletion>
struct OutboundInvocation final {
    const Request<TCommand>& RequestView;
    CancellationToken Cancellation;
    TCompletion Completion;
};

} // ESPressio::Command
