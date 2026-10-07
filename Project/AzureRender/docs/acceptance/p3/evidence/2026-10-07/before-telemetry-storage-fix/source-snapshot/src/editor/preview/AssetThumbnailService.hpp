#pragma once
#include "render/RenderViewService.hpp"
#include <map>
namespace azurerender {
struct ThumbnailKey {
    std::string assetId,fingerprint,settings;
    bool operator==(const ThumbnailKey& other) const{return assetId==other.assetId && fingerprint==other.fingerprint && settings==other.settings;}
};
class AssetThumbnailService final {
public:
    explicit AssetThumbnailService(RenderViewService& views,std::size_t capacity=2);
    ~AssetThumbnailService();
    RenderViewHandle request(const ThumbnailKey&,const RenderViewDescriptor&);
    void invalidate(const std::string& assetId);
    void clear();
    nlohmann::json describe() const;
private:
    struct Entry{ThumbnailKey key;RenderViewHandle handle=0;std::uint64_t access=0;};
    RenderViewService& views_;
    std::size_t capacity_;
    std::uint64_t access_=0;
    std::map<std::string,Entry> entries_;
};
}
