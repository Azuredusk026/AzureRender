#include "ai/ModelClient.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace azurerender;
using Json=nlohmann::json;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class Function> void reject(Function fn){bool failed=false;try{fn();}catch(const std::exception&){failed=true;}require(failed,"Illegal protocol input must be rejected");}
int main(int argc,char** argv){
    try{
        require(argc==4,"Expected protocol fixtures, Python and bridge paths");
        std::ifstream input(argv[1]);Json fixtures;input>>fixtures;
        for(const auto& row:fixtures.at("requests")){
            Json data={{"runId","run-1"},{"prompt","Build a valid candidate"},{"schema",{{"type","object"}}},
                       {"history",Json::array()},{"profile","content"},{"timeoutMs",5000}};
            data.update(row.at("overrides"));ModelRequest shared;
            shared.runId=data.at("runId");shared.prompt=data.at("prompt");shared.profile=data.at("profile");
            shared.schema=data.at("schema");shared.history=data.at("history");shared.timeoutMs=data.at("timeoutMs");
            if(row.at("valid").get<bool>())validateModelRequest(shared);else reject([&]{validateModelRequest(shared);});
        }
        for(const auto& row:fixtures.at("handshakes")){
            if(row.at("valid").get<bool>())validateModelHandshake(row.at("frame"),1);
            else reject([&]{validateModelHandshake(row.at("frame"),1);});
        }
        for(const auto& row:fixtures.at("streams")){
            const auto read=[&]{ModelFrameDecoder decoder(2,"run-1");std::optional<ModelResponse> response;
                for(const auto& frame:row.at("frames"))response=decoder.accept(frame.dump());
                require(response&&response->passed,"A valid stream must finish with its response");};
            if(row.at("valid").get<bool>())read();else reject(read);
        }
        ModelClient absent;
        ModelRequest request;request.runId="run-1";request.prompt="generate";
        require(!absent.request(request).get().passed,"Missing service must yield a recoverable failure");
        ModelFrameDecoder decoder(2,"run-1");reject([&]{decoder.accept("{");});
        reject([&]{decoder.accept(std::string(8*1024*1024+1,'x'));});
        ModelProcessOptions options;options.executable=argv[2];options.arguments={argv[3],"--fixture",fixtures.at("processFixture").get<std::string>()};
        options.workingDirectory=std::filesystem::path(argv[1]).parent_path();
        auto process=std::make_shared<ModelProcess>(options);ModelClient client(process);
        auto response=client.request(request).get();
#ifdef _WIN32
        require(response.passed&&Json::parse(response.content).at("schemaVersion")==1,"Real bridge process must return a bounded structured result");
#else
        require(!response.passed&&!response.diagnostic.empty(),"Unavailable native transport must return a recoverable diagnostic");
        require(!client.request(request).get().passed,"An unavailable transport must remain reusable after failure");
#endif
        request.timeoutMs=0;require(!client.request(request).get().passed,"Invalid deadline must be rejected before transport");
        request.timeoutMs=300000;request.history=Json::array();for(int i=0;i<25;++i)request.history.push_back({{"role","user"},{"content","x"}});
        require(!client.request(request).get().passed,"History count must remain bounded");
#ifdef _WIN32
        auto stalledOptions=options;stalledOptions.arguments={fixtures.at("stalledProcess").get<std::string>()};
        ModelClient stalled(std::make_shared<ModelProcess>(stalledOptions));
        request.history=Json::array();request.prompt=std::string(2*1024*1024,'x');request.timeoutMs=300;
        auto stalledResult=stalled.request(request);
        require(stalledResult.wait_for(std::chrono::milliseconds(800))==std::future_status::ready,
                "A provider that stops reading must not block pipe writes beyond its request deadline");
        require(!stalledResult.get().passed,"A stalled provider must fail without affecting the host");
        request.prompt="generate";request.timeoutMs=5000;
        auto missingOptions=options;missingOptions.executable="missing-model-service.exe";
        ModelClient missingProcess(std::make_shared<ModelProcess>(missingOptions));
        require(!missingProcess.request(request).get().passed,"Missing executable must return a recoverable failure");
        auto exitOptions=options;exitOptions.arguments={"-c","raise SystemExit(7)"};
        ModelClient exited(std::make_shared<ModelProcess>(exitOptions));
        require(!exited.request(request).get().passed,"Process exit must return a recoverable failure");
        auto cancelledTransport=std::make_shared<ModelProcess>(stalledOptions);ModelClient cancelled(cancelledTransport);
        request.prompt=std::string(2*1024*1024,'x');auto cancelledResult=cancelled.request(request);
        cancelled.cancel(request.runId);
        require(cancelledResult.wait_for(std::chrono::milliseconds(1000))==std::future_status::ready,
                "Cancellation must release a stalled process without waiting for its deadline");
        require(cancelledResult.get().diagnostic.find("Cancelled")!=std::string::npos,"Cancelled transport must report cancellation");
#endif
        std::cout<<"Model framing, version, ordering, bounds and platform transport contract passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
