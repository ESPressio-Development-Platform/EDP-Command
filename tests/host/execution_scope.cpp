#include <cassert>
#include <array>
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
namespace S = ESPressio::System;

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

struct DeliveryIdentifier final {
    std::uint32_t Value{0U};

    [[nodiscard]] friend bool operator==(
        const DeliveryIdentifier&,
        const DeliveryIdentifier&
    ) noexcept = default;
};

/// Test-local Request used to verify explicit inbound local admission.
struct ScopeRequest final {
    static constexpr S::TypeIdentifier Identifier{
        S::TypeIdentifier::Storage{
            0x00U, 0xFEU, 0x02U,
            0x00U, 0x00U, 0x00U, 0x00U, 0x01U
        }
    };

    /// Example payload value.
    std::int32_t Value{0};

    using Fields = S::FieldSet<
        S::FieldBinding<&ScopeRequest::Value, 0U>
    >;
};

/// Test-local Command declaration used by InboundAdmission.
struct ScopeCommand final {
    static constexpr S::TypeIdentifier Identifier{
        S::TypeIdentifier::Storage{
            0x00U, 0xFEU, 0x02U,
            0x00U, 0x00U, 0x00U, 0x00U, 0x02U
        }
    };

    using Fields = S::FieldSet<>;
    using Family = C::Family;

    /// Request Type used by this test Command.
    using Request = ScopeRequest;

    /// Response Type used by this test Command.
    using Response = C::NoResponsePayload;
};

static_assert(C::CommandType<ScopeCommand>);

/// Minimal local Runtime substitute used only to observe inbound admission calls.
struct FakeRuntime final {
    // Admission observation state.

    /// Number of local admission attempts performed.
    std::size_t DispatchCount{0U};

    /// Most recently admitted payload value.
    std::int32_t LastValue{0};

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

struct FakeRemoteBinding final {
    std::array<std::uint32_t, 2U> Recipients{11U, 22U};
    std::array<bool, 2U> ResponseTaken{};
    std::size_t* ReleaseCount{nullptr};

    FakeRemoteBinding(std::size_t& releaseCount) noexcept :
        ReleaseCount(&releaseCount) {
    }

    FakeRemoteBinding(const FakeRemoteBinding&) = delete;
    FakeRemoteBinding& operator=(const FakeRemoteBinding&) = delete;
    FakeRemoteBinding(FakeRemoteBinding&&) noexcept = default;
    FakeRemoteBinding& operator=(FakeRemoteBinding&&) = delete;

    [[nodiscard]] std::size_t RecipientCount() const noexcept {
        return Recipients.size();
    }

    [[nodiscard]] const std::uint32_t& Recipient(std::size_t index) const noexcept {
        return Recipients[index];
    }

    [[nodiscard]] RemoteOutcome Observe(std::size_t index) const noexcept {
        return index < Recipients.size()
            ? RemoteOutcome::Accepted
            : RemoteOutcome::Unavailable;
    }

    [[nodiscard]] C::WaitResult WaitFor(std::size_t index, C::Duration) noexcept {
        return index < Recipients.size()
            ? C::WaitResult::Terminal
            : C::WaitResult::InvalidHandle;
    }

    [[nodiscard]] C::WaitResult WaitUntil(
        std::size_t index,
        C::MonotonicTimestamp
    ) noexcept {
        return index < Recipients.size()
            ? C::WaitResult::Terminal
            : C::WaitResult::InvalidHandle;
    }

    [[nodiscard]] C::CancellationRequestResult RequestCancellation(
        std::size_t index
    ) noexcept {
        return index < Recipients.size()
            ? C::CancellationRequestResult::Requested
            : C::CancellationRequestResult::InvalidHandle;
    }

    [[nodiscard]] C::TakeResponseStatus TakeResponse(std::size_t index) noexcept {
        if (index >= Recipients.size()) {
            return C::TakeResponseStatus::InvalidHandle;
        }
        if (ResponseTaken[index]) {
            return C::TakeResponseStatus::AlreadyTaken;
        }
        ResponseTaken[index] = true;
        return C::TakeResponseStatus::Taken;
    }

    void Release() noexcept {
        ++(*ReleaseCount);
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
static_assert(!std::is_copy_constructible_v<C::RemoteCommandOperation<ScopeCommand, FakeRemoteBinding>>);

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

    // Correlation is semantic Command identity, distinct from later delivery lifecycles.
    const S::Identity::DeviceIdentifier source{
        S::Identity::DeviceIdentifier::Storage{
            1U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
            0U, 0U, 0U, 0U, 0U, 0U, 0U, 1U
        }
    };
    const S::Identity::RuntimeIncarnationId incarnation{7U};
    const C::CommandInvocationCorrelation firstCorrelation{
        source,
        incarnation,
        DeliveryIdentifier{19U}
    };
    const C::CommandInvocationCorrelation sameCorrelation{
        source,
        incarnation,
        DeliveryIdentifier{19U}
    };
    const C::CommandInvocationCorrelation nextInvocation{
        source,
        incarnation,
        DeliveryIdentifier{20U}
    };
    assert(firstCorrelation == sameCorrelation);
    assert(!(firstCorrelation == nextInvocation));
    assert(firstCorrelation.Source() == source);
    assert(firstCorrelation.Runtime() == incarnation);
    assert(firstCorrelation.InvocationDelivery().Value == 19U);

    // The family-owned remote operation is move-only and delegates one bounded frozen set.
    std::size_t releaseCount = 0U;
    {
        C::RemoteCommandOperation<ScopeCommand, FakeRemoteBinding> operation(
            FakeRemoteBinding{releaseCount}
        );
        assert(operation.RecipientCount() == 2U);
        assert(operation.Recipient(0U) == 11U);
        assert(operation.Observe(1U) == RemoteOutcome::Accepted);
        assert(operation.WaitFor(0U, C::Duration{}) == C::WaitResult::Terminal);
        assert(
            operation.WaitUntil(1U, C::MonotonicTimestamp{}) ==
            C::WaitResult::Terminal
        );
        assert(
            operation.RequestCancellation(0U) ==
            C::CancellationRequestResult::Requested
        );
        assert(operation.TakeResponse(0U) == C::TakeResponseStatus::Taken);
        assert(operation.TakeResponse(0U) == C::TakeResponseStatus::AlreadyTaken);

        auto moved = std::move(operation);
        assert(!operation.IsValid());
        assert(moved.IsValid());
    }
    assert(releaseCount == 1U);
}
