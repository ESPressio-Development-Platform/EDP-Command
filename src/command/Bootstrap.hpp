#pragma once

#include <type_traits>

#include <ESPressio_System.hpp>

#include "Composition.hpp"
#include "Runtime.hpp"

namespace ESPressio::Command {

/// Application-owned binding between a validated Command Composition, its persistent
/// handler provider instance and the fixed-capacity Command Runtime.
///
/// Bootstrap performs no runtime discovery and owns neither the handler nor any external
/// provider. The application constructs persistent providers first, then this binding,
/// then explicitly initializes the Runtime. The compile-time Composition fixes the
/// topology for the lifetime of the application.
template<class TCommand, class TComposition, class TPlan>
requires CommandType<TCommand>
class Bootstrap final {
public:
    using Command = TCommand;
    using CompositionType = TComposition;
    using Plan = TPlan;
    using HandlerProvider = Composition::HandlerProvider<TCommand, TComposition>;
    using RuntimeType = Runtime<TCommand, HandlerProvider, TPlan>;

private:
    HandlerProvider* _handler{nullptr};
    RuntimeType _runtime;

public:
    explicit Bootstrap(HandlerProvider& handler) noexcept :
        _handler(&handler),
        _runtime(handler) {}

    Bootstrap(const Bootstrap&) = delete;
    Bootstrap& operator=(const Bootstrap&) = delete;
    Bootstrap(Bootstrap&&) = delete;
    Bootstrap& operator=(Bootstrap&&) = delete;

    [[nodiscard]] bool Initialize() noexcept {
        return _runtime.Initialize();
    }

    [[nodiscard]] RuntimeType& RuntimeInstance() noexcept {
        return _runtime;
    }

    [[nodiscard]] const RuntimeType& RuntimeInstance() const noexcept {
        return _runtime;
    }

    [[nodiscard]] HandlerProvider& HandlerInstance() noexcept {
        return *_handler;
    }
};

} // ESPressio::Command
