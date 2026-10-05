#pragma once
#include <map>
#include <set>
#include <string>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include "runtime/GameplayKeys.hpp"
namespace azurerender {
class InputActions {
public:
    void configure(const nlohmann::json& config) {
        if(!config.is_object())throw std::invalid_argument("Input bindings must be an object");
        std::map<std::string,std::set<int>> candidate;
        for(const auto& item:config.items()) {
            if(item.key().empty() || !item.value().is_array() || item.value().empty())
                throw std::invalid_argument("Invalid action binding: " + item.key());
            for(const auto& key:item.value()) {
                if(!key.is_number_integer() || key.get<std::int64_t>()<32 || key.get<std::int64_t>()>348 || !isGameplayKey(key.get<int>()))
                    throw std::invalid_argument("Invalid physical key: " + item.key());
                candidate[item.key()].insert(key.get<int>());
            }
        }
        bindings_=std::move(candidate); release();
    }
    std::vector<int> keys() const { std::set<int> result;for(const auto& item:bindings_)result.insert(item.second.begin(),item.second.end());return {result.begin(),result.end()}; }
    bool bound(int key) const { for(const auto& item:bindings_)if(item.second.count(key))return true;return false; }
    bool hasAction(const std::string& action) const { return bindings_.count(action)!=0; }
    void bind(std::string action, int key) { bindings_[std::move(action)] = {key}; }
    void bindAdditional(const std::string& action,int key) { bindings_[action].insert(key); }
    void key(int key, bool down) {
        if (!focused_) return;
        if (down) { if (down_.insert(key).second) pressed_.insert(key); }
        else down_.erase(key);
    }
    bool down(const std::string& action) const { return active(action, down_); }
    bool pressed(const std::string& action) const { return active(action, pressed_); }
    void endStep() { pressed_.clear(); }
    void release() { down_.clear(); pressed_.clear(); }
    void setFocused(bool value) { focused_ = value; if (!value) release(); }
    bool focused() const noexcept { return focused_; }
private:
    bool active(const std::string& action, const std::set<int>& keys) const {
        const auto found = bindings_.find(action);
        if(!focused_||found==bindings_.end())return false;
        for(const auto key:found->second)if(keys.count(key))return true;
        return false;
    }
    bool focused_ = true;
    std::map<std::string, std::set<int>> bindings_;
    std::set<int> down_, pressed_;
};
}
