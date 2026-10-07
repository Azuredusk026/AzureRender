#include "EditorTaskService.hpp"
#include <algorithm>
#include <stdexcept>
namespace azurerender {
std::string EditorTaskService::add(std::string source,std::string target,EditorTaskCallbacks callbacks) {
    if(!callbacks.progress||!callbacks.ready||!callbacks.finish||!callbacks.cancel)throw std::invalid_argument("Editor tasks require complete lifecycle callbacks");
    const auto id="task-"+std::to_string(++sequence_);Task task;task.id=id;task.source=std::move(source);task.target=std::move(target);task.callbacks=std::move(callbacks);tasks_.push_back(std::move(task));return id;
}
void EditorTaskService::cancel(const std::string& id){
    const auto task=std::find_if(tasks_.begin(),tasks_.end(),[&](const auto& value){return value.id==id;});
    if(task==tasks_.end())throw std::invalid_argument("Unknown task identity");
    if(task->state!="running")return;task->callbacks.cancel();task->cancelling=true;
}
void EditorTaskService::poll(bool allowCommit) {
    for(auto& task:tasks_)if(task.state=="running"){
        task.progress=task.callbacks.progress();if(!task.callbacks.ready()||!allowCommit)continue;
        try{task.result=task.callbacks.finish();task.state=task.cancelling?"cancelled":"completed";task.progress=1;}
        catch(const std::exception& error){task.diagnostic=error.what();task.state=task.cancelling?"cancelled":"failed";if(!task.cancelling&&failure_)failure_(task.source,task.diagnostic);}
        task.callbacks={};
    }
}
nlohmann::json EditorTaskService::report() const {
    auto result=nlohmann::json::array();for(const auto& task:tasks_)result.push_back({{"id",task.id},{"source",task.source},{"target",task.target},{"state",task.state},{"progress",task.progress},{"cancelling",task.cancelling},{"diagnostic",task.diagnostic},{"result",task.result}});return result;
}
}
