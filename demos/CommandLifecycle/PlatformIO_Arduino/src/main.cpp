#include <Arduino.h>
#include <ESPressio_Command.hpp>

namespace C = ESPressio::Command;

struct Request final { int Value; explicit Request(int value) noexcept : Value(value) {} Request(Request&&) noexcept = default; Request(const Request&) = delete; ~Request() noexcept = default; };
struct Response final { int Value; explicit Response(int value) noexcept : Value(value) {} Response(Response&&) noexcept = default; Response(const Response&) = delete; ~Response() noexcept = default; };
struct DoubleCommand final { using Request = ::Request; using Response = ::Response; };
struct Executor final { C::ExecutionResult<Response> Execute(const Request& request, C::CancellationToken) noexcept { return C::ExecutionResult<Response>::Succeeded(Response{request.Value * 2}); } };

Executor ExecutorInstance;
C::Runtime<DoubleCommand, Executor, C::ResourcePlan<4U, 3U, 1U>> Runtime(ExecutorInstance);

void setup() {
    Serial.begin(115200);
    if (!Runtime.Initialize()) return;
    auto dispatch = Runtime.Dispatch(Request{21});
    if (!dispatch.Accepted()) return;
    auto handle = dispatch.TakeHandle();
    if (!Runtime.ExecuteOne()) return;
    auto response = handle.TakeResponse();
    if (!response.HasValue()) return;
    Serial.printf("response=%d\n", response.Take().Value);
    handle.Release();
    static_cast<void>(Runtime.BeginQuiesce());
}

void loop() {}
