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
    void select(const std::string& state, double blendSeconds = 0);
    void advance(double delta, double playbackRate = 1);
    const std::string& state() const noexcept { return state_; }
    std::uint32_t clip() const { return states_.at(state_).clip; }
    double time() const noexcept { return time_; }
    bool loop() const { return states_.at(state_).loop; }
    std::uint32_t previousClip() const { return states_.at(previous_.empty() ? state_ : previous_).clip; }
    double previousTime() const { return previousTime_; }
    bool previousLoop() const { return states_.at(previous_.empty() ? state_ : previous_).loop; }
    float blend() const { return duration_ > 0 ? static_cast<float>(std::min(elapsed_ / duration_, 1.0)) : 1.0F; }
private:
    struct State { std::uint32_t clip; bool loop; };
    struct Transition { std::string from, to, parameter; bool value; double blendSeconds; };
    std::map<std::string, State> states_;
    std::vector<Transition> transitions_;
    std::map<std::string, bool> parameters_;
    std::string state_;
    double time_ = 0;
    std::string previous_;
    std::string pending_;
    double pendingDuration_ = 0;
    double previousTime_ = 0, duration_ = 0, elapsed_ = 0;
};
}
