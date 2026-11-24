#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>
namespace azurerender {
class SettingRegistry;
void registerModelSettings(SettingRegistry& settings);
struct ModelRequest {
    std::string runId,prompt,profile="content";
    nlohmann::json schema=nlohmann::json::object(),history=nlohmann::json::array();
    std::uint32_t timeoutMs=300000;
};
struct ModelResponse {
    bool passed=false;
    std::string content,diagnostic,structuredMode;
    nlohmann::json events=nlohmann::json::array();
};
class IModelTransport {
public:
    virtual ~IModelTransport()=default;
    virtual std::shared_future<ModelResponse> request(const ModelRequest& request)=0;
    virtual void cancel(const std::string& runId)=0;
};
void validateModelRequest(const ModelRequest& request);
void validateModelHandshake(const nlohmann::json& frame,std::uint64_t identity);
class ModelFrameDecoder {
public:
    ModelFrameDecoder(std::uint64_t identity,std::string runId):identity_(identity),runId_(std::move(runId)){}
    std::optional<ModelResponse> accept(const std::string& line);
private:
    std::uint64_t identity_,sequence_=0;
    std::string runId_;
    std::size_t eventBytes_=0;
    nlohmann::json events_=nlohmann::json::array();
    bool finished_=false;
};
class ModelClient {
public:
    explicit ModelClient(std::shared_ptr<IModelTransport> transport={}):transport_(std::move(transport)){}
    std::shared_future<ModelResponse> request(const ModelRequest& request);
    void cancel(const std::string& runId){if(transport_)transport_->cancel(runId);}
    bool available() const noexcept{return transport_!=nullptr;}
private:
    std::shared_ptr<IModelTransport> transport_;
};
struct ModelProcessOptions {
    std::filesystem::path executable,workingDirectory;
    std::vector<std::string> arguments;
};
class ModelProcess final:public IModelTransport {
public:
    explicit ModelProcess(ModelProcessOptions options);
    ~ModelProcess() override;
    ModelProcess(const ModelProcess&)=delete;
    ModelProcess& operator=(const ModelProcess&)=delete;
    std::shared_future<ModelResponse> request(const ModelRequest& request) override;
    void cancel(const std::string& runId) override;
private:
    struct State;
    std::unique_ptr<State> state_;
};
}
