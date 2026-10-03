#pragma once
#include "editor/EditorSession.hpp"
namespace azurerender {
class EditorAutomation {
public:
    explicit EditorAutomation(const std::filesystem::path& file);
    void advance(std::uint64_t frame,EditorSession& session);
    nlohmann::json report() const;
    bool complete() const noexcept { return cursor_ == actions_.size(); }
private:
    nlohmann::json actions_,results_=nlohmann::json::array(),editBefore_;
    std::size_t cursor_=0;
    std::string imported_;
    std::uint64_t steps_=0,revision_=0;
    std::size_t scriptErrors_=0,recovered_=0;
    bool restored_=true;
};
}
