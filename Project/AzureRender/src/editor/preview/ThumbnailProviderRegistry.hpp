#pragma once
#include "editor/preview/AssetThumbnailService.hpp"
#include "runtime/AssetDatabase.hpp"
namespace azurerender {
struct ThumbnailDescription {ThumbnailKey key;RenderViewDescriptor view;};
class ThumbnailProviderRegistry {
public:
    using Provider=std::function<ThumbnailDescription(const AssetRecord&,const AssetDatabase&,const std::filesystem::path&,const RenderViewDescriptor&)>;
    void add(std::string type,Provider);
    ThumbnailDescription describe(const AssetRecord&,const AssetDatabase&,const std::filesystem::path&,const RenderViewDescriptor&)const;
    nlohmann::json types()const;
    static ThumbnailProviderRegistry& builtins();
private:
    std::map<std::string,Provider> providers_;
};
}
