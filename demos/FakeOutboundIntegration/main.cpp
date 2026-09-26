#include <cassert>
#include <ESPressio_Command.hpp>
namespace C = ESPressio::Command;

struct Response { int value; Response(int v) noexcept:value(v){} Response(Response&&) noexcept=default; Response(const Response&)=delete; ~Response() noexcept=default; };
struct State { bool completed{false}; int value{0}; };

static bool Complete(void* context, C::CompletionStatus status, Response* response) noexcept {
    auto& state = *static_cast<State*>(context);
    if (state.completed || status == C::CompletionStatus::Failed) return false;
    state.completed = true; state.value = response->value; return true;
}
static bool Fail(void* context, C::ExecutionFailure) noexcept { auto& s=*static_cast<State*>(context); if(s.completed)return false; s.completed=true; return true; }
static bool Cancel(void* context) noexcept { auto& s=*static_cast<State*>(context); if(s.completed)return false; s.completed=true; return true; }

int main() {
    State state;
    C::OutboundCompletion<Response> completion(&state, &Complete, &Fail, &Cancel);
    assert(completion.Succeeded(Response{9}));
    assert(state.completed && state.value == 9);
    assert(!completion.Failed());
}
