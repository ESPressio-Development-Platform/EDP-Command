#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include <ESPressio_Command.hpp>

static_assert(
    std::is_same_v<
        ESPressio::Command::LocalOnly,
        ESPressio::Primitives::ExecutionDomain::LocalOnly
    >
);

static_assert(
    std::is_same_v<
        ESPressio::Command::RemoteOnly,
        ESPressio::Primitives::ExecutionDomain::RemoteOnly
    >
);

static_assert(
    std::is_same_v<
        ESPressio::Command::LocalAndRemote,
        ESPressio::Primitives::ExecutionDomain::LocalAndRemote
    >
);

namespace C = ESPressio::Command;

/// Test-local outcome for a selected local Dispatch operation.
enum class LocalOutcome : std::uint8_t {
    Accepted = 0U,
    Rejected = 1U
};

/// Test-local outcome for a selected remote Dispatch operation.
enum class RemoteOutcome : std::uint8_t {
    Accepted = 0U,
    Unavailable = 1U
};

/// Test-local Request used to verify explicit inbound local admission.
struct ScopeRequest final {
    /// Example payload value.
    int Value{0};
};

/// Test-local Command declaration used by InboundAdmission.
struct ScopeCommand final {
    /// Request Type used by this test Command.
    using Request = ScopeRequest;

    /// Response Type used by this test Command.
    using Response = void;
};

/// Minimal local Runtime substitute used only to observe inbound admission calls.
struct FakeRuntime final {
    // Admission observation state.

    /// Number of local admission attempts performed.
    std::size_t DispatchCount{0U};

    /// Most recently admitted payload value.
    int LastValue{0};

    // Local admission operation.

    /// Accepts one Request and records that local admission occurred.
    [[nodiscard]] LocalOutcome Dispatch(ScopeRequest request) noexcept {
        ++DispatchCount;
        LastValue = request.Value;
        return LocalOutcome::Accepted;
    }
};

/// Callable representing one already-selected local Dispatch operation.
struct LocalOperation final {
    // Operation state.

    /// Number of times the operation has been invoked.
    std::size_t* InvocationCount{nullptr};

    /// Result returned by the operation.
    LocalOutcome Result{LocalOutcome::Accepted};

    // Dispatch operation.

    /// Executes the selected local-domain operation once.
    [[nodiscard]] LocalOutcome operator()() noexcept {
        ++(*InvocationCount);
        return Result;
    }
};

/// Callable representing one already-selected remote Dispatch operation.
struct RemoteOperation final {
    // Operation state.

    /// Number of times the operation has been invoked.
    std::size_t* InvocationCount{nullptr};

    /// Result returned by the operation.
    RemoteOutcome Result{RemoteOutcome::Accepted};

    // Dispatch operation.

    /// Executes the selected remote-domain operation once.
    [[nodiscard]] RemoteOutcome operator()() noexcept {
        ++(*InvocationCount);
        return Result;
    }
};

/// Detects whether an inbound admission facade incorrectly accepts RemoteOnly scope.
/// @tparam TAdmission Inbound admission facade under test.
template<class TAdmission>
concept SupportsRemoteInbound = requires(
    TAdmission& admission,
    ScopeRequest request
) {
    admission.Dispatch(
        C::RemoteOnly{},
        request
    );
};

static_assert(C::ExecutionDomainScope<C::LocalOnly>);
static_assert(C::ExecutionDomainScope<C::RemoteOnly>);
static_assert(C::ExecutionDomainScope<C::LocalAndRemote>);
static_assert(!C::ExecutionDomainScope<int>);

using Inbound = C::InboundAdmission<ScopeCommand, FakeRuntime>;
static_assert(!SupportsRemoteInbound<Inbound>);

using CombinedResult = decltype(
    C::DispatchScoped(
        C::LocalAndRemote{},
        std::declval<LocalOperation&>(),
        std::declval<RemoteOperation&>()
    )
);
static_assert(std::is_same_v<decltype(std::declval<CombinedResult&>().Local()), LocalOutcome&>);
static_assert(std::is_same_v<decltype(std::declval<CombinedResult&>().Remote()), RemoteOutcome&>);

int main() {
    // LocalOnly invokes only the selected local operation and preserves its scalar result shape.
    std::size_t localCount = 0U;
    LocalOperation localOperation{&localCount, LocalOutcome::Accepted};
    const auto localResult = C::DispatchScoped(
        C::LocalOnly{},
        localOperation
    );
    assert(localResult == LocalOutcome::Accepted);
    assert(localCount == 1U);

    // RemoteOnly invokes only the selected remote operation and creates no local admission result.
    std::size_t remoteCount = 0U;
    RemoteOperation remoteOperation{&remoteCount, RemoteOutcome::Accepted};
    const auto remoteResult = C::DispatchScoped(
        C::RemoteOnly{},
        remoteOperation
    );
    assert(remoteResult == RemoteOutcome::Accepted);
    assert(remoteCount == 1U);

    // LocalAndRemote invokes both independently; a negative local result never suppresses remote participation.
    localCount = 0U;
    remoteCount = 0U;
    LocalOperation rejectedLocal{&localCount, LocalOutcome::Rejected};
    RemoteOperation acceptedRemote{&remoteCount, RemoteOutcome::Accepted};
    auto combined = C::DispatchScoped(
        C::LocalAndRemote{},
        rejectedLocal,
        acceptedRemote
    );
    assert(localCount == 1U);
    assert(remoteCount == 1U);
    assert(combined.Local() == LocalOutcome::Rejected);
    assert(combined.Remote() == RemoteOutcome::Accepted);

    // The opposite negative-domain case is independent as well.
    localCount = 0U;
    remoteCount = 0U;
    LocalOperation acceptedLocal{&localCount, LocalOutcome::Accepted};
    RemoteOperation unavailableRemote{&remoteCount, RemoteOutcome::Unavailable};
    auto remoteUnavailable = C::DispatchScoped(
        C::LocalAndRemote{},
        acceptedLocal,
        unavailableRemote
    );
    assert(localCount == 1U);
    assert(remoteCount == 1U);
    assert(remoteUnavailable.Local() == LocalOutcome::Accepted);
    assert(remoteUnavailable.Remote() == RemoteOutcome::Unavailable);

    // Inbound admission remains explicitly local-only and retains the original shorthand overload.
    FakeRuntime runtime;
    Inbound inbound(runtime);
    assert(inbound.Dispatch(ScopeRequest{17}) == LocalOutcome::Accepted);
    assert(inbound.Dispatch(
        C::LocalOnly{},
        ScopeRequest{23}
    ) == LocalOutcome::Accepted);
    assert(runtime.DispatchCount == 2U);
    assert(runtime.LastValue == 23);
}
