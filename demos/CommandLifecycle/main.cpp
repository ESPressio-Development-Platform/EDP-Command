#include <cstdio>
#include <ESPressio_Command.hpp>
#include <ESPressio_Platform_FreeRTOS.hpp>

namespace C = ESPressio::Command;
namespace T = ESPressio::Threading;
namespace CF = ESPressio::System::CompositionFramework;

struct Request { int value; };
struct Response { int value; };
struct DoubleCommand { using Request = ::Request; using Response = ::Response; };
struct DoubleHandler : CF::Provider<C::Composition::Domain, CF::Offers<CF::Offer<C::Composition::Handler<DoubleCommand>>>> {
    C::ExecutionResult<Response> Execute(const Request& request, C::CancellationToken) noexcept {
        return C::ExecutionResult<Response>::Succeeded(Response{request.value * 2});
    }
};

using WaitProvider = T::BoundedWaitWakeProvider<ESPressio::Platform::FreeRTOS::Synchronization::SignalProvider, 4U>;
using CommandComposition = CF::Composition<C::Composition::Domain, DoubleHandler>;
using ThreadingComposition = CF::Composition<T::Domain, WaitProvider>;
using ApplicationArchitecture = CF::Architecture<CommandComposition, ThreadingComposition>;
using CommandBootstrap = C::Bootstrap<DoubleCommand, ApplicationArchitecture, C::ResourcePlan<4, 3, 1>>;

int main() {
    DoubleHandler handler; WaitProvider waits; CommandBootstrap bootstrap(handler, waits);
    if (!bootstrap.Initialize()) return 1;
    auto& runtime = bootstrap.RuntimeInstance();
    auto dispatch = runtime.Dispatch(Request{21}); if (!dispatch.Accepted()) return 2;
    auto handle = dispatch.TakeHandle(); if (!runtime.ExecuteOne()) return 3;
    if (handle.WaitFor(C::Duration{}) != C::WaitResult::Terminal) return 4;
    auto response = handle.TakeResponse(); if (!response.HasValue()) return 5;
    std::printf("response=%d\n", response.Take().value);
    handle.Release(); runtime.BeginQuiesce(); return 0;
}
