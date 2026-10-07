#include "validation/ValidationService.hpp"
#include <algorithm>
#include <stdexcept>
namespace azurerender {
namespace {
using Json=nlohmann::json;
Json failure(const char* code,const std::string& message) { return {{"passed",false},{"code",code},{"message",message}}; }
}
ValidationService::ValidationService(ObservationRegistry& observations,ValidationCallbacks callbacks)
    :observations_(observations),callbacks_(std::move(callbacks)) {}
ValidationService::~ValidationService() { stop(); }
void ValidationService::checkThread() const { if(owner_!=std::this_thread::get_id())throw std::logic_error("Validation requires host thread"); }
std::future<ValidationService::Json> ValidationService::submit(Json request) {
    auto job=std::make_shared<Pending>();job->request=std::move(request);auto future=job->result.get_future();
    std::lock_guard<std::mutex> lock(mutex_);
    if(stopped_)job->result.set_value(failure("Stopped","Validation service is closed"));
    else if(job->request.dump().size()>65536)job->result.set_value(failure("RequestTooLarge","Request exceeds 64 KiB"));
    else if(pending_.size()>=64)job->result.set_value(failure("QueueFull","Request queue exceeds 64 entries"));
    else pending_.push_back(job);
    return future;
}
std::optional<ValidationService::Json> ValidationService::evaluate(Pending& job,std::uint64_t frame,Clock::time_point now) {
    const auto& request=job.request;
    if(!request.is_object()||!request.at("schemaVersion").is_number_integer()||request.at("schemaVersion")!=1)throw std::invalid_argument("Unsupported validation protocol");
    const auto timeout=request.value("timeoutMs",std::int64_t(10000));
    if(timeout<1||timeout>60000)throw std::invalid_argument("timeoutMs requires 1..60000");
    if(now-job.queued>=std::chrono::milliseconds(timeout))return failure("Timeout","Request deadline elapsed");
    const auto op=request.at("op").get<std::string>();Json value;
    if(op=="describe") { value={{"schemaVersion",1},{"observations",observations_.names()},{"operations",Json::array({"describe","query","assert","wait-until","wait-frames","edit","input","screenshot"})}};
        if(callbacks_.describe)value["edits"]=callbacks_.describe(request); }
    else if(op=="query"||op=="assert"||op=="wait-until") {
        value=observations_.query(request.at("name").get<std::string>());
        if(op!="query"&&value!=request.at("equals")) {
            if(op=="wait-until")return std::nullopt;
            return failure("AssertionFailed","Observation differs from expected value");
        }
    }else if(op=="wait-frames") {
        const auto frames=request.at("frames").get<std::int64_t>();
        if(frames<0||frames>100000)throw std::invalid_argument("Invalid frame wait budget");
        if(!job.firstFrame)job.firstFrame=frame;
        if(frame-*job.firstFrame<static_cast<std::uint64_t>(frames))return std::nullopt;
    }else {
        const auto* callback=op=="edit"?&callbacks_.edit:op=="input"?&callbacks_.input:op=="screenshot"?&callbacks_.capture:nullptr;
        if(!callback||!*callback)throw std::invalid_argument("Unavailable operation: "+op);
        value=(*callback)(request);
    }
    return Json{{"passed",true},{"value",value}};
}
void ValidationService::pump(std::uint64_t frame,Clock::time_point now) {
    checkThread();
    if(cursor_<steps_.size()&&!step_.valid()) {
        auto step=steps_[cursor_];step["schemaVersion"]=1;step_=submit(std::move(step));
    }
    std::deque<std::shared_ptr<Pending>> jobs;
    { std::lock_guard<std::mutex> lock(mutex_);jobs=pending_; }
    for(const auto& job:jobs) {
        std::optional<Json> result;
        try { result=evaluate(*job,frame,now); }
        catch(const std::exception& e) { result=failure("InvalidRequest",e.what()); }
        if(!result)continue;
        (*result)["elapsedMs"]=std::chrono::duration<double,std::milli>(now-job->queued).count();
        if(result->dump().size()>1024*1024)result=failure("ResponseTooLarge","Response exceeds 1 MiB");
        { std::lock_guard<std::mutex> lock(mutex_);
          auto found=std::find(pending_.begin(),pending_.end(),job);
          if(found==pending_.end())continue;
          pending_.erase(found);job->result.set_value(std::move(*result)); }
    }
    if(step_.valid()&&step_.wait_for(std::chrono::milliseconds(0))==std::future_status::ready) {
        auto result=step_.get();result["step"]=cursor_;results_.push_back(result);++cursor_;
        if(!result.at("passed").get<bool>())cursor_=steps_.size();
    }
    if(cursor_<steps_.size()&&now-scriptStart_>std::chrono::seconds(300)) {
        results_.push_back(failure("Timeout","Script exceeds 300 seconds"));cursor_=steps_.size();stop();
    }
}
void ValidationService::stop() {
    std::lock_guard<std::mutex> lock(mutex_);stopped_=true;
    for(const auto& job:pending_)job->result.set_value(failure("Stopped","Validation service is closed"));
    pending_.clear();
}
void ValidationService::loadScript(const Json& script) {
    checkThread();if(step_.valid()||cursor_<steps_.size())throw std::logic_error("Script is already active");
    if(!script.at("schemaVersion").is_number_integer()||script.at("schemaVersion")!=1||!script.at("steps").is_array()||script.at("steps").size()>1024||script.dump().size()>2*1024*1024)
        throw std::invalid_argument("Invalid validation script");
    steps_=script.at("steps");cursor_=0;results_=Json::array();scriptStart_=Clock::now();
}
bool ValidationService::complete() const { checkThread();return cursor_==steps_.size(); }
ValidationService::Json ValidationService::report() const {
    checkThread();bool passed=complete();for(const auto& result:results_)passed=passed&&result.at("passed").get<bool>();
    return {{"schemaVersion",1},{"passed",passed},{"completedSteps",results_.size()},{"plannedSteps",steps_.size()},{"steps",results_}};
}
}
