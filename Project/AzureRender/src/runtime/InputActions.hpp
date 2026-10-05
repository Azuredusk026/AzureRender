#pragma once
#include <map>
#include <set>
#include <string>
namespace azurerender {
class InputActions {
public:
    InputActions() {
        bind("move-left", 65); bind("move-right", 68); bind("move-forward", 87);
        bind("move-back", 83); bind("jump", 32); bind("interact", 69); bind("restart", 82);
        bind("sprint",340); bindAdditional("sprint",344);
    }
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
