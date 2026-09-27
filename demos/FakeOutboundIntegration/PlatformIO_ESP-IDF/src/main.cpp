#include <cstdio>
#include <ESPressio_Command.hpp>
namespace C = ESPressio::Command;
struct Response final { int Value; explicit Response(int value) noexcept : Value(value) {} Response(Response&&) noexcept = default; Response(const Response&) = delete; ~Response() noexcept = default; };
struct State final { bool Completed{false}; int Value{0}; };
bool Complete(void* context, C::CompletionStatus status, Response* response) noexcept { auto& state=*static_cast<State*>(context); if(state.Completed || status==C::CompletionStatus::Failed)return false; state.Completed=true; state.Value=response->Value; return true; }
bool Fail(void* context, C::ExecutionFailure) noexcept { auto& state=*static_cast<State*>(context); if(state.Completed)return false; state.Completed=true; return true; }
bool Cancel(void* context) noexcept { auto& state=*static_cast<State*>(context); if(state.Completed)return false; state.Completed=true; return true; }
extern "C" void app_main() { State state; C::OutboundCompletion<Response> completion(&state,&Complete,&Fail,&Cancel); const bool first=completion.Succeeded(Response{9}); const bool second=completion.Failed(); std::printf("first=%d second=%d value=%d\n",first,second,state.Value); }
