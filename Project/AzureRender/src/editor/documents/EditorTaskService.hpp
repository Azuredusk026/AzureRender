#pragma once
#include <nlohmann/json.hpp>
#include <functional>
#include <string>
#include <vector>
namespace azurerender {
struct EditorTaskCallbacks {
    std::function<float()> progress;
    std::function<bool()> ready;
    std::function<nlohmann::json()> finish;
    std::function<void()> cancel;
};
class EditorTaskService {
public:
    using Failure=std::function<void(const std::string&,const std::string&)>;
    explicit EditorTaskService(Failure failure):failure_(std::move(failure)){}
    std::string add(std::string source,std::string target,EditorTaskCallbacks callbacks);
    void cancel(const std::string& id);
    void poll(bool allowCommit=true);
    nlohmann::json report() const;
private:
    struct Task {std::string id,source,target,state="running",diagnostic;float progress=0;bool cancelling=false;EditorTaskCallbacks callbacks;nlohmann::json result;};
    std::vector<Task> tasks_;
    Failure failure_;
    std::uint64_t sequence_=0;
};
}
