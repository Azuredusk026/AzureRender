#include "assets/AssetFormatRegistry.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>

namespace azurerender {
std::string assetExtension(const std::filesystem::path& path) {
    auto value=path.extension().u8string();
    std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    return value;
}
void AssetFormatRegistry::add(AssetFormatDescriptor descriptor) {
    if(descriptor.id.empty()||descriptor.label.empty()||descriptor.extensions.empty()||
       descriptor.settingsVersion==0||descriptor.generatorVersion==0||(!descriptor.canRead&&!descriptor.canWrite))
        throw std::invalid_argument("Invalid asset format descriptor");
    if(descriptor.canRead&&(!descriptor.probe||!descriptor.prepare))
        throw std::invalid_argument("Readable formats require content probing and preparation");
    if(descriptor.canWrite&&(!descriptor.prepare||!descriptor.plan||!descriptor.normalizeExportSettings))throw std::invalid_argument("Writable formats require preparation, planning and settings validation");
    for(auto& extension:descriptor.extensions) {
        if(extension.size()<2||extension.front()!='.'||extension.find_first_of("/\\ \t")!=std::string::npos)
            throw std::invalid_argument("Invalid asset format extension");
        extension=assetExtension(std::filesystem::u8path("asset"+extension));
    }
    std::sort(descriptor.extensions.begin(),descriptor.extensions.end());
    if(std::adjacent_find(descriptor.extensions.begin(),descriptor.extensions.end())!=descriptor.extensions.end())
        throw std::invalid_argument("Duplicate format extension");
    const auto id=descriptor.id;
    if(!formats_.emplace(id,std::move(descriptor)).second)throw std::invalid_argument("Duplicate asset format: "+id);
}
const AssetFormatDescriptor& AssetFormatRegistry::require(const std::string& id) const {
    const auto found=formats_.find(id);
    if(found==formats_.end())throw std::invalid_argument("Unknown asset format: "+id);
    return found->second;
}
std::vector<AssetFormatDescriptor> AssetFormatRegistry::list(AssetDomain domain) const {
    std::vector<AssetFormatDescriptor> result;
    for(const auto& pair:formats_)if(pair.second.domain==domain)result.push_back(pair.second);
    return result;
}
const AssetFormatDescriptor& AssetFormatRegistry::detect(const std::filesystem::path& path) const {
    std::ifstream stream(path,std::ios::binary);
    if(!stream)throw std::runtime_error("Asset source is unavailable: "+path.u8string());
    std::vector<unsigned char> bytes(65536);stream.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    bytes.resize(static_cast<std::size_t>(stream.gcount()));
    if(stream.bad())throw std::runtime_error("Asset content probe failed");
    const auto extension=assetExtension(path);const AssetFormatDescriptor* match=nullptr;
    for(const auto& pair:formats_) {
        const auto& format=pair.second;
        if(!format.canRead||std::find(format.extensions.begin(),format.extensions.end(),extension)==format.extensions.end()||!format.probe(bytes))continue;
        if(match)throw std::invalid_argument("Ambiguous asset format content");
        match=&format;
    }
    if(!match)throw std::invalid_argument("Asset extension and content do not match a supported format");
    return *match;
}
}
