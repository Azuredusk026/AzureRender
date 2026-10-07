#pragma once
#include <nlohmann/json.hpp>
#include <array>
#include <filesystem>
#include <map>
#include <optional>
#include <thread>
namespace azurerender {
enum class SettingSource { Default, DefaultFile, UserFile, Project, CommandLine, Console };
const char* settingSourceName(SettingSource source);
struct SettingDescriptor {
    std::string name, description;
    nlohmann::json defaultValue;
    std::optional<double> minimum, maximum;
    bool readOnly=false, persistent=false, startupOnly=false;
};
struct SettingResult { bool passed=false; std::string diagnostic; };
class SettingRegistry {
public:
    SettingRegistry()=default;
    SettingRegistry(const SettingRegistry&)=delete;
    SettingRegistry& operator=(const SettingRegistry&)=delete;
    using Json=nlohmann::json;
    void add(SettingDescriptor descriptor);
    SettingResult set(const std::string& name, Json value, SettingSource source);
    SettingResult reset(const std::string& name, SettingSource source);
    SettingResult replaceLayer(const Json& values, SettingSource source);
    void applyPending();
    void start();
    std::uint64_t revision() const {checkThread();return revision_;}
    Json get(const std::string& name) const;
    SettingSource source(const std::string& name) const;
    Json describe(const std::string& search={}) const;
    void saveUser(const std::filesystem::path& path) const;
    SettingResult load(const std::filesystem::path& path, SettingSource source);
private:
    struct Entry {
        SettingDescriptor descriptor;
        std::array<std::optional<Json>,6> layers, pending;
    };
    std::map<std::string,Entry> entries_;
    std::thread::id owner_=std::this_thread::get_id();
    bool started_=false;
    std::uint64_t revision_=0;
    void checkThread() const;
    void validate(const Entry& entry,const Json& value,SettingSource source) const;
};
}
