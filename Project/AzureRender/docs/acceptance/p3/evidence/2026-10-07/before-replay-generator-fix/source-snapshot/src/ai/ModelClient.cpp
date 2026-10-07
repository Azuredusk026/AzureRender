#include "ModelClient.hpp"
#include "foundation/SettingRegistry.hpp"
#include <cmath>
#include <stdexcept>
namespace azurerender {
void registerModelSettings(SettingRegistry& settings){
    settings.add({"ai.enabled","Enable optional content assistance",false,{},{},false,false,true});
    settings.add({"ai.profile","Model tool profile","content",{},{},false,true,false});
    settings.add({"ai.timeoutMs","Total proposal deadline",300000,1.,300000.,false,true,false});
    settings.add({"ai.maxOperations","Maximum proposal operations",128,1.,128.,false,true,false});
    settings.add({"ai.maxSourceBytes","Maximum proposal source bytes",2097152,1.,2097152.,false,true,false});
    settings.add({"ai.maxHistoryMessages","Maximum proposal history messages",24,1.,24.,false,true,false});
    settings.add({"ai.maxHistoryBytes","Maximum proposal history bytes",131072,2.,131072.,false,true,false});
    settings.add({"ai.maxRepairs","Maximum proposal repairs",1,0.,1.,false,true,false});
}
namespace {
using Json=nlohmann::json;
void integer(const Json& value,std::uint64_t expected){
    if(!value.is_number_integer()||value!=expected)throw std::invalid_argument("Invalid model protocol identity or version");
}
void envelope(const Json& frame){
    if(!frame.is_object()||frame.at("jsonrpc")!="2.0")throw std::invalid_argument("Invalid model JSON-RPC envelope");
}
void finite(const Json& value){
    if(value.is_number_float()&&!std::isfinite(value.get<double>()))throw std::invalid_argument("Nonfinite model input");
    if(value.is_structured())for(const auto& item:value)finite(item);
}
}
void validateModelRequest(const ModelRequest& request){
    if(request.runId.empty()||request.runId.size()>128||request.profile.empty()||request.profile.size()>128||request.prompt.empty()
        ||request.prompt.size()>2*1024*1024||request.timeoutMs<1||request.timeoutMs>300000)
        throw std::invalid_argument("Model request identity, source or deadline exceeds budget");
    finite(request.schema);finite(request.history);
    if(!request.schema.is_object()||request.schema.dump().size()>2*1024*1024||!request.history.is_array()
        ||request.history.size()>24||request.history.dump().size()>128*1024)throw std::invalid_argument("Model schema or history exceeds budget");
    for(const auto& row:request.history){
        if(!row.is_object()||row.size()!=2||!row.at("content").is_string()||(row.at("role")!="user"&&row.at("role")!="assistant"))
            throw std::invalid_argument("Invalid model history message");
    }
}
void validateModelHandshake(const Json& frame,std::uint64_t identity){
    envelope(frame);integer(frame.at("id"),identity);integer(frame.at("result").at("protocolVersion"),1);
    if(frame.size()!=3)throw std::invalid_argument("Unknown model handshake fields");
}
std::optional<ModelResponse> ModelFrameDecoder::accept(const std::string& line){
    if(finished_||line.size()>8*1024*1024)throw std::invalid_argument("Model frame exceeds its lifecycle or budget");
    const auto frame=Json::parse(line);envelope(frame);
    if(frame.contains("method")){
        if(frame.size()!=3||frame.at("method")!="run.event")throw std::invalid_argument("Unknown model event");
        const auto& parameters=frame.at("params");
        if(parameters.size()!=3||parameters.at("runId")!=runId_||!parameters.at("delta").is_string())throw std::invalid_argument("Invalid model event correlation");
        integer(parameters.at("sequence"),sequence_+1);
        eventBytes_+=parameters.at("delta").get_ref<const std::string&>().size();
        if(eventBytes_>2*1024*1024||events_.size()>=1024)throw std::invalid_argument("Model stream exceeds source budget");
        ++sequence_;events_.push_back(parameters);return {};
    }
    if(frame.size()!=3)throw std::invalid_argument("Unknown model response fields");
    integer(frame.at("id"),identity_);finished_=true;
    ModelResponse response;response.events=std::move(events_);
    if(frame.contains("error")){
        response.diagnostic=frame.at("error").at("code").get<std::string>()+": "+frame.at("error").at("message").get<std::string>();return response;
    }
    const auto& result=frame.at("result");
    if(!result.is_object()||result.size()!=3||result.at("runId")!=runId_)throw std::invalid_argument("Invalid model result correlation");
    response.content=result.at("content").get<std::string>();response.structuredMode=result.at("structuredMode").get<std::string>();
    if(response.content.size()>2*1024*1024||(response.structuredMode!="native"&&response.structuredMode!="json"&&response.structuredMode!="prompt"))
        throw std::invalid_argument("Invalid model structured mode or source budget");
    response.passed=true;return response;
}
std::shared_future<ModelResponse> ModelClient::request(const ModelRequest& request){
    try{validateModelRequest(request);if(transport_)return transport_->request(request);}
    catch(const std::exception& error){std::promise<ModelResponse> result;result.set_value({false,{},error.what(),{}});return result.get_future().share();}
    std::promise<ModelResponse> result;result.set_value({false,{},"Optional model service unavailable",{}});return result.get_future().share();
}
}
