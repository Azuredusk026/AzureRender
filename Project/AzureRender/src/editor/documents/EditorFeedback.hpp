#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
namespace azurerender {
class EditorFeedback {
public:
    std::string record(std::string source,std::string message);
    void dismiss(const std::string& id={});
    const std::string& latestError() const{return latest_;}
    nlohmann::json report() const;
private:
    struct Entry { std::string id,source,message; };
    std::vector<Entry> entries_;
    std::string latest_;
    std::uint64_t sequence_=0;
};
}
