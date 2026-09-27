#include <cstdio>
#include <ESPressio_Command.hpp>

namespace C = ESPressio::Command;
namespace CF = ESPressio::System::CompositionFramework;

struct Request {
    int value;
    explicit Request(int v) noexcept : value(v) {}
    Request(Request&&) noexcept = default;
    Request(const Request&) = delete;
    ~Request() noexcept = default;
};

struct Response {
    int value;
    explicit Response(int v) noexcept : value(v) {}
    Response(Response&&) noexcept = default;
    Response(const Response&) = delete;
    ~Response() noexcept = default;
};

struct DoubleCommand {
    using Request = ::Request;
    using Response = ::Response;
};

struct DoubleHandler : CF::Provider<
    C::Composition::Domain,
    CF::Offers<CF::Offer<C::Composition::Handler<DoubleCommand>>>
> {
    C::ExecutionResult<Response> Execute(const Request& request, C::CancellationToken) noexcept {
        return C::ExecutionResult<Response>::Succeeded(Response{request.value * 2});
    }
};

using CommandComposition = CF::Composition<C::Composition::Domain, DoubleHandler>;
using CommandBootstrap = C::Bootstrap<DoubleCommand, CommandComposition, C::ResourcePlan<4, 3, 1>>;

int main() {
    DoubleHandler handler;
    CommandBootstrap bootstrap(handler);
    if (!bootstrap.Initialize()) return 1;

    auto& runtime = bootstrap.RuntimeInstance();
    auto dispatch = runtime.Dispatch(Request{21});
    if (!dispatch.Accepted()) return 2;

    auto handle = dispatch.TakeHandle();
    if (!runtime.ExecuteOne()) return 3;

    auto response = handle.TakeResponse();
    if (!response.HasValue()) return 4;

    std::printf("response=%d\n", response.Take().value);
    handle.Release();
    runtime.BeginQuiesce();
    return 0;
}
