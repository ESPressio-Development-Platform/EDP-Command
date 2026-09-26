#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <ESPressio_Memory.hpp>

#include "CommandTypes.hpp"

namespace ESPressio::Command {

template<class TResponse>
class TakeResponseResult final {
    TakeResponseStatus _status;
    alignas(TResponse) std::byte _storage[sizeof(TResponse)];
    bool _live{false};
public:
    explicit TakeResponseResult(TakeResponseStatus status) noexcept : _status(status) {}
    TakeResponseResult(const TakeResponseResult&) = delete;
    TakeResponseResult& operator=(const TakeResponseResult&) = delete;
    ~TakeResponseResult() { if (_live) ESPressio::Memory::ObjectLifetime::Destroy(*reinterpret_cast<TResponse*>(_storage)); }
    [[nodiscard]] TakeResponseStatus Status() const noexcept { return _status; }
    [[nodiscard]] bool HasValue() const noexcept { return _live; }
    void MarkLive() noexcept { _live = true; }
    [[nodiscard]] void* Storage() noexcept { return _storage; }
    TResponse Take() noexcept {
        auto& value = *reinterpret_cast<TResponse*>(_storage);
        TResponse result(ESPressio::Memory::OwnershipTransfer::Move(value));
        ESPressio::Memory::ObjectLifetime::Destroy(value);
        _live = false;
        return result;
    }
};

template<class TCommand, class TRuntime>
class Handle final {
    TRuntime* _runtime{nullptr};
    std::size_t _index{0U};
    std::uint32_t _generation{0U};

    friend class DispatchResult<TCommand, TRuntime>;
    Handle(TRuntime& runtime, std::size_t index, std::uint32_t generation) noexcept :
        _runtime(&runtime), _index(index), _generation(generation) {}
public:
    Handle() noexcept = default;
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    Handle(Handle&& other) noexcept : _runtime(other._runtime), _index(other._index), _generation(other._generation) { other._runtime = nullptr; }
    Handle& operator=(Handle&& other) noexcept {
        if (this == &other) return *this;
        Release(); _runtime = other._runtime; _index = other._index; _generation = other._generation; other._runtime = nullptr; return *this;
    }
    ~Handle() { Release(); }

    [[nodiscard]] bool IsValid() const noexcept {
        if (_runtime == nullptr) return false;
        bool valid = false; static_cast<void>(_runtime->Observe(_index, _generation, valid)); return valid;
    }
    [[nodiscard]] InvocationObservation State(bool& valid) const noexcept {
        if (_runtime == nullptr) { valid = false; return {}; }
        return _runtime->Observe(_index, _generation, valid);
    }
    [[nodiscard]] CancellationRequestResult RequestCancellation() noexcept {
        return _runtime == nullptr ? CancellationRequestResult::InvalidHandle : _runtime->RequestCancellation(_index, _generation);
    }

    template<class R = Response<TCommand>>
    requires (!std::is_void_v<R>)
    [[nodiscard]] TakeResponseResult<R> TakeResponse() noexcept {
        if (_runtime == nullptr) return TakeResponseResult<R>(TakeResponseStatus::InvalidHandle);
        alignas(R) std::byte temporary[sizeof(R)];
        const auto status = _runtime->template TakeResponse<R>(_index, _generation, temporary);
        TakeResponseResult<R> result(status);
        if (status == TakeResponseStatus::Taken) {
            auto& value = *reinterpret_cast<R*>(temporary);
            static_cast<void>(ESPressio::Memory::ObjectLifetime::MoveConstruct<R>(result.Storage(), value));
            ESPressio::Memory::ObjectLifetime::Destroy(value);
            result.MarkLive();
        }
        return result;
    }

    void Release() noexcept {
        if (_runtime == nullptr) return;
        _runtime->Release(_index, _generation);
        _runtime = nullptr;
    }
};

template<class TCommand, class TRuntime>
class DispatchResult final {
    bool _accepted{false};
    DispatchFailure _failure{DispatchFailure::NoCapacity};
    Handle<TCommand, TRuntime> _handle{};
public:
    explicit DispatchResult(DispatchFailure failure) noexcept : _failure(failure) {}
    DispatchResult(TRuntime& runtime, std::size_t index, std::uint32_t generation) noexcept :
        _accepted(true), _handle(runtime, index, generation) {}
    DispatchResult(const DispatchResult&) = delete;
    DispatchResult& operator=(const DispatchResult&) = delete;
    DispatchResult(DispatchResult&&) noexcept = default;
    [[nodiscard]] bool Accepted() const noexcept { return _accepted; }
    [[nodiscard]] DispatchFailure Failure() const noexcept { return _failure; }
    Handle<TCommand, TRuntime> TakeHandle() noexcept { return ESPressio::Memory::OwnershipTransfer::Move(_handle); }
};

} // ESPressio::Command
