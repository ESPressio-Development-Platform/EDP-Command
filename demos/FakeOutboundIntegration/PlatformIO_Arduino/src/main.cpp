#include <Arduino.h>
#include <ESPressio_Command.hpp>

namespace Command = ESPressio::Command;

struct Response final {
    int Value;
    explicit Response(int value) noexcept : Value(value) {}
    Response(Response&&) noexcept = default;
    Response(const Response&) = delete;
    ~Response() noexcept = default;
};

struct State final {
    bool Completed{false};
    int Value{0};
};

Command::CompletionPublicationResult Complete(
    void* context,
    Command::Outcome outcome,
    Response* response
) noexcept {
    auto& state = *static_cast<State*>(context);
    if (state.Completed) return Command::CompletionPublicationResult::AlreadyCompleted;
    if (outcome == Command::Outcome::Failed || outcome == Command::Outcome::Cancelled) {
        return Command::CompletionPublicationResult::Unavailable;
    }
    state.Completed = true;
    state.Value = response->Value;
    return Command::CompletionPublicationResult::Accepted;
}

Command::CompletionPublicationResult Fail(void* context, Command::ExecutionFailure) noexcept {
    auto& state = *static_cast<State*>(context);
    if (state.Completed) return Command::CompletionPublicationResult::AlreadyCompleted;
    state.Completed = true;
    return Command::CompletionPublicationResult::Accepted;
}

Command::CompletionPublicationResult Cancel(void* context) noexcept {
    auto& state = *static_cast<State*>(context);
    if (state.Completed) return Command::CompletionPublicationResult::AlreadyCompleted;
    state.Completed = true;
    return Command::CompletionPublicationResult::Accepted;
}

void setup() {
    Serial.begin(115200);
    State state;
    Command::OutboundCompletion<Response> completion(&state, &Complete, &Fail, &Cancel);
    const auto first = completion.Succeeded(Response{9});
    const auto second = completion.Failed();
    Serial.printf(
        "first=%u second=%u value=%d\n",
        static_cast<unsigned>(first),
        static_cast<unsigned>(second),
        state.Value
    );
}

void loop() {
}
