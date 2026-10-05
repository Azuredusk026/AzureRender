#pragma once
#include <cstdint>
#include <string>
#include <nlohmann/json.hpp>
namespace azurerender {
struct DocumentVersion {
    std::string documentId;
    std::uint64_t revision=0;
    std::string contentHash;
    bool operator==(const DocumentVersion& other) const {
        return documentId==other.documentId&&revision==other.revision&&contentHash==other.contentHash;
    }
    bool operator!=(const DocumentVersion& other) const { return !(*this==other); }
    nlohmann::json describe() const { return {{"documentId",documentId},{"revision",revision},{"contentHash",contentHash}}; }
};
std::string newDocumentIdentity();
std::string documentFingerprint(const nlohmann::json& content);
}
