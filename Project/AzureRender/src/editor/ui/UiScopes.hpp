// Adapted scope pattern from gkNextEngine, 4ba5b7cd106c282e7ed166ff853aeea87b392680.
// Copyright (c) 2024-2026 gameknife. MIT; see third_party/gknextengine/LICENSE.txt.
#pragma once
#include <imgui.h>
namespace azurerender::ui {
class ScopedId {
public:
    explicit ScopedId(const char* id){ImGui::PushID(id);}
    explicit ScopedId(int id){ImGui::PushID(id);}
    ~ScopedId(){ImGui::PopID();}
    ScopedId(const ScopedId&)=delete;ScopedId& operator=(const ScopedId&)=delete;
};
class ScopedDisabled {
public:
    explicit ScopedDisabled(bool disabled){ImGui::BeginDisabled(disabled);}
    ~ScopedDisabled(){ImGui::EndDisabled();}
    ScopedDisabled(const ScopedDisabled&)=delete;ScopedDisabled& operator=(const ScopedDisabled&)=delete;
};
class ScopedStyle {
public:
    ScopedStyle()=default;
    ~ScopedStyle(){if(colors_)ImGui::PopStyleColor(colors_);if(variables_)ImGui::PopStyleVar(variables_);}
    ScopedStyle& color(ImGuiCol index,const ImVec4& value){ImGui::PushStyleColor(index,value);++colors_;return *this;}
    ScopedStyle& variable(ImGuiStyleVar index,float value){ImGui::PushStyleVar(index,value);++variables_;return *this;}
    ScopedStyle& variable(ImGuiStyleVar index,const ImVec2& value){ImGui::PushStyleVar(index,value);++variables_;return *this;}
    ScopedStyle(const ScopedStyle&)=delete;ScopedStyle& operator=(const ScopedStyle&)=delete;
private:int colors_=0,variables_=0;
};
}
