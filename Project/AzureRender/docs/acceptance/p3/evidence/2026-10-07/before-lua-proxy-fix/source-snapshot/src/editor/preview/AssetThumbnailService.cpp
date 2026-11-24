#include "AssetThumbnailService.hpp"
#include <algorithm>
#include <stdexcept>
namespace azurerender {
AssetThumbnailService::AssetThumbnailService(RenderViewService& views,std::size_t capacity):views_(views),capacity_(capacity){
    if(!capacity || capacity>2)throw std::invalid_argument("Thumbnail cache reserves one of three view slots for cameras");
}
AssetThumbnailService::~AssetThumbnailService(){try{clear();}catch(...){}}
RenderViewHandle AssetThumbnailService::request(const ThumbnailKey& key,const RenderViewDescriptor& descriptor){
    if(key.assetId.empty() || key.fingerprint.empty() || key.settings.empty())throw std::invalid_argument("Thumbnail cache needs asset, fingerprint and settings");
    auto existing=entries_.find(key.assetId);
    if(existing!=entries_.end()){
        if(existing->second.key==key){existing->second.access=++access_;return existing->second.handle;}
        invalidate(key.assetId);
    }
    if(entries_.size()>=capacity_){
        const auto oldest=std::min_element(entries_.begin(),entries_.end(),[](const auto& a,const auto& b){return a.second.access<b.second.access;});
        invalidate(oldest->first);
    }
    const auto handle=views_.create(descriptor);views_.request(handle);
    entries_.emplace(key.assetId,Entry{key,handle,++access_});return handle;
}
void AssetThumbnailService::invalidate(const std::string& assetId){
    const auto found=entries_.find(assetId);if(found==entries_.end())return;
    views_.release(found->second.handle);entries_.erase(found);
}
void AssetThumbnailService::clear(){while(!entries_.empty())invalidate(entries_.begin()->first);}
nlohmann::json AssetThumbnailService::describe() const{
    auto result=nlohmann::json::array();
    for(const auto& [asset,entry]:entries_)result.push_back({{"asset",asset},{"fingerprint",entry.key.fingerprint},
        {"settings",entry.key.settings},{"handle",entry.handle},{"view",views_.describe(entry.handle)}});
    return result;
}
}
