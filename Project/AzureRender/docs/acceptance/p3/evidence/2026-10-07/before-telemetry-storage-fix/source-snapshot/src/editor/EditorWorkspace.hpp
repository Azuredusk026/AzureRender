#pragma once
#include <array>
#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace azurerender {
struct EditorPanelState { std::string id, title; bool visible = true; };
struct EditorWorkspaceLayout {
    float menu = 24, toolbar = 40, status = 24, left = 220, right = 320, bottom = 220;
    float viewportWidth = 0, viewportHeight = 0;
    bool compact = false;
};
class EditorWorkspace {
public:
    static constexpr int version = 2;
    EditorWorkspace() { reset(); }
    void reset();
    std::vector<EditorPanelState>& panels() { return panels_; }
    const std::vector<EditorPanelState>& panels() const { return panels_; }
    bool visible(const std::string& id) const;
    bool* open(const std::string& id);
    void setVisible(const std::string& id, bool value);
    void registerPanel(std::string id,std::string title,bool visible=true);
    bool load(const std::filesystem::path& directory) noexcept;
    void save(const std::filesystem::path& directory) const;
    static std::filesystem::path configDirectory();
    static EditorWorkspaceLayout layout(float width, float height, float dpi, bool compact=false);
    static std::array<float,2> ndcToScreen(float x, float y) { return {(x+1)*.5F,(y+1)*.5F}; }
    nlohmann::json snapshot() const;
    std::string diagnostic;
private:
    std::vector<EditorPanelState> panels_;
    std::vector<EditorPanelState> extraPanels_;
};
}
