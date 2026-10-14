#include "editor/ImGuiEditorLayer.hpp"
#include "ui/Widgets.hpp"
#include "ui/UiScopes.hpp"
#include "reflection/Registry.hpp"
#include "runtime/ComponentRegistry.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#ifdef AZURERENDER_HAS_IMGUI
#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
namespace azurerender {
#ifndef IMGUI_HAS_DOCK
namespace {
void setFallbackPanelRect(float x,float y,float width,float height) {
    const auto display=ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({display.x*x,display.y*y},ImGuiCond_Always);
    ImGui::SetNextWindowSize({display.x*width,display.y*height},ImGuiCond_Always);
}
}
#endif
void ImGuiEditorLayer::drawConsolePanel(PanelContext&) {
#ifndef IMGUI_HAS_DOCK
    setFallbackPanelRect(0.50F, 0.72F, 0.50F, 0.28F);
#endif
    if(!ImGui::Begin("日志###console",workspace_.open("console"))){ImGui::End();return;}
    const auto metrics=ui::UiMetrics::fromScale(dpi_);
    const auto tasks=session_->tasks().report(),feedback=session_->feedback().report();
    const auto developer=session_->developerServices().report?session_->developerServices().report():nlohmann::json::object();
    const bool hasDetails=!tasks.empty()||!feedback.empty()||(developer.contains("shaderAvailable")&&developer.at("shaderAvailable").get<bool>())
        ||(session_->scripts()&&!session_->scripts()->errors().empty())||(session_->levels()&&!session_->levels()->lastError().empty())
        ||(session_->presentation()&&!session_->presentation()->errors().empty());
    const bool compact=ImGui::GetContentRegionAvail().y<metrics.compactPanelHeight;
    const auto drawDetails=[&](float height){
    const auto latestFeedback=feedback.empty()?std::string():feedback.back().at("id").get<std::string>();
    ImGui::BeginChild("console-diagnostics",{0,height},ImGuiChildFlags_Borders);
    for(const auto& task:tasks) {
        const auto state=task.at("state").get<std::string>();
        ui::statusText((task.at("source").get<std::string>()+": "+state).c_str(),state=="failed"?ui::StatusKind::Error:state=="completed"?ui::StatusKind::Success:ui::StatusKind::Info);
        if(task.at("state")=="running") {
            ImGui::ProgressBar(task.at("progress").get<float>());
            const auto id=task.at("id").get<std::string>();ImGui::PushID(id.c_str());
            if(ui::button("取消任务"))session_->edit("tasks.cancel",{{"id",id}});
            observeWidget("tasks.cancel."+id);ImGui::PopID();
        }
        const auto diagnostic=task.at("diagnostic").get<std::string>();
        if(!diagnostic.empty())ui::resultMessage(diagnostic,state=="failed");
    }
    for(const auto& error:feedback) {
        const auto id=error.at("id").get<std::string>();ImGui::PushID(id.c_str());
        ui::resultMessage(error.at("source").get<std::string>()+": "+error.at("message").get<std::string>(),true);
        if(ui::button("Dismiss"))session_->edit("feedback.dismiss",{{"id",id}});
        observeWidget("feedback.dismiss."+id);
        if(id==latestFeedback&&id!=consoleLastFeedback_)ImGui::SetScrollHereY(1);
        ImGui::PopID();
    }
    if(!developer.empty()){
        if(developer.at("shaderAvailable").get<bool>()){
            ImGui::Text("Shader reload: %s",developer.at("shaderState").get<std::string>().c_str());
            if(ui::button("Rebuild shaders"))session_->edit("developer.shader-rebuild");observeWidget("developer.shader-rebuild");
            const auto diagnostic=developer.at("shaderDiagnostic").get<std::string>();if(!diagnostic.empty())ui::resultMessage(diagnostic,developer.at("shaderState")=="failed");
        }
    }
    if(auto* scripts=session_->scripts())for(const auto& error:scripts->errors())ui::resultMessage("Script: "+error,true);
    if(auto* levels=session_->levels())if(!levels->lastError().empty())ui::resultMessage("Level: "+levels->lastError(),true);
    if(auto* presentation=session_->presentation())for(const auto& error:presentation->errors())ui::resultMessage("Presentation: "+error,true);
    consoleLastFeedback_=latestFeedback;ImGui::EndChild();observeWidget("console.diagnostics-region");
    };
    if(!compact&&hasDetails&&ImGui::CollapsingHeader("任务与诊断",ImGuiTreeNodeFlags_DefaultOpen)){
        drawDetails(std::max(ImGui::GetFrameHeight(),std::min(metrics.controlHeight*4,ImGui::GetContentRegionAvail().y*.35F)));
    }
    const auto searchWidth=ImGui::GetContentRegionAvail().x>metrics.widePanelWidth?ImGui::GetContentRegionAvail().x-metrics.filterWidth-metrics.spacing:-1;
    ui::searchField("##Search logs","搜索日志",consoleFilter_,searchWidth);observeWidget("console.search");
    ui::nextToolbarItem(metrics.filterWidth);ImGui::SetNextItemWidth(metrics.filterWidth);
    const char* labels[]={"全部级别","警告及错误","仅错误"};
    const char* ids[]={"console.level.info","console.level.warning","console.level.error"};
    if(ImGui::BeginCombo("##Level",labels[consoleLevel_])){
        for(int index=0;index<3;++index){if(ImGui::Selectable(labels[index],consoleLevel_==index))consoleLevel_=index;observeWidget(ids[index]);}
        ImGui::EndCombo();
    }observeWidget("console.level");
    bool paused=consoleLogs_.paused();
    ImGui::Checkbox("暂停日志",&paused);consoleLogs_.setPaused(paused);observeWidget("console.pause");
    if(ImGui::IsItemHovered())ImGui::SetTooltip("固定日志快照；后台任务继续运行");
    ui::nextToolbarItem(ImGui::GetFrameHeight()+ImGui::CalcTextSize("自动跟随").x+metrics.spacing);
    ImGui::Checkbox("自动跟随",&consoleAutoFollow_);observeWidget("console.follow");
    consoleLogs_.update(RuntimeDiagnostics::instance().messages());
    const auto minimum=static_cast<DiagnosticLevel>(consoleLevel_);
    const auto rows=consoleLogs_.visible(minimum,[&](const auto& text){return consoleFilter_.PassFilter(text.c_str());});
    ui::nextToolbarItem(ui::buttonWidth("复制可见日志"));
    if(ui::button("复制可见日志",{ui::ButtonVariant::Default,false,rows.empty()}))ImGui::SetClipboardText(consoleLogs_.copyVisible(rows).c_str());observeWidget("console.copy");
    if(compact&&hasDetails){
        ui::nextToolbarItem(ui::buttonWidth("任务与诊断"));
        if(ui::button("任务与诊断"))ImGui::OpenPopup("Console diagnostics");observeWidget("console.diagnostics");
        const auto display=ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowSize({std::min(metrics.widePanelWidth*2,display.x-metrics.spacing*4),0},ImGuiCond_Appearing);
        if(ImGui::BeginPopup("Console diagnostics")){
            drawDetails(std::min(metrics.controlHeight*12,display.y*.6F));
            ImGui::EndPopup();
        }
    }
    consoleView_={{"minimum",diagnosticLevelName(minimum)},{"paused",paused},{"autoFollow",consoleAutoFollow_},{"search",consoleFilter_.InputBuf},{"total",consoleLogs_.entries().size()},{"visibleCount",rows.size()},{"visible",nlohmann::json::array()}};
    ImGui::BeginChild("console-log-list",{0,0},ImGuiChildFlags_Borders);
    for(const auto index:rows){
        const auto& entry=consoleLogs_.entries()[index];
        consoleView_["visible"].push_back({{"level",diagnosticLevelName(entry.level)},{"text",entry.text}});
        ui::statusText(entry.text.c_str(),entry.level==DiagnosticLevel::Error?ui::StatusKind::Error:entry.level==DiagnosticLevel::Warning?ui::StatusKind::Warning:ui::StatusKind::Neutral);
    }
    if(rows.empty())ui::emptyState("没有匹配的日志","调整搜索或级别；恢复日志可查看最新诊断。");
    const auto latest=consoleLogs_.entries().empty()?std::string():consoleLogs_.entries().back().text;
    if(consoleAutoFollow_&&!paused&&latest!=consoleLastLine_&&ImGui::GetScrollY()>=ImGui::GetScrollMaxY()-ImGui::GetTextLineHeightWithSpacing())ImGui::SetScrollHereY(1);
    consoleLastLine_=latest;ImGui::EndChild();
    ImGui::End();
}


}
#endif
