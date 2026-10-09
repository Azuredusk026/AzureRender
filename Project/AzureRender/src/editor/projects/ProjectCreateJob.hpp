#pragma once
#include "ProjectOpenService.hpp"
#include <atomic>
#include <future>
namespace azurerender {
class ProjectCreateJob {
public:
    ProjectCreateJob(std::string templateId,std::filesystem::path destination,std::string name);
    ~ProjectCreateJob();
    bool ready() const;
    float progress() const {return progress_.load();}
    void cancel() {cancelled_=true;}
    std::shared_ptr<EditorContext> finish();
private:
    std::filesystem::path destination_,temporary_;
    std::atomic<bool> cancelled_{false};
    std::atomic<float> progress_{0};
    std::future<std::shared_ptr<EditorContext>> future_;
};
}
