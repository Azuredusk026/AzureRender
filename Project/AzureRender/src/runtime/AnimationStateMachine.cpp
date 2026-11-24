#include "runtime/AnimationStateMachine.hpp"
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
namespace azurerender {
AnimationStateMachine AnimationStateMachine::parse(const nlohmann::json& document) {
    const auto version=document.at("schemaVersion");
    if (version != 1 && version != 2 && version != 3 && version != 4)
        throw std::invalid_argument("Unsupported animation graph version");
    const bool hasTimeline = version == 3 || version == 4;
    AnimationStateMachine machine;
    if (version == 2 || (hasTimeline && document.contains("locomotion")))
        machine.locomotion_ = locomotionBlendProfileFromJson(document.at("locomotion"));
    else if (document.contains("locomotion")) throw std::invalid_argument("Locomotion requires animation graph version two or later");
    if (!hasTimeline && document.contains("timeline")) throw std::invalid_argument("Timeline requires animation graph version three or later");
    if (version != 4 && (document.contains("signals") || document.contains("branches")))
        throw std::invalid_argument("Signals require animation graph version four");
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
    const auto checkedEdge = [&](const nlohmann::json& transition, const std::string& from, double after) {
        TimedTransition next{transition.at("to").get<std::string>(), after,
            transition.value("blendSeconds", 0.0), transition.value("startSeconds", 0.0)};
        if (!machine.states_.count(from) || !machine.states_.count(next.to) ||
            !std::isfinite(next.afterSeconds) || next.afterSeconds < 0 ||
            !std::isfinite(next.startSeconds) || next.startSeconds < 0 ||
            !std::isfinite(next.blendSeconds) || next.blendSeconds < 0 || next.blendSeconds > 10)
            throw std::invalid_argument("Invalid timed animation transition");
        return next;
    };
    if (hasTimeline) {
        const auto& timeline = document.at("timeline");
        if (!timeline.is_array()) throw std::invalid_argument("Animation timeline must be an array");
        for (const auto& transition : timeline) {
            const auto from = transition.at("from").get<std::string>();
            auto next = checkedEdge(transition, from, transition.at("afterSeconds").get<double>());
            if (!machine.timeline_.emplace(from, std::move(next)).second)
                throw std::invalid_argument("Duplicate timed animation transition");
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
    if (version == 4) {
        const auto& signals = document.at("signals");
        const auto& branches = document.at("branches");
        if (!signals.is_array() || signals.empty() || !branches.is_array())
            throw std::invalid_argument("Expected declared animation signals and branches");
        for (const auto& value : signals) {
            const auto name = value.get<std::string>();
            if (name.empty() || machine.states_.count(name) || !machine.signals_.insert(name).second)
                throw std::invalid_argument("Invalid or duplicate animation signal");
        }
        for (const auto& row : branches) {
            const auto from = row.at("from").get<std::string>();
            const auto next = checkedEdge(row, from, row.value("afterSeconds", 0.0));
            const auto& enabled = row.at("signals");
            if (next.to == from || !enabled.is_array() || enabled.empty())
                throw std::invalid_argument("Expected a distinct branch target and an animation signal");
            auto& choices = machine.branches_[from];
            const bool once = row.value("once", false);
            for (const auto& value : enabled) {
                const auto name = value.get<std::string>();
                if (!machine.signals_.count(name) || !choices.emplace(name, Branch{next, once}).second)
                    throw std::invalid_argument("Undeclared or overlapping animation branch signal");
            }
        }
    }
    return machine;
}
void AnimationStateMachine::select(const std::string& state, double blendSeconds) {
    if (!std::isfinite(blendSeconds) || blendSeconds < 0 || blendSeconds > 10) throw std::invalid_argument("Invalid crossfade duration");
    if (signals_.count(state)) {
        signal_ = state;
        freshSignal_ = true;
        return;
    }
    if (!states_.count(state)) throw std::invalid_argument("Unknown animation state: " + state);
    signal_.clear();
    freshSignal_ = false;
    selectState(state, blendSeconds);
}
void AnimationStateMachine::selectState(const std::string& state, double blendSeconds) {
    if (state_ == state) { pending_.clear(); return; }
    if (blend()<1 && !previous_.empty()) { pending_=state;pendingDuration_=blendSeconds;return; }
    if (state_ != state) { previous_ = state_; previousTime_ = time_; duration_ = blendSeconds; elapsed_ = 0; state_ = state; time_ = 0; }
}
void AnimationStateMachine::advance(double delta, double playbackRate) {
    if (!std::isfinite(delta) || delta < 0) throw std::invalid_argument("Invalid animation time delta");
    if (!std::isfinite(playbackRate)||playbackRate<0)throw std::invalid_argument("Invalid animation playback rate");
    for (const auto& transition : transitions_)
        if (transition.from == state_ && parameters_[transition.parameter] == transition.value) {
            selectState(transition.to, transition.blendSeconds);
            break;
        }
    const auto tick = [&](double step) {
        time_ += step * playbackRate;
        previousTime_ += step * playbackRate;
        elapsed_ += step;
    };
    // Traverse the authored boundaries within this frame. Blend durations use wall time;
    // thresholds and start offsets use the source clip clock.
    std::size_t traversed = 0;
    while (pending_.empty()) {
        const TimedTransition* transition = nullptr;
        double wait = std::numeric_limits<double>::infinity();
        bool consume = false;
        const auto consider = [&](const TimedTransition& next, bool once) {
            const double clipWait = std::max(0.0, next.afterSeconds - time_);
            const double exitWait = clipWait == 0 ? 0 : (playbackRate == 0
                ? std::numeric_limits<double>::infinity() : clipWait / playbackRate);
            const double ready = std::max(exitWait, duration_ - elapsed_);
            if (ready <= wait) {
                transition = &next;
                wait = ready;
                consume = once;
            }
        };
        if (const auto edge = timeline_.find(state_); edge != timeline_.end())
            consider(edge->second, false);
        if (!signal_.empty()) {
            const auto branches = branches_.find(state_);
            if (branches != branches_.end()) {
                const auto edge = branches->second.find(signal_);
                if (edge != branches->second.end() && (!edge->second.once || freshSignal_))
                    consider(edge->second.transition, edge->second.once);
            }
        }
        if (!transition || wait > delta) break;
        if (++traversed > 2 * states_.size())
            throw std::runtime_error("Animation branch limit exceeded in one frame");
        tick(wait);
        elapsed_ = std::max(elapsed_, duration_);
        if (consume) freshSignal_ = false;
        selectState(transition->to, transition->blendSeconds);
        time_ = transition->startSeconds;
        delta -= wait;
    }
    tick(delta);
    if(blend()==1&&!pending_.empty()){const auto state=pending_;pending_.clear();selectState(state,pendingDuration_);}
}
}
