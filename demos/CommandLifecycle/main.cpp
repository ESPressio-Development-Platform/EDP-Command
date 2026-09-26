#include <cstdio>
#include <ESPressio_Command.hpp>
namespace C = ESPressio::Command;

struct Request { int value; Request(int v) noexcept:value(v){} Request(Request&&) noexcept=default; Request(const Request&)=delete; ~Request() noexcept=default; };
struct Response { int value; Response(int v) noexcept:value(v){} Response(Response&&) noexcept=default; Response(const Response&)=delete; ~Response() noexcept=default; };
struct DoubleCommand { using Request = ::Request; using Response = ::Response; };
struct Executor { C::ExecutionResult<Response> Execute(const Request& r, C::CancellationToken) noexcept { return C::ExecutionResult<Response>::Succeeded(Response{r.value * 2}); } };

int main() {
    Executor executor;
    C::Runtime<DoubleCommand, Executor, C::ResourcePlan<4,3,1>> runtime(executor);
    runtime.Initialize();
    auto dispatch = runtime.Dispatch(Request{21});
    if (!dispatch.Accepted()) return 1;
    auto handle = dispatch.TakeHandle();
    runtime.ExecuteOne();
    auto response = handle.TakeResponse();
    if (!response.HasValue()) return 2;
    std::printf("response=%d\n", response.Take().value);
    handle.Release();
    runtime.BeginQuiesce();
    return 0;
}
