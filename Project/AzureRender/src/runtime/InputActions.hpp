#pragma once
#include <map>
#include <set>
#include <string>
namespace azurerender {
class InputActions {
public:
    InputActions() {
        bind("move-left", 65); bind("move-right", 68); bind("move-forward", 87);
        bind("move-back", 83); bind("jump", 32); bind("interact", 69);
    }
    void bind(std::string action, int key) { bindings_[std::move(action)] = key; }
    void key(int key, bool down) {
        if (!focused_) return;
        if (down) { if (down_.insert(key).second) pressed_.insert(key); }
        else down_.erase(key);
    }
    bool down(const std::string& action) const { return active(action, down_); }
    bool pressed(const std::string& action) const { return active(action, pressed_); }
    void endStep() { pressed_.clear(); }
    void setFocused(bool value) { focused_ = value; if (!value) { down_.clear(); pressed_.clear(); } }
    bool focused() const noexcept { return focused_; }
private:
    bool active(const std::string& action, const std::set<int>& keys) const {
        const auto found = bindings_.find(action); return focused_ && found != bindings_.end() && keys.count(found->second) != 0;
    }
    bool focused_ = true;
    std::map<std::string, int> bindings_;
    std::set<int> down_, pressed_;
};
}
