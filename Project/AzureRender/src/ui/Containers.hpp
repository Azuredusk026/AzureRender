#pragma once
#include <imgui.h>
namespace azurerender::ui {
class WindowScope final {
public:
    WindowScope(const char* title,bool* open=nullptr,ImGuiWindowFlags flags=0);
    ~WindowScope();
    explicit operator bool() const { return visible_; }
    WindowScope(const WindowScope&)=delete;
    WindowScope& operator=(const WindowScope&)=delete;
private: bool visible_;
};
class ChildScope final {
public:
    ChildScope(const char* id,ImVec2 size={},ImGuiChildFlags flags=0,ImGuiWindowFlags windowFlags=0);
    ~ChildScope();
    explicit operator bool() const { return visible_; }
    ChildScope(const ChildScope&)=delete;
    ChildScope& operator=(const ChildScope&)=delete;
private: bool visible_;
};
}
