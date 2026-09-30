#include <cassert>
#include <cstdint>

#include <ESPressio_Command.hpp>

namespace C = ESPressio::Command;
namespace S = ESPressio::System;

struct Response final {
    static constexpr S::TypeIdentifier Identifier{
        S::TypeIdentifier::Storage{
            0x00U, 0xFEU, 0x11U,
            0x00U, 0x00U, 0x00U, 0x00U, 0x01U
        }
    };

    std::int32_t Value;

    using Fields = S::FieldSet<
        S::FieldBinding<&Response::Value, 0U>
    >;

    explicit Response(std::int32_t value) noexcept : Value(value) {}
    Response(Response&&) noexcept = default;
    Response(const Response&) = delete;
    ~Response() noexcept = default;
};

static_assert(S::SchemaType<Response>);

struct State final {
    bool Completed{false};
    std::int32_t Value{0};
};

static C::CompletionPublicationResult Complete(void* context, C::Outcome outcome, Response* response) noexcept {
    auto& state = *static_cast<State*>(context);
    if (state.Completed) return C::CompletionPublicationResult::AlreadyCompleted;
    if (outcome == C::Outcome::Failed || outcome == C::Outcome::Cancelled) return C::CompletionPublicationResult::Unavailable;
    state.Completed = true;
    state.Value = response->Value;
    return C::CompletionPublicationResult::Accepted;
}

static C::CompletionPublicationResult Fail(void* context, C::ExecutionFailure) noexcept {
    auto& state = *static_cast<State*>(context);
    if (state.Completed) return C::CompletionPublicationResult::AlreadyCompleted;
    state.Completed = true;
    return C::CompletionPublicationResult::Accepted;
}

static C::CompletionPublicationResult Cancel(void* context) noexcept {
    auto& state = *static_cast<State*>(context);
    if (state.Completed) return C::CompletionPublicationResult::AlreadyCompleted;
    state.Completed = true;
    return C::CompletionPublicationResult::Accepted;
}

int main() {
    State state;
    C::OutboundCompletion<Response> completion(&state, &Complete, &Fail, &Cancel);
    assert(completion.Succeeded(Response{9}) == C::CompletionPublicationResult::Accepted);
    assert(state.Completed && state.Value == 9);
    assert(completion.Failed() == C::CompletionPublicationResult::AlreadyCompleted);
}
