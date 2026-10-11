#pragma once
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace azurerender {
enum class AssetDomain { Model, Image, Audio };
struct AssetTransferRequest;
struct AssetTransferReport;
using AssetCancellation = std::function<bool()>;
struct AssetFormatDescriptor {
    std::string id, label;
    AssetDomain domain=AssetDomain::Model;
    std::vector<std::string> extensions, features;
    unsigned settingsVersion=1, generatorVersion=1;
    bool canRead=false, canWrite=false;
    std::function<bool(const std::vector<unsigned char>&)> probe;
    std::function<AssetTransferReport(const AssetTransferRequest&,const AssetCancellation&)> prepare;
};
class AssetFormatRegistry {
public:
    void add(AssetFormatDescriptor descriptor);
    const AssetFormatDescriptor& require(const std::string& id) const;
    std::vector<AssetFormatDescriptor> list(AssetDomain domain) const;
    const AssetFormatDescriptor& detect(const std::filesystem::path& path) const;
    const std::map<std::string,AssetFormatDescriptor>& formats() const noexcept { return formats_; }
private:
    std::map<std::string,AssetFormatDescriptor> formats_;
};
std::string assetExtension(const std::filesystem::path& path);
const AssetFormatRegistry& assetFormatRegistry();
}
