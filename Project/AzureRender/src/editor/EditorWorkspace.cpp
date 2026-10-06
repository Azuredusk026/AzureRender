#include "EditorWorkspace.hpp"
#include "ui/UiMetrics.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
namespace azurerender {
namespace {
std::uint64_t layoutHash(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary);
    if(!file)throw std::runtime_error("Cannot read docking data");
    std::uint64_t hash=14695981039346656037ULL;
    char value;
    while(file.get(value)){hash^=static_cast<unsigned char>(value);hash*=1099511628211ULL;}
    return hash;
}
}
void EditorWorkspace::reset() {
    panels_={{"viewport","Viewport###viewport"},{"outliner","Scene Outliner###outliner"},
        {"inspector","Details###inspector"},{"assets","Content Browser###assets"},
        {"capture","Capture###capture"},{"console","Console###console"},
        {"build","Build Game###build"},{"animation","Animation Preview###animation"},
        {"gameplay-debug","Gameplay Debug###gameplay-debug"},{"settings","Settings###settings",false}};
    panels_.insert(panels_.end(),extraPanels_.begin(),extraPanels_.end());
}
void EditorWorkspace::registerPanel(std::string id,std::string title,bool visible) {
    if(id.empty()||title.empty()||id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.")!=std::string::npos
        ||title.find("###")!=std::string::npos||panels_.size()>=64)throw std::invalid_argument("Invalid panel declaration");
    for(const auto& panel:panels_)if(panel.id==id)throw std::invalid_argument("Duplicate panel identity");
    EditorPanelState panel{std::move(id),{},visible};panel.title=std::move(title)+"###"+panel.id;
    extraPanels_.push_back(panel);panels_.push_back(std::move(panel));
}
bool* EditorWorkspace::open(const std::string& id) {
    for(auto& panel:panels_)if(panel.id==id)return &panel.visible;
    throw std::invalid_argument("Unknown editor panel: "+id);
}
bool EditorWorkspace::visible(const std::string& id) const {
    for(const auto& panel:panels_)if(panel.id==id)return panel.visible;
    throw std::invalid_argument("Unknown editor panel: "+id);
}
void EditorWorkspace::setVisible(const std::string& id,bool value){*open(id)=value;}
nlohmann::json EditorWorkspace::snapshot() const {
    nlohmann::json result={{"version",version},{"panels",nlohmann::json::object()}};
    for(const auto& panel:panels_)result["panels"][panel.id]=panel.visible;
    return result;
}
bool EditorWorkspace::load(const std::filesystem::path& directory) noexcept {
    reset();diagnostic.clear();
    if(!std::filesystem::exists(directory/"workspace.json"))return false;
    try {
        std::ifstream file(directory/"workspace.json");nlohmann::json state;file>>state;
        if(state.at("version")!=version)throw std::runtime_error("layout version");
        if(std::filesystem::exists(directory/"layout.ini") &&
            state.at("layoutHash").get<std::uint64_t>()!=layoutHash(directory/"layout.ini"))
            throw std::runtime_error("docking data integrity");
        auto candidate=panels_;
        for(auto& panel:candidate) {
            const bool extension=panel.id=="settings"||std::any_of(extraPanels_.begin(),extraPanels_.end(),[&](const auto& extra){return extra.id==panel.id;});
            if(extension&&!state.at("panels").contains(panel.id))continue;
            panel.visible=state.at("panels").at(panel.id).get<bool>();
        }
        panels_=std::move(candidate);return true;
    }catch(...) { diagnostic="Invalid workspace configuration. Default layout loaded.";return false; }
}
void EditorWorkspace::save(const std::filesystem::path& directory) const {
    std::filesystem::create_directories(directory);
    const auto pending=directory/"workspace.pending";
    auto state=snapshot();
    if(std::filesystem::exists(directory/"layout.ini"))state["layoutHash"]=layoutHash(directory/"layout.ini");
    {std::ofstream file(pending);file<<state.dump(2);if(!file)throw std::runtime_error("Cannot save workspace");}
    // Replace only the editor-owned state file.
    std::filesystem::copy_file(pending,directory/"workspace.json",std::filesystem::copy_options::overwrite_existing);
    std::filesystem::remove(pending);
}
std::filesystem::path EditorWorkspace::configDirectory() {
    if(const char* value=std::getenv("AZURERENDER_EDITOR_CONFIG"))return std::filesystem::u8path(value);
    if(const char* value=std::getenv("LOCALAPPDATA"))return std::filesystem::u8path(value)/"AzureRender/editor";
    if(const char* value=std::getenv("XDG_CONFIG_HOME"))return std::filesystem::u8path(value)/"AzureRender/editor";
    if(const char* value=std::getenv("HOME"))return std::filesystem::u8path(value)/".config/AzureRender/editor";
    return std::filesystem::temp_directory_path()/"AzureRender/editor";
}
EditorWorkspaceLayout EditorWorkspace::layout(float width,float height,float dpi,bool compact) {
    if(!std::isfinite(dpi)||dpi<=0)throw std::invalid_argument("Invalid DPI");
    dpi=std::clamp(dpi,.75F,3.F);
    const auto metrics=ui::UiMetrics::fromScale(dpi);
    EditorWorkspaceLayout r;r.menu=metrics.menuHeight;r.toolbar=metrics.toolbarHeight;r.status=metrics.statusHeight;
    r.compact=compact || width/dpi<1280 || height/dpi<720;
    r.left=r.compact?0:220*dpi;r.right=std::min(320*dpi,width*.26F);
    r.bottom=r.compact?60*dpi:220*dpi;
    r.viewportWidth=width-r.left-r.right;
    r.viewportHeight=height-r.menu-r.toolbar-r.status-r.bottom-36*dpi;
    return r;
}
}
