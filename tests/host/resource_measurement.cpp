#include <cstddef>
#include <cstdio>
#include <ESPressio_Command.hpp>

namespace C = ESPressio::Command;

struct Request final { std::uint32_t Value{0U}; };
struct Response final { std::uint32_t Value{0U}; };
struct Command final { using Request = ::Request; using Response = ::Response; };
struct Executor final { C::ExecutionResult<Response> Execute(const Request& request, C::CancellationToken) noexcept { return C::ExecutionResult<Response>::Succeeded(Response{request.Value}); } };

template<std::size_t Invocations, std::size_t Queue, std::size_t Concurrency>
void Measure(const char* name) {
    using Plan = C::ResourcePlan<Invocations, Queue, Concurrency>;
    using Runtime = C::Runtime<Command, Executor, Plan>;
    std::printf("EDP_COMMAND_RESOURCE plan=%s invocations=%zu queue=%zu concurrency=%zu runtime=%zu request=%zu response=%zu\n", name, Invocations, Queue, Concurrency, sizeof(Runtime), sizeof(Request), sizeof(Response));
}

int main() {
    Measure<1U, 1U, 1U>("minimum");
    Measure<4U, 3U, 1U>("small");
    Measure<8U, 8U, 2U>("representative");
    Measure<16U, 16U, 4U>("high");
    return 0;
}
