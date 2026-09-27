#pragma once

#include <type_traits>

#include <ESPressio_System.hpp>
#include <ESPressio_Threading.hpp>

#include "Composition.hpp"
#include "Runtime.hpp"

namespace ESPressio::Command {

template<class TCommand, class TComposition, class TPlan>
requires CommandType<TCommand>
class Bootstrap final {
public:
    using Command = TCommand;
    using CompositionType = TComposition;
    using Plan = TPlan;
    using HandlerProvider = Composition::HandlerProvider<TCommand, TComposition>;
    using WaitProvider = Composition::WaitProvider<TComposition>;
    using RuntimeType = Runtime<TCommand, HandlerProvider, WaitProvider, TPlan>;

    static_assert(
        WaitProvider::Capacity >= Plan::InvocationCapacity,
        "Command wait provider capacity must cover every bounded invocation record"
    );

private:
    HandlerProvider* _handler{nullptr};
    WaitProvider* _waitProvider{nullptr};
    RuntimeType _runtime;

public:
    Bootstrap(HandlerProvider& handler, WaitProvider& waitProvider) noexcept :
        _handler(&handler),
        _waitProvider(&waitProvider),
        _runtime(handler, waitProvider) {}

    Bootstrap(const Bootstrap&) = delete;
    Bootstrap& operator=(const Bootstrap&) = delete;
    Bootstrap(Bootstrap&&) = delete;
    Bootstrap& operator=(Bootstrap&&) = delete;

    [[nodiscard]] bool Initialize() noexcept { return _runtime.Initialize(); }
    [[nodiscard]] RuntimeType& RuntimeInstance() noexcept { return _runtime; }
    [[nodiscard]] const RuntimeType& RuntimeInstance() const noexcept { return _runtime; }
    [[nodiscard]] HandlerProvider& HandlerInstance() noexcept { return *_handler; }
    [[nodiscard]] WaitProvider& WaitProviderInstance() noexcept { return *_waitProvider; }
};

} // ESPressio::Command
