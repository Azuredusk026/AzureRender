#include "Widgets.hpp"
namespace azurerender::ui {
void resultMessage(const std::string& message,bool failed) {
    if(message.empty())return;
    const auto theme=ThemeTokens::dark();ImGui::TextColored(failed?theme.error:theme.success,"%s",message.c_str());
}
}
