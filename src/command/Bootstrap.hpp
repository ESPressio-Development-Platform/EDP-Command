#pragma once

#include <type_traits>

#include <ESPressio_System.hpp>
#include <ESPressio_Threading.hpp>

#include "Composition.hpp"
#include "Runtime.hpp"

namespace ESPressio::Command {

    /// Application-owned Command binding resolved from the complete cross-domain Architecture.
    ///
    /// @tparam TCommand Command type bound by this Bootstrap.
    /// @tparam TArchitecture Complete immutable application Architecture used to resolve providers.
    /// @tparam TPlan Compile-time Command resource plan.
    template<class TCommand, class TArchitecture, class TPlan>
    requires CommandType<TCommand>
    class Bootstrap final {
    public:
        // Resolved Command and provider vocabulary.

        /// Command type bound by this Bootstrap.
        using Command = TCommand;

        /// Complete application Architecture used for static provider resolution.
        using ArchitectureType = TArchitecture;

        /// Compile-time resource plan used by the Runtime.
        using Plan = TPlan;

        /// Unique application-owned Handler provider resolved for this Command.
        using HandlerProvider = Composition::HandlerProvider<TCommand, TArchitecture>;

        /// Unique application-owned bounded wait/wake provider resolved from Threading.
        using WaitProvider = Composition::WaitProvider<TArchitecture>;

        /// Concrete Runtime type established by the resolved providers and resource plan.
        using RuntimeType = Runtime<TCommand, HandlerProvider, WaitProvider, TPlan>;

        // Compile-time Architecture validation.

        static_assert(
            TArchitecture::IsValid,
            "Command Bootstrap requires a valid EDP-System Architecture"
        );

        static_assert(
            WaitProvider::Capacity >= Plan::InvocationCapacity,
            "Command wait provider capacity must cover every bounded invocation record"
        );

    private:
        // Borrowed provider bindings and owned Runtime wiring.

        /// Application-owned Handler provider retained for the Bootstrap lifetime.
        HandlerProvider* _handler{nullptr};

        /// Application-owned Threading wait provider retained for the Bootstrap lifetime.
        WaitProvider* _waitProvider{nullptr};

        /// Command Runtime wired to the resolved application-owned providers.
        RuntimeType _runtime;

    public:
        // Construction and ownership.

        /// Establishes immutable Runtime wiring over persistent application-owned providers.
        Bootstrap(
            HandlerProvider& handler,
            WaitProvider& waitProvider
        ) noexcept :
            _handler(&handler),
            _waitProvider(&waitProvider),
            _runtime(
                handler,
                waitProvider
            ) {
        }

        /// Bootstrap cannot be copied because it owns Runtime identity and borrowed provider bindings.
        Bootstrap(const Bootstrap&) = delete;

        /// Bootstrap cannot be copy-assigned because it owns Runtime identity and borrowed provider bindings.
        Bootstrap& operator=(const Bootstrap&) = delete;

        /// Bootstrap cannot be moved because Runtime and Handle identity depend on a stable address.
        Bootstrap(Bootstrap&&) = delete;

        /// Bootstrap cannot be move-assigned because Runtime and Handle identity depend on a stable address.
        Bootstrap& operator=(Bootstrap&&) = delete;

        // Lifecycle operations.

        /// Initializes the bound Runtime without mutating application topology.
        [[nodiscard]] InitializationResult Initialize() noexcept {
            return _runtime.Initialize();
        }

        // Bound-instance accessors.

        /// Returns the mutable Runtime instance owned by this Bootstrap.
        [[nodiscard]] RuntimeType& RuntimeInstance() noexcept {
            return _runtime;
        }

        /// Returns the immutable Runtime instance owned by this Bootstrap.
        [[nodiscard]] const RuntimeType& RuntimeInstance() const noexcept {
            return _runtime;
        }

        /// Returns the application-owned Handler provider bound to this Bootstrap.
        [[nodiscard]] HandlerProvider& HandlerInstance() noexcept {
            return *_handler;
        }

        /// Returns the application-owned bounded wait/wake provider bound to this Bootstrap.
        [[nodiscard]] WaitProvider& WaitProviderInstance() noexcept {
            return *_waitProvider;
        }
    };

} // ESPressio::Command
