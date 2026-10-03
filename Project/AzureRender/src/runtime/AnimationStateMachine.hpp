#pragma once
#include <nlohmann/json.hpp>
#include <map>
#include <string>
#include <vector>
namespace azurerender {
class AnimationStateMachine {
public:
    static AnimationStateMachine parse(const nlohmann::json& document);
    void set(const std::string& parameter, bool value) { parameters_[parameter] = value; }
    void select(const std::string& state);
    void advance(double delta);
    const std::string& state() const noexcept { return state_; }
    std::uint32_t clip() const { return states_.at(state_).clip; }
    double time() const noexcept { return time_; }
    bool loop() const { return states_.at(state_).loop; }
private:
    struct State { std::uint32_t clip; bool loop; };
    struct Transition { std::string from, to, parameter; bool value; };
    std::map<std::string, State> states_;
    std::vector<Transition> transitions_;
    std::map<std::string, bool> parameters_;
    std::string state_;
    double time_ = 0;
};
}
