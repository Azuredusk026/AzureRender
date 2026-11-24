#pragma once
#include "editor/EditorSession.hpp"
#include <chrono>
namespace azurerender {
class EditorAutomation {
public:
    explicit EditorAutomation(const std::filesystem::path& file);
    void advance(std::uint64_t frame,EditorSession& session);
    nlohmann::json report() const;
    bool complete() const noexcept { return cursor_ == actions_.size(); }
    bool needsObservations() const;
    void setObservations(std::function<nlohmann::json(const std::string&)> query) { query_=std::move(query); }
private:
    nlohmann::json actions_,results_=nlohmann::json::array(),editBefore_;
    std::size_t cursor_=0;
    std::string imported_;
    std::map<std::string,std::string> imports_;
    std::uint64_t steps_=0,revision_=0;
    std::size_t scriptErrors_=0,recovered_=0;
    bool restored_=true;
    std::function<nlohmann::json(const std::string&)> query_;
    std::optional<std::chrono::steady_clock::time_point> waitStarted_;
};
}
