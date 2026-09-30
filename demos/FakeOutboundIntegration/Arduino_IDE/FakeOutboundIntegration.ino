#include <cstdint>

#include <ESPressio_Command.hpp>

namespace Command = ESPressio::Command;
namespace System = ESPressio::System;

struct Response final {
    static constexpr System::TypeIdentifier Identifier{
        System::TypeIdentifier::Storage{
            0x00U, 0xFEU, 0x11U,
            0x00U, 0x00U, 0x00U, 0x00U, 0x01U
        }
    };

    std::int32_t Value;

    using Fields = System::FieldSet<
        System::FieldBinding<&Response::Value, 0U>
    >;

    explicit Response(std::int32_t value) noexcept : Value(value) {}
    Response(Response&&) noexcept = default;
    Response(const Response&) = delete;
    ~Response() noexcept = default;
};

static_assert(System::SchemaType<Response>);

struct State final {
    bool Completed{false};
    std::int32_t Value{0};
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
        "first=%u second=%u value=%ld\n",
        static_cast<unsigned>(first),
        static_cast<unsigned>(second),
        static_cast<long>(state.Value)
    );
}

void loop() {
}
