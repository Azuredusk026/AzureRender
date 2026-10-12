#include "ProjectCenterController.hpp"
#include <chrono>
#include <stdexcept>
namespace azurerender {
ProjectCenterController::ProjectCenterController(std::filesystem::path history) : service_(std::move(history)) {}
void ProjectCenterController::open(std::filesystem::path path) {
    if(busy()) throw std::logic_error("A project task is already running");
    diagnostic_.clear(); operation_="Validating and loading project"; state_=ApplicationState::ProjectLoading;
    task_=std::async(std::launch::async,[this,path=std::move(path)] { return service_.open(path); });
}
void ProjectCenterController::create(std::string kind,std::filesystem::path destination,std::string name) {
    if(busy()) throw std::logic_error("A project task is already running");
    diagnostic_.clear(); operation_="Creating template and validating project"; state_=ApplicationState::ProjectLoading;
    task_=std::async(std::launch::async,[this,kind=std::move(kind),destination=std::move(destination),name=std::move(name)] { return service_.create(kind,destination,name); });
}
void ProjectCenterController::poll() {
    if(!task_.valid() || task_.wait_for(std::chrono::seconds(0))!=std::future_status::ready) return;
    try { ready_=task_.get(); service_.remember(ready_->project()); state_=ApplicationState::Editor; }
    catch(const std::exception& error) { ready_.reset(); diagnostic_=error.what(); state_=ApplicationState::ProjectCenter; }
}
std::shared_ptr<EditorContext> ProjectCenterController::takeReady() { return std::exchange(ready_,{}); }
}
