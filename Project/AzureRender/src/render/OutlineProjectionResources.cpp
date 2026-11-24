#include "OutlineProjectionResources.hpp"
#include <cstring>

namespace azurerender {
void OutlineProjectionResources::initialize(const OutlineProjectionData& data,SampledTextureCache& cache) {
    if(buffer_.buffer)throw std::logic_error("Retire and reset outline resources before initialization");
    const auto bytes=data.byteSize();
    if(data.header[0]!=PackedOutlineProjectionRecord::kLayoutVersion||data.header[1]!=data.records.size()||data.records.empty()
        ||data.header[2]!=4+data.records.size()*16||data.header[3]!=data.texcoord1.size()
        ||data.masks.size()!=data.records.size()||(maximumBytes_&&bytes>maximumBytes_))
        throw std::invalid_argument("Invalid outline projection table or device storage budget");
    for(const auto& record:data.records) {
        if(record.indices[0]>2)throw std::invalid_argument("Unknown outline direction encoding");
        if(record.parameters[3]>.5F&&record.indices[0]!=0) {
            if(record.indices[2]<data.header[2]||(record.indices[2]-data.header[2])%2)
                throw std::invalid_argument("Invalid outline UV1 address");
            const auto first=(record.indices[2]-data.header[2])/2;
            if(first>data.texcoord1.size()||record.indices[3]>data.texcoord1.size()-first)
                throw std::invalid_argument("Outline UV1 range exceeds the uploaded stream");
        }
    }
    SampledTextureUpload upload;upload.width=upload.height=1;upload.mipLevels=1;
    upload.format=VK_FORMAT_R8G8B8A8_UNORM;upload.semantic=TextureFilterSemantic::Linear;
    upload.sampler.filter=VK_FILTER_LINEAR;upload.sampler.maxLod=0;
    upload.sampler.addressU=upload.sampler.addressV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    const auto fallback=cache.acquire(std::vector<std::uint8_t>{255,0,0,255},upload);
    std::vector<SampledTextureBinding> masks(data.records.size(),fallback);
    for(std::size_t index=0;index<data.masks.size();++index) {
        auto* shape=data.masks[index];
        if(!shape)throw std::invalid_argument("Outline mask upload source is missing");
        if(!shape->present)continue;
        upload.width=shape->maskWidth;upload.height=shape->maskHeight;
        masks[index]=cache.acquire(shape->maskPixels,upload);
    }
    auto buffer=allocator_.createBuffer(bytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    try {
        if(!buffer.mapped)throw std::runtime_error("Outline projection storage must be host mapped");
        auto* destination=static_cast<std::uint8_t*>(buffer.mapped);
        std::memcpy(destination,data.header.data(),sizeof(data.header));
        const auto recordBytes=data.records.size()*sizeof(PackedOutlineProjectionRecord);
        std::memcpy(destination+sizeof(data.header),data.records.data(),recordBytes);
        if(!data.texcoord1.empty())std::memcpy(destination+sizeof(data.header)+recordBytes,
            data.texcoord1.data(),data.texcoord1.size()*sizeof(std::array<float,2>));
        allocator_.flush(buffer,0,bytes);
    }catch(...){allocator_.destroyBuffer(buffer);throw;}
    buffer_=buffer;masks_=std::move(masks);recordCount_=data.header[1];coordinateCount_=data.header[3];
    for(auto* shape:data.masks)if(shape->present)std::vector<std::uint8_t>{}.swap(shape->maskPixels);
}
void OutlineProjectionResources::reset() noexcept {
    allocator_.destroyBuffer(buffer_);buffer_={};masks_.clear();recordCount_=coordinateCount_=0;
}
} // namespace azurerender
