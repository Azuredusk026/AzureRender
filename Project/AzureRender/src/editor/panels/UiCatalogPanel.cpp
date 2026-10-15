#include "ui/localization/Localization.hpp"
#include "UiCatalogPanel.hpp"
#include "ui/Widgets.hpp"
#include "ui/BrandMark.hpp"
#include "ui/Containers.hpp"
namespace azurerender {
void drawUiCatalog(){
    ui::WindowScope window(ui::tr("控件目录###ui-catalog"));if(!window)return;
    ui::brandMark(48);ui::sectionHeading(ui::tr("公共控件与状态"),ui::tr("项目中心与编辑器共享此设计系统。"));
    for(auto variant:{ui::ButtonVariant::Default,ui::ButtonVariant::Primary,ui::ButtonVariant::Toolbar,ui::ButtonVariant::Danger}){
        ImGui::PushID(static_cast<int>(variant));ui::ButtonOptions options;options.variant=variant;
        ui::button(ui::tr("普通状态"),options);ImGui::SameLine();options.selected=true;ui::button(ui::tr("选中状态"),options);
        ImGui::SameLine();options.disabled=true;options.tooltip=ui::tr("示例：当前操作不可用");ui::button(ui::tr("禁用状态"),options);ImGui::PopID();
    }
    for(auto kind:{ui::StatusKind::Neutral,ui::StatusKind::Info,ui::StatusKind::Warning,ui::StatusKind::Error,ui::StatusKind::Success})ui::statusText(ui::tr("带文字说明的状态反馈"),kind);
}
}
