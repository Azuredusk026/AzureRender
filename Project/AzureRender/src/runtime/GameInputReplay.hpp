#pragma once
#include "runtime/GameRuntime.hpp"
#include "runtime/LevelSession.hpp"
#include "runtime/GameplayKeys.hpp"
#include <nlohmann/json.hpp>
#include <vector>
namespace azurerender {
// Deterministic host input for Player route acceptance, using the normal actions.
class GameInputReplay {
public:
    static GameInputReplay parse(const nlohmann::json& document) {
        if(document.at("schemaVersion")!=1)throw std::invalid_argument("Unsupported game input replay");
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
            else if(kind=="preload-level" || kind=="load-level"){
                if(action.at("reference").get<std::string>().empty())throw std::invalid_argument("Empty replay level reference");
            }else if(kind!="cancel-level")throw std::invalid_argument("Unknown replay action");
            replay.actions_.push_back(action);
        }
        return replay;
    }
    void apply(std::uint64_t frame,GameRuntime& game,LevelSession* levels=nullptr) {
        while(cursor_<actions_.size()&&actions_[cursor_].at("frame").get<std::uint64_t>()<=frame) {
            const auto& action=actions_[cursor_++];const auto kind=action.at("action").get<std::string>();
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
    std::size_t consumed() const {return cursor_;}
private:
    std::vector<nlohmann::json> actions_;std::size_t cursor_=0;
};
}
