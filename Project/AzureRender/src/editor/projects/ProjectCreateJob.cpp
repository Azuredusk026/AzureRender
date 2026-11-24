#include "ProjectCreateJob.hpp"
#include "editor/commands/DocumentVersion.hpp"
namespace azurerender {
ProjectCreateJob::ProjectCreateJob(std::string templateId,std::filesystem::path destination,std::string name) {
    if(destination.empty())throw std::invalid_argument("Project destination is required");
    destination_=std::filesystem::absolute(destination).lexically_normal();
    if(std::filesystem::exists(destination_))throw std::invalid_argument("Project destination already exists");
    temporary_=destination_.parent_path()/(".azure-task-"+newDocumentIdentity());
    future_=std::async(std::launch::async,[this,templateId=std::move(templateId),name=std::move(name)]{
        progress_=.1F;ProjectOpenService service({});
        auto context=service.create(templateId,temporary_,name,[this]{return cancelled_.load();});
        progress_=.9F;return context;
    });
}
ProjectCreateJob::~ProjectCreateJob() {
    cancelled_=true;
    if(future_.valid())future_.wait();
    std::error_code error;std::filesystem::remove_all(temporary_,error);
}
bool ProjectCreateJob::ready() const {
    return future_.valid()&&future_.wait_for(std::chrono::seconds(0))==std::future_status::ready;
}
std::shared_ptr<EditorContext> ProjectCreateJob::finish() {
    if(!ready())throw std::logic_error("Project creation is still running");
    auto prepared=future_.get();prepared.reset();
    if(cancelled_)throw std::runtime_error("Project creation cancelled");
    // Rename commits a validated project on the UI thread. A competing target is rejected.
    std::filesystem::rename(temporary_,destination_);
    progress_=1;return ProjectOpenService({}).open(destination_/"project.azureproject");
}
}
