#include <cassert>
#include <type_traits>
#include <ESPressio_Command.hpp>

namespace C = ESPressio::Command;

struct EchoRequest { int Value; explicit EchoRequest(int value) noexcept : Value(value) {} EchoRequest(EchoRequest&&) noexcept = default; EchoRequest(const EchoRequest&) = delete; ~EchoRequest() noexcept = default; };
struct EchoResponse { int Value; explicit EchoResponse(int value) noexcept : Value(value) {} EchoResponse(EchoResponse&&) noexcept = default; EchoResponse(const EchoResponse&) = delete; ~EchoResponse() noexcept = default; };
struct Echo { using Request = EchoRequest; using Response = EchoResponse; };

struct Executor {
    C::ExecutionResult<EchoResponse> Execute(const EchoRequest& request, C::CancellationToken cancellation) noexcept {
        if (cancellation.IsRequested()) return C::ExecutionResult<EchoResponse>::Failed();
        return C::ExecutionResult<EchoResponse>::Succeeded(EchoResponse{request.Value});
    }
};

using Plan = C::ResourcePlan<4, 3, 1>;
using Runtime = C::Runtime<Echo, Executor, Plan>;
static_assert(Plan::QueueCapacity + Plan::ExecutionConcurrency <= Plan::InvocationCapacity);
static_assert(!std::is_copy_constructible_v<C::Handle<Echo, Runtime>>);

int main() {
    Executor executor;
    Runtime runtime(executor);
    assert(runtime.Initialize());
    auto dispatch = runtime.Dispatch(EchoRequest{42});
    assert(dispatch.Accepted());
    auto handle = dispatch.TakeHandle();
    bool valid = false;
    assert(handle.State(valid).State == C::InvocationState::Queued && valid);
    assert(runtime.ExecuteOne());
    auto observation = handle.State(valid);
    assert(valid && observation.State == C::InvocationState::Completed);
    assert(observation.Completion == C::CompletionStatus::Succeeded);
    auto response = handle.TakeResponse();
    assert(response.Status() == C::TakeResponseStatus::Taken);
    assert(response.Take().Value == 42);
    auto cancelledDispatch = runtime.Dispatch(EchoRequest{7});
    auto cancelled = cancelledDispatch.TakeHandle();
    assert(cancelled.RequestCancellation() == C::CancellationRequestResult::Accepted);
    assert(cancelled.State(valid).State == C::InvocationState::Cancelled);
    assert(runtime.BeginQuiesce());
    auto rejected = runtime.Dispatch(EchoRequest{1});
    assert(!rejected.Accepted());
    assert(rejected.Failure() == C::DispatchFailure::RuntimeUnavailable);
}
