#pragma once
#include "assets/GenerationManifest.hpp"
#include <functional>
#include <vector>
namespace azurerender {
struct GenerationInput { std::string reference,bytes; };
struct GenerationRequest {
    nlohmann::json parameters=nlohmann::json::object();
    std::vector<GenerationInput> inputs,dependencies;
    std::string outputReference,licenseSource;
    std::function<void()> check;
};
struct GenerationResult { std::string bytes,fingerprint;GenerationManifest manifest;nlohmann::json diagnostics=nlohmann::json::array(); };
class GeneratorRegistry {
public:
    using Factory=std::function<std::string(const GenerationRequest&)>;
    void add(std::string id,unsigned version,Factory factory);
    GenerationResult generate(const std::string& id,const GenerationRequest& request) const;
    nlohmann::json describe() const;
    static GeneratorRegistry builtins();
private:
    struct Entry { unsigned version;Factory factory; };
    std::map<std::string,Entry> entries_;
};
}
