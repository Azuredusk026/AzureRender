#include "runtime/AnimationStateMachine.hpp"
#include <cmath>
#include <stdexcept>
namespace azurerender {
AnimationStateMachine AnimationStateMachine::parse(const nlohmann::json& document) {
    if (document.at("schemaVersion") != 1) throw std::invalid_argument("Unsupported animation graph version");
    AnimationStateMachine machine;
    for (const auto& state : document.at("states")) {
        const auto name = state.at("name").get<std::string>();
        const auto clip = state.at("clip");
        if (name.empty() || !clip.is_number_integer() || clip.get<std::int64_t>() < 0 || clip.get<std::uint64_t>() > UINT32_MAX
            || !machine.states_.emplace(name, State{clip.get<std::uint32_t>(), state.value("loop", true)}).second)
            throw std::invalid_argument("Invalid or duplicate animation state");
    }
    machine.select(document.at("initial").get<std::string>());
    for (const auto& transition : document.at("transitions")) {
        Transition value{transition.at("from").get<std::string>(), transition.at("to").get<std::string>(),
            transition.at("parameter").get<std::string>(), transition.at("value").get<bool>()};
        if (!machine.states_.count(value.from) || !machine.states_.count(value.to) || value.parameter.empty())
            throw std::invalid_argument("Invalid animation transition");
        machine.transitions_.push_back(std::move(value));
    }
    return machine;
}
void AnimationStateMachine::select(const std::string& state) {
    if (!states_.count(state)) throw std::invalid_argument("Unknown animation state: " + state);
    if (state_ != state) { state_ = state; time_ = 0; }
}
void AnimationStateMachine::advance(double delta) {
    if (!std::isfinite(delta) || delta < 0) throw std::invalid_argument("Invalid animation time delta");
    for (const auto& transition : transitions_)
        if (transition.from == state_ && parameters_[transition.parameter] == transition.value) { select(transition.to); break; }
    time_ += delta;
}
}
