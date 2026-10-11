#include "assets/AssetExportJob.hpp"
#include "assets/model/ModelExportScene.hpp"
#include "platform/NativeArguments.hpp"
#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <thread>
namespace {
volatile std::sig_atomic_t interrupted=0;
void interrupt(int){interrupted=1;}
}
int main(int argc,char** argv){
    using namespace azurerender;
    try{
        const auto args=nativeArguments(argc,argv);
        if(args.size()<3||(args[1]!="plan"&&args[1]!="export"))throw std::invalid_argument("Usage: AzureAssetExchange plan|export request.json [--replace] [--deadline-ms N]");
        bool replace=false;std::int64_t deadlineMs=300000;
        for(std::size_t i=3;i<args.size();++i){if(args[i]=="--replace")replace=true;
            else if(args[i]=="--deadline-ms"&&i+1<args.size()){std::size_t consumed=0;deadlineMs=std::stoll(args[++i],&consumed);if(consumed!=args[i].size()||deadlineMs<0||deadlineMs>3600000)throw std::invalid_argument("Deadline must be within [0,3600000] ms");}
            else throw std::invalid_argument("Unknown exchange option");}
        const auto input=std::filesystem::absolute(std::filesystem::u8path(args[2]));
        if(std::filesystem::file_size(input)>1024*1024)throw std::invalid_argument("Exchange request exceeds 1 MiB");
        std::ifstream file(input);nlohmann::json document;file>>document;
        if(!document.is_object())throw std::invalid_argument("Exchange request must be an object");
        for(auto it=document.begin();it!=document.end();++it)if(it.key()!="source"&&it.key()!="format"&&it.key()!="destination"&&it.key()!="settings"&&it.key()!="settingsVersion"&&it.key()!="maxSourceBytes"&&it.key()!="scene")throw std::invalid_argument("Unknown exchange request field: "+it.key());
        const auto path=[&](const std::string& value){auto result=std::filesystem::u8path(value);return (result.is_absolute()?result:input.parent_path()/result).lexically_normal();};
        AssetTransferRequest request;request.source=path(document.at("source").get<std::string>());request.destination=path(document.at("destination").get<std::string>());
        request.formatId=document.at("format").get<std::string>();request.settings=document.value("settings",nlohmann::json::object());request.settingsVersion=document.value("settingsVersion",1U);request.maxSourceBytes=document.value("maxSourceBytes",request.maxSourceBytes);
        request.staging=request.destination.parent_path()/(".azure-export-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        if(document.contains("scene")){auto scene=std::make_shared<ModelExportScene>();
            for(const auto& value:document.at("scene")){ModelExportNode node;node.id=value.at("id").get<std::string>();node.parent=value.value("parent",std::string());const auto source=value.value("source",std::string());if(!source.empty())node.source=path(source);if(value.contains("transform"))node.transform=value.at("transform").get<decltype(node.transform)>();scene->nodes.push_back(std::move(node));}request.scene=std::move(scene);}
        std::signal(SIGINT,interrupt);const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(deadlineMs);
        const auto cancel=[&]{return interrupted||std::chrono::steady_clock::now()>=deadline;};
        if(args[1]=="plan"){request.operation=AssetTransferOperation::ExportPlan;const auto plan=planAssetExport(request,assetFormatRegistry(),cancel);std::cout<<plan.summary.dump()<<'\n';return 0;}
        if(cancel())throw std::runtime_error("Exchange deadline reached");
        AssetExportJob job(request);while(!job.ready()){if(cancel())job.cancel();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        if(cancel())job.cancel();const auto output=job.finish(replace);auto result=job.report().summary;result["output"]=output.u8string();result["entry"]=job.report().entry.u8string();std::cout<<result.dump()<<'\n';return 0;
    }catch(const std::exception& error){std::cout<<nlohmann::json{{"error",error.what()}}.dump()<<'\n';return 1;}
}
