#pragma once
#include "runtime/GameRuntime.hpp"
#include "runtime/LevelSession.hpp"
#include "runtime/GameplayKeys.hpp"
#include <nlohmann/json.hpp>
#include <vector>
#include <algorithm>
namespace azurerender {
// Deterministic host input for Player route acceptance, using the normal actions.
class GameInputReplay {
public:
    static GameInputReplay parse(const nlohmann::json& document) {
        if(!document.at("schemaVersion").is_number_integer()||document.at("schemaVersion")!=1||!document.at("actions").is_array()||document.at("actions").size()>8192)throw std::invalid_argument("Unsupported game input replay");
        GameInputReplay replay;std::uint64_t previous=0;
        for(const auto& action:document.at("actions")) {
            const auto frame=action.at("frame").get<std::int64_t>();
            if(frame<0||static_cast<std::uint64_t>(frame)<previous)throw std::invalid_argument("Replay frames must increase");
            previous=static_cast<std::uint64_t>(frame);
            const auto kind=action.at("action").get<std::string>();
            if(kind=="key") {
                const int key=action.at("key").get<int>();
                if(!isGameplayKey(key))throw std::invalid_argument("Unsupported replay key");
                action.at("down").get<bool>();
            }else if(kind=="focus")action.at("focused").get<bool>();
            else if(kind=="camera") {
                for(const char* field:{"x","y","scroll"})
                    if(!std::isfinite(action.value(field,0.0F)))throw std::invalid_argument("Invalid replay camera input");
            }
            else if(kind=="preload-level" || kind=="load-level" || kind=="wait-level"){
                if(action.at("reference").get<std::string>().empty())throw std::invalid_argument("Empty replay level reference");
                if(kind=="wait-level") {
                    const auto& timeout=action.at("timeoutFrames");
                    const auto revision=action.value("afterRevision",nlohmann::json(0));
                    if(!timeout.is_number_integer()||timeout.get<std::int64_t>()<1||timeout.get<std::int64_t>()>1000000)
                        throw std::invalid_argument("Replay level wait requires a bounded frame timeout");
                    if(!revision.is_number_integer()||revision.get<std::int64_t>()<0)
                        throw std::invalid_argument("Replay level revision must be a nonnegative integer");
                }
            }else if(kind!="cancel-level")throw std::invalid_argument("Unknown replay action");
            replay.actions_.push_back(action);
        }
        return replay;
    }
    void enqueue(nlohmann::json event,std::uint64_t frame) {
        if(frame>static_cast<std::uint64_t>(INT64_MAX))throw std::invalid_argument("Replay frame exceeds range");
        frame=frame>=delayFrames_?frame-delayFrames_:0;
        event["frame"]=frame;
        // Parse before changing the queue so rejection preserves pending input.
        auto checked=parse({{"schemaVersion",1},{"actions",nlohmann::json::array({event})}});
        if(cursor_>=1024||actions_.size()>=8192){actions_.erase(actions_.begin(),actions_.begin()+cursor_);cursor_=0;}
        if(actions_.size()>=8192)throw std::invalid_argument("Game input queue budget exceeded");
        const auto position=std::upper_bound(actions_.begin()+cursor_,actions_.end(),frame,
            [](auto value,const auto& action){return value<action.at("frame").template get<std::uint64_t>();});
        actions_.insert(position,std::move(checked.actions_.front()));
    }
    void apply(std::uint64_t frame,GameRuntime& game,LevelSession* levels=nullptr) {
        while(cursor_<actions_.size()&&actions_[cursor_].at("frame").get<std::uint64_t>()+delayFrames_<=frame) {
            const auto& action=actions_[cursor_];const auto kind=action.at("action").get<std::string>();
            if(kind=="wait-level") {
                if(!levels)throw std::logic_error("Level replay requires a level session");
                const auto due=action.at("frame").get<std::uint64_t>()+delayFrames_;
                if(levels->currentReference()!=action.at("reference").get<std::string>()||
                    levels->revision()<=action.value("afterRevision",std::uint64_t(0))) {
                    if(frame-due>=action.at("timeoutFrames").get<std::uint64_t>())
                        throw std::runtime_error("Replay timed out waiting for committed level: "+action.at("reference").get<std::string>());
                    return;
                }
                delayFrames_+=frame-due;
                ++cursor_;++consumed_;continue;
            }
            ++cursor_;++consumed_;
            if(kind=="key")game.input().key(action.at("key").get<int>(),action.at("down").get<bool>());
            else if(kind=="focus")game.input().setFocused(action.at("focused").get<bool>());
            else if(kind=="camera")game.cameraInput(action.value("x",0.0F),action.value("y",0.0F),action.value("scroll",0.0F));
            else {
                if(!levels)throw std::logic_error("Level replay requires a level session");
                if(kind=="preload-level")levels->preload(action.at("reference").get<std::string>());
                else if(kind=="load-level")levels->request(action.at("reference").get<std::string>());
                else levels->cancelPending();
            }
        }
    }
    std::size_t consumed() const {return consumed_;}
    std::uint64_t delayedFrames() const {return delayFrames_;}
private:
    std::vector<nlohmann::json> actions_;std::size_t cursor_=0,consumed_=0;
    std::uint64_t delayFrames_=0;
};
}
