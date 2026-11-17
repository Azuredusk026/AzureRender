#include "runtime/AnimationStateMachine.hpp"
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
namespace azurerender {
AnimationStateMachine AnimationStateMachine::parse(const nlohmann::json& document) {
    const auto version=document.at("schemaVersion");
    if (version != 1 && version != 2 && version != 3) throw std::invalid_argument("Unsupported animation graph version");
    AnimationStateMachine machine;
    if (version == 2 || (version == 3 && document.contains("locomotion")))
        machine.locomotion_ = locomotionBlendProfileFromJson(document.at("locomotion"));
    else if (document.contains("locomotion")) throw std::invalid_argument("Locomotion requires animation graph version two or three");
    if (version != 3 && document.contains("timeline")) throw std::invalid_argument("Timeline requires animation graph version three");
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
            transition.at("parameter").get<std::string>(), transition.at("value").get<bool>(), transition.value("blendSeconds", 0.0)};
        if (!machine.states_.count(value.from) || !machine.states_.count(value.to) || value.parameter.empty()
            || !std::isfinite(value.blendSeconds) || value.blendSeconds < 0 || value.blendSeconds > 10)
            throw std::invalid_argument("Invalid animation transition");
        machine.transitions_.push_back(std::move(value));
    }
    if (version == 3) {
        const auto& timeline = document.at("timeline");
        if (!timeline.is_array()) throw std::invalid_argument("Animation timeline must be an array");
        for (const auto& transition : timeline) {
            const auto from = transition.at("from").get<std::string>();
            TimedTransition next{transition.at("to").get<std::string>(), transition.at("afterSeconds").get<double>(),
                transition.value("blendSeconds", 0.0), transition.value("startSeconds", 0.0)};
            if (!machine.states_.count(from) || !machine.states_.count(next.to) ||
                !std::isfinite(next.afterSeconds) || next.afterSeconds < 0 ||
                !std::isfinite(next.startSeconds) || next.startSeconds < 0 ||
                !std::isfinite(next.blendSeconds) || next.blendSeconds < 0 || next.blendSeconds > 10 ||
                !machine.timeline_.emplace(from, std::move(next)).second)
                throw std::invalid_argument("Invalid or duplicate timed animation transition");
        }
        std::set<std::string> visited;
        for (const auto& edge : machine.timeline_) {
            std::set<std::string> chain;
            auto next = machine.timeline_.find(edge.first);
            while (next != machine.timeline_.end() && visited.count(next->first) == 0) {
                if (!chain.insert(next->first).second) throw std::invalid_argument("Animation timeline contains a cycle");
                next = machine.timeline_.find(next->second.to);
            }
            visited.merge(chain);
        }
    }
    return machine;
}
void AnimationStateMachine::select(const std::string& state, double blendSeconds) {
    if (!states_.count(state)) throw std::invalid_argument("Unknown animation state: " + state);
    if (!std::isfinite(blendSeconds) || blendSeconds < 0 || blendSeconds > 10) throw std::invalid_argument("Invalid crossfade duration");
    if (state_ == state) { pending_.clear(); return; }
    if (blend()<1 && !previous_.empty()) { pending_=state;pendingDuration_=blendSeconds;return; }
    if (state_ != state) { previous_ = state_; previousTime_ = time_; duration_ = blendSeconds; elapsed_ = 0; state_ = state; time_ = 0; }
}
void AnimationStateMachine::advance(double delta, double playbackRate) {
    if (!std::isfinite(delta) || delta < 0) throw std::invalid_argument("Invalid animation time delta");
    if (!std::isfinite(playbackRate)||playbackRate<0)throw std::invalid_argument("Invalid animation playback rate");
    for (const auto& transition : transitions_)
        if (transition.from == state_ && parameters_[transition.parameter] == transition.value) { select(transition.to, transition.blendSeconds); break; }
    const auto tick = [&](double step) {
        time_ += step * playbackRate;
        previousTime_ += step * playbackRate;
        elapsed_ += step;
    };
    // Traverse the authored boundaries within this frame. Blend durations use wall time;
    // thresholds and start offsets use the source clip clock.
    while (pending_.empty()) {
        const auto edge = timeline_.find(state_);
        if (edge == timeline_.end()) break;
        const auto& transition = edge->second;
        const double clipWait = std::max(0.0, transition.afterSeconds - time_);
        const double exitWait = clipWait == 0 ? 0 : (playbackRate == 0
            ? std::numeric_limits<double>::infinity() : clipWait / playbackRate);
        const double wait = std::max(exitWait, duration_ - elapsed_);
        if (wait > delta) break;
        tick(wait);
        elapsed_ = std::max(elapsed_, duration_);
        select(transition.to, transition.blendSeconds);
        time_ = transition.startSeconds;
        delta -= wait;
    }
    tick(delta);
    if(blend()==1&&!pending_.empty()){const auto state=pending_;pending_.clear();select(state,pendingDuration_);}
}
}
