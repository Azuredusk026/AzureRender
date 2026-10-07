#pragma once
#include "editor/commands/DocumentVersion.hpp"
#include <stdexcept>
#include <vector>
namespace azurerender {
enum class EditStatus { Applied, Rejected, Stale, Failed };
struct EditRequest {
    std::string requestId,commandId;
    nlohmann::json parameters=nlohmann::json::object();
    DocumentVersion baseVersion;
    std::string mergeKey;
};
struct EditResult {
    EditStatus status=EditStatus::Rejected;
    DocumentVersion version;
    nlohmann::json diff=nlohmann::json::array();
    nlohmann::json diagnostics=nlohmann::json::array();
    nlohmann::json value=nullptr;
    explicit operator bool() const { return status==EditStatus::Applied; }
};
struct EditDescriptor {
    std::string id;
    unsigned version=1;
    nlohmann::json parameters=nlohmann::json::object();
    bool modifiesDocument=false,transactional=false,requiresIdle=false;
};
class EditRejection:public std::runtime_error { public:using std::runtime_error::runtime_error; };
}
