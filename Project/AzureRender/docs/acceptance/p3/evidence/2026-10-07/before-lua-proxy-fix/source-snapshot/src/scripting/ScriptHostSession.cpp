#include "scripting/ScriptHostSession.hpp"
#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <stdexcept>
namespace azurerender {
namespace {
struct SessionState {
    std::shared_ptr<ScriptBindingHost> host;
    std::thread::id owner=std::this_thread::get_id();
};
std::mutex sessionsMutex;
std::map<std::uint64_t,std::shared_ptr<SessionState>> sessions;
std::uint64_t nextSession=1;
std::int32_t invoke(std::uint64_t token,const std::uint8_t* input,std::uint32_t inputSize,
    std::uint8_t* output,std::uint32_t capacity,std::uint32_t* written) noexcept {
    if(!written)return 1;
    *written=0;
    // All callers reserve the response budget before any operation executes.
    if(!output || capacity<kScriptInteropByteLimit)return 2;
    std::string response;
    std::int32_t status=0;
    try {
        if(!input || !inputSize || inputSize>kScriptInteropByteLimit)
            throw std::invalid_argument("Invalid script request byte length");
        std::shared_ptr<SessionState> state;
        {std::lock_guard<std::mutex> lock(sessionsMutex);
         const auto found=sessions.find(token);if(found!=sessions.end())state=found->second;}
        if(!state)throw std::runtime_error("Script session is expired");
        if(state->owner!=std::this_thread::get_id())throw std::runtime_error("Script session owner thread required");
        const auto request=nlohmann::json::parse(input,input+inputSize);
        if(!request.is_object() || request.size()!=4 || !request.at("apiVersion").is_number_integer()
            || request.at("apiVersion")!=1 || !request.at("method").is_string())
            throw std::invalid_argument("Invalid script request or API version");
        const auto value=state->host->invoke(ScriptBindingHost::decode(request.at("object")),
            request.at("method").get<std::string>(),request.at("arguments"));
        response=nlohmann::json{{"ok",true},{"value",value}}.dump();
        if(response.size()>kScriptInteropByteLimit)throw std::length_error("Script response exceeds byte budget");
    } catch(const std::exception& error) {
        status=1;
        try{response=nlohmann::json{{"ok",false},{"error",std::string(error.what()).substr(0,4096)}}.dump();}
        catch(...){response="{\"ok\":false,\"error\":\"Script callback failed\"}";}
    } catch(...) {status=1;response="{\"ok\":false,\"error\":\"Script callback failed\"}";}
    std::memcpy(output,response.data(),response.size());*written=static_cast<std::uint32_t>(response.size());
    return status;
}
}
ScriptHostSession::ScriptHostSession(std::shared_ptr<ScriptBindingHost> host) {
    if(!host)throw std::invalid_argument("Script session requires a binding host");
    auto state=std::make_shared<SessionState>();state->host=std::move(host);
    std::lock_guard<std::mutex> lock(sessionsMutex);
    if(nextSession==std::numeric_limits<std::uint64_t>::max())throw std::overflow_error("Script session identities exhausted");
    api_.session=nextSession++;api_.invoke=&invoke;sessions.emplace(api_.session,std::move(state));
}
void ScriptHostSession::close() noexcept {
    std::lock_guard<std::mutex> lock(sessionsMutex);sessions.erase(api_.session);api_.session=0;
}
}
