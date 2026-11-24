#include "ui/ThemeTokens.hpp"
#include "ui/UiMetrics.hpp"
#include "ui/UiScopes.hpp"
#include "ui/Widgets.hpp"
#include <imgui_internal.h>
#include "ui/Containers.hpp"
#include "ui/UiContext.hpp"
#include <iostream>
#include <cmath>
using namespace azurerender::ui;
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main(){try{
    const auto theme=ThemeTokens::dark();
    const auto luminance=[](ImVec4 c){auto linear=[](float v){return v<=.04045F?v/12.92F:std::pow((v+.055F)/1.055F,2.4F);};return .2126F*linear(c.x)+.7152F*linear(c.y)+.0722F*linear(c.z);};
    for(auto background:{theme.surface,theme.control,theme.hover})
        check((luminance(theme.muted)+.05F)/(luminance(background)+.05F)>=4.5F,"Secondary text remains readable on interactive surfaces");
    for(float scale:{.75F,1.F,1.5F,2.F,3.F}){
        const auto metrics=UiMetrics::fromScale(scale);
        check(metrics.controlHeight>=20*scale&&metrics.spacing>0,"Scaled controls remain usable");
    }
    bool bad=false;try{UiMetrics::fromScale(NAN);}catch(const std::exception&){bad=true;}
    check(bad,"Nonfinite scale rejection");
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.DisplaySize={1280,720};io.DeltaTime=1/60.F;
    unsigned char* pixels=nullptr;int width=0,height=0;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    io.Fonts->SetTexID(1);ImGui::NewFrame();ImGui::Begin("UI foundation");
    const auto colors=GImGui->ColorStack.Size,variables=GImGui->StyleVarStack.Size,ids=ImGui::GetCurrentWindow()->IDStack.Size;
    const auto earlyExit=[](){ScopedId id("early");ScopedStyle style;style.color(ImGuiCol_Button,{1,0,0,1}).variable(ImGuiStyleVar_FrameRounding,8.F);ScopedDisabled disabled(true);return;};
    earlyExit();
    check(GImGui->ColorStack.Size==colors&&GImGui->StyleVarStack.Size==variables&&ImGui::GetCurrentWindow()->IDStack.Size==ids,"Early exit restores all stacks");
    check((ImGui::GetCurrentContext()->CurrentItemFlags&ImGuiItemFlags_Disabled)==0,"Disabled scope restored");
    ButtonOptions disabled;disabled.disabled=true;disabled.selected=true;disabled.tooltip="Unavailable";
    check(!button("Disabled",disabled),"Disabled control cannot execute");
    ButtonOptions selected;selected.selected=true;selected.variant=ButtonVariant::Toolbar;button("Selected",selected);
    check(GImGui->ColorStack.Size==colors&&GImGui->StyleVarStack.Size==variables,"Widget restores semantic style");
    const auto stack=GImGui->CurrentWindowStack.Size;
    {ChildScope child("scoped child",{100,50});}
    check(GImGui->CurrentWindowStack.Size==stack,"Child scope restores parent window");
    UiContext context;check(context.titleFont()==ImGui::GetFont(),"Missing title font uses active host font");
    ImGui::End();ImGui::Render();ImGui::DestroyContext();std::cout<<"UI tokens, metrics and scopes passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';if(ImGui::GetCurrentContext())ImGui::DestroyContext();return 1;}}
