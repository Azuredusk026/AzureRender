#include "Containers.hpp"
namespace azurerender::ui {
WindowScope::WindowScope(const char* title,bool* open,ImGuiWindowFlags flags):visible_(ImGui::Begin(title,open,flags)){}
WindowScope::~WindowScope(){ImGui::End();}
ChildScope::ChildScope(const char* id,ImVec2 size,ImGuiChildFlags flags,ImGuiWindowFlags windowFlags):visible_(ImGui::BeginChild(id,size,flags,windowFlags)){}
ChildScope::~ChildScope(){ImGui::EndChild();}
}
