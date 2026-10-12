#pragma once
#include "ProjectOpenService.hpp"
#include <future>
namespace azurerender {
enum class ApplicationState { Boot, ProjectCenter, ProjectLoading, Editor, Shutdown };
class ProjectCenterController final {
public:
    explicit ProjectCenterController(std::filesystem::path history);
    void open(std::filesystem::path path);
    void create(std::string kind, std::filesystem::path destination, std::string name);
    void poll();
    bool busy() const { return state_ == ApplicationState::ProjectLoading; }
    ApplicationState state() const { return state_; }
    const std::string& diagnostic() const { return diagnostic_; }
    const std::string& operation() const { return operation_; }
    nlohmann::json recent() const { return service_.recent(); }
    std::shared_ptr<EditorContext> takeReady();
private:
    ProjectOpenService service_;
    ApplicationState state_ = ApplicationState::ProjectCenter;
    std::future<std::shared_ptr<EditorContext>> task_;
    std::shared_ptr<EditorContext> ready_;
    std::string diagnostic_, operation_;
};
}
