#pragma once
#include "editor/EditorContext.hpp"
#include <chrono>
#include <atomic>
#include <future>
#include <memory>
namespace azurerender {
struct ContentDirectorySnapshot {
    std::uint64_t version=0;
    std::string mount,path;
    nlohmann::json assets=nlohmann::json::array(),directories=nlohmann::json::array(),diagnostics=nlohmann::json::array();
};
class ContentDirectoryModel {
public:
    ~ContentDirectoryModel(){cancelRefresh();if(reclamation_.valid())reclamation_.get();}
    std::shared_ptr<const ContentDirectorySnapshot> snapshot(EditorContext&,const std::string& mount={},const std::string& path={},const std::string& type={},const std::string& search={});
    void refresh(EditorContext&);
    void poll(EditorContext&);
    nlohmann::json describe(const ContentDirectorySnapshot&) const;
    nlohmann::json statistics() const;
    const nlohmann::json& tree() const{return directories_;}
    void reset();
    void cancelRefresh();
private:
    void rebuild(EditorContext&,bool directories=true);
    void retireCatalog();
    const EditorContext* owner_=nullptr;
    const AssetDatabase* databaseOwner_=nullptr;
    std::uint64_t databaseVersion_=0;
    std::string sceneSignature_;
    std::uint64_t revision_=0,version_=0,catalogBuilds_=0,directoryEnumerations_=0;
    nlohmann::json catalog_=nlohmann::json::array(),directories_=nlohmann::json::array(),diagnostics_=nlohmann::json::array();
    std::map<std::filesystem::path,std::filesystem::file_time_type> stamps_;
    std::map<std::string,std::shared_ptr<const ContentDirectorySnapshot>> snapshots_;
    std::chrono::steady_clock::time_point nextPoll_{};
    std::future<AssetDatabase> pending_;
    std::future<void> reclamation_;
    std::shared_ptr<std::atomic_bool> cancelled_;
    std::uint64_t pendingRevision_=0;
};
}
