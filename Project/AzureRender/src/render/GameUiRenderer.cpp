#include "render/GameUiRenderer.hpp"
#include "assets/image/ImageCodec.hpp"
#include <RmlUi/Core/Matrix4.h>
#include <stb_image.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <map>
#include <stdexcept>
namespace azurerender {
struct GameUiRenderer::Impl {
    rhi::IRhi& rhi;
    VkDescriptorSetLayout setLayout=VK_NULL_HANDLE;
    VkDescriptorPool pool=VK_NULL_HANDLE;
    VkPipelineLayout layout=VK_NULL_HANDLE;
    VkPipeline pipeline=VK_NULL_HANDLE;
    VkSampler sampler=VK_NULL_HANDLE;
    struct Range {VkDeviceSize offset=0,size=0;};
    struct Page {rhi::GpuBuffer buffer;std::vector<Range> free;std::size_t references=0;};
    struct Geometry {std::uintptr_t page=0;Range range;VkDeviceSize indexOffset=0;std::uint32_t count=0;std::uint64_t retire=0;};
    struct Texture {rhi::GpuImage image;VkImageView view=VK_NULL_HANDLE;VkDescriptorSet set=VK_NULL_HANDLE;std::uint64_t retire=0;};
    std::map<std::uintptr_t,Geometry> geometry;
    std::map<std::uintptr_t,Page> pages;
    std::map<std::uintptr_t,Texture> textures;
    std::vector<VkDescriptorSet> spareSets;
    struct Push {std::array<float,16> matrix;std::array<float,2> translation;std::array<float,2> extent;};
    struct Draw {std::uintptr_t geometry,texture;Push push;VkRect2D clip;};
    std::vector<Draw> draws;
    std::array<float,16> transform{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    VkExtent2D extent{640,360};VkRect2D scissor{{0,0},{640,360}};
    bool clipping=false;
    std::uint64_t serial=0,totalDraws=0;
    std::uintptr_t next=1,nextPage=1,white=0;
    explicit Impl(rhi::IRhi& backend):rhi(backend){}
    Geometry allocateGeometry(VkDeviceSize bytes,VkDeviceSize vertexBytes){
        static_assert(sizeof(int)==4 && sizeof(Rml::Vertex)%4==0,"UI indices require four-byte aligned storage");
        auto page=pages.end();std::size_t slot=0;
        for(auto candidate=pages.begin();candidate!=pages.end();++candidate){
            const auto found=std::find_if(candidate->second.free.begin(),candidate->second.free.end(),
                [bytes](const Range& range){return range.size>=bytes;});
            if(found!=candidate->second.free.end()){
                page=candidate;slot=static_cast<std::size_t>(found-candidate->second.free.begin());break;
            }
        }
        if(page==pages.end()){
            VkDeviceSize capacity=64*1024;while(capacity<bytes)capacity*=2;
            Page value;value.free.reserve(16);value.free.push_back({0,capacity});
            value.buffer=rhi.allocator().createBuffer(capacity,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_INDEX_BUFFER_BIT,true);
            try{page=pages.emplace(nextPage++,std::move(value)).first;}
            catch(...){rhi.allocator().destroyBuffer(value.buffer);throw;}
        }
        auto& value=page->second;
        // Reserve free-list storage before taking a range. Retirement and
        // exception rollback can then return ranges without allocating.
        const auto required=value.free.size()+value.references+1;
        if(value.free.capacity()<required)value.free.reserve(std::max(required,value.free.capacity()*2));
        Geometry result;result.page=page->first;result.range={value.free[slot].offset,bytes};
        result.indexOffset=result.range.offset+vertexBytes;
        value.free[slot].offset+=bytes;value.free[slot].size-=bytes;
        if(!value.free[slot].size)value.free.erase(value.free.begin()+static_cast<std::ptrdiff_t>(slot));
        ++value.references;return result;
    }
    void destroy(Geometry& value){
        if(!value.page)return;
        auto page=pages.find(value.page);auto& storage=page->second;
        if(--storage.references==0){rhi.allocator().destroyBuffer(storage.buffer);pages.erase(page);}
        else{
            storage.free.push_back(value.range);
            std::sort(storage.free.begin(),storage.free.end(),[](const Range& a,const Range& b){return a.offset<b.offset;});
            std::size_t count=0;
            for(const auto& range:storage.free){
                if(count && storage.free[count-1].offset+storage.free[count-1].size==range.offset)storage.free[count-1].size+=range.size;
                else storage.free[count++]=range;
            }
            storage.free.resize(count);
        }
        value.page=0;
    }
    void destroy(Texture& value){if(value.view)rhi.destroyImageView(value.view);rhi.allocator().destroyImage(value.image);if(value.set)spareSets.push_back(value.set);}
    ~Impl(){for(auto& item:geometry)destroy(item.second);for(auto& item:textures)destroy(item.second);if(pipeline)rhi.destroyPipeline(pipeline);if(layout)rhi.destroyPipelineLayout(layout);if(pool)rhi.destroyDescriptorPool(pool);if(setLayout)rhi.destroyDescriptorSetLayout(setLayout);if(sampler)rhi.destroySampler(sampler);}
    void initialize(VkRenderPass pass,const std::filesystem::path& shaders){
        setLayout=rhi.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT}});
        pool=rhi.createDescriptorPool({{{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,512}},512});
        rhi::PushConstantRangeDesc push{VK_SHADER_STAGE_VERTEX_BIT,sizeof(Push)};layout=rhi.createPipelineLayout(setLayout,&push);
        rhi::SamplerDesc sampling;sampling.addressU=sampling.addressV=sampling.addressW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;sampler=rhi.createSampler(sampling);
        const auto shader=[&](const char* file){std::ifstream input(shaders/file,std::ios::binary);if(!input)throw std::runtime_error("Game UI shader missing");return rhi.createShaderModule({std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()});};
        const auto vertex=shader("game-ui.vert.spv");VkShaderModule fragment=VK_NULL_HANDLE;
        try{
            fragment=shader("game-ui.frag.spv");rhi::GraphicsPipelineDesc desc;desc.vertexShader=vertex;desc.fragmentShader=fragment;desc.vertexStride=sizeof(Rml::Vertex);
            desc.vertexAttributes={{0,VK_FORMAT_R32G32_SFLOAT,offsetof(Rml::Vertex,position)},{1,VK_FORMAT_R8G8B8A8_UNORM,offsetof(Rml::Vertex,colour)},{2,VK_FORMAT_R32G32_SFLOAT,offsetof(Rml::Vertex,tex_coord)}};
            desc.depthTest=false;desc.depthWrite=false;desc.cullMode=VK_CULL_MODE_NONE;desc.alphaBlend=true;desc.premultipliedAlpha=true;desc.colorAttachmentCount=1;desc.renderPass=pass;desc.layout=layout;pipeline=rhi.createGraphicsPipeline(desc);
        }catch(...){rhi.destroyShaderModule(vertex);if(fragment)rhi.destroyShaderModule(fragment);throw;}
        rhi.destroyShaderModule(vertex);rhi.destroyShaderModule(fragment);
    }
};
GameUiRenderer::GameUiRenderer(rhi::IRhi& rhi,VkRenderPass pass,const std::filesystem::path& shaders):impl_(std::make_unique<Impl>(rhi)){
    impl_->initialize(pass,shaders);const Rml::byte white[4]={255,255,255,255};impl_->white=GenerateTexture({white,4},{1,1});
}
GameUiRenderer::~GameUiRenderer()=default;
void GameUiRenderer::beginFrame(std::uint64_t serial,VkExtent2D extent){
    if(!extent.width||!extent.height||serial<impl_->serial)throw std::invalid_argument("Invalid game UI frame");
    impl_->serial=serial;impl_->extent=extent;impl_->draws.clear();impl_->clipping=false;
    for(auto it=impl_->geometry.begin();it!=impl_->geometry.end();)if(it->second.retire&&it->second.retire<=serial){impl_->destroy(it->second);it=impl_->geometry.erase(it);}else ++it;
    for(auto it=impl_->textures.begin();it!=impl_->textures.end();)if(it->second.retire&&it->second.retire<=serial){impl_->destroy(it->second);it=impl_->textures.erase(it);}else ++it;
}
Rml::CompiledGeometryHandle GameUiRenderer::CompileGeometry(Rml::Span<const Rml::Vertex> vertices,Rml::Span<const int> indices){
    if(vertices.empty()||indices.empty())return 0;if(vertices.size()>1000000||indices.size()>3000000)throw std::invalid_argument("Game UI geometry exceeds limit");
    for(auto index:indices)if(index<0||static_cast<std::size_t>(index)>=vertices.size())throw std::invalid_argument("Game UI index out of bounds");
    const auto vertexBytes=vertices.size()*sizeof(Rml::Vertex),indexBytes=indices.size()*sizeof(int);
    auto value=impl_->allocateGeometry(vertexBytes+indexBytes,vertexBytes);auto& allocator=impl_->rhi.allocator();
    try{const auto& buffer=impl_->pages.at(value.page).buffer;auto* data=static_cast<std::byte*>(buffer.mapped);
        std::memcpy(data+value.range.offset,vertices.data(),vertexBytes);std::memcpy(data+value.indexOffset,indices.data(),indexBytes);
        allocator.flush(buffer,value.range.offset,value.range.size);value.count=static_cast<std::uint32_t>(indices.size());
        const auto handle=impl_->next++;impl_->geometry.emplace(handle,value);return handle;
    }catch(...){impl_->destroy(value);throw;}
}
void GameUiRenderer::RenderGeometry(Rml::CompiledGeometryHandle geometry,Rml::Vector2f translation,Rml::TextureHandle texture){
    const auto handle=texture?texture:impl_->white;const auto& mesh=impl_->geometry.at(geometry);const auto& image=impl_->textures.at(handle);
    if(mesh.retire||image.retire)throw std::logic_error("Game UI handle is retired");
    VkRect2D clip{{0,0},impl_->extent};if(impl_->clipping){const auto x=std::clamp(impl_->scissor.offset.x,0,static_cast<int>(impl_->extent.width));const auto y=std::clamp(impl_->scissor.offset.y,0,static_cast<int>(impl_->extent.height));
        const auto right=std::clamp(static_cast<std::int64_t>(impl_->scissor.offset.x)+impl_->scissor.extent.width,std::int64_t{0},static_cast<std::int64_t>(impl_->extent.width));
        const auto bottom=std::clamp(static_cast<std::int64_t>(impl_->scissor.offset.y)+impl_->scissor.extent.height,std::int64_t{0},static_cast<std::int64_t>(impl_->extent.height));
        clip={{x,y},{static_cast<std::uint32_t>(std::max(right-x,std::int64_t{0})),static_cast<std::uint32_t>(std::max(bottom-y,std::int64_t{0}))}};}
    if(!clip.extent.width||!clip.extent.height)return;
    impl_->draws.push_back({geometry,handle,{impl_->transform,{translation.x,translation.y},{static_cast<float>(impl_->extent.width),static_cast<float>(impl_->extent.height)}},clip});
}
void GameUiRenderer::ReleaseGeometry(Rml::CompiledGeometryHandle geometry){auto found=impl_->geometry.find(geometry);if(found!=impl_->geometry.end())found->second.retire=impl_->serial+3;}
Rml::TextureHandle GameUiRenderer::GenerateTexture(Rml::Span<const Rml::byte> source,Rml::Vector2i dimensions){
    if(dimensions.x<=0||dimensions.y<=0||dimensions.x>8192||dimensions.y>8192||source.size()!=static_cast<std::size_t>(dimensions.x)*dimensions.y*4)throw std::invalid_argument("Invalid game UI texture");
    Impl::Texture value;auto& allocator=impl_->rhi.allocator();auto staging=allocator.createBuffer(source.size(),VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true);
    try{
        std::memcpy(staging.mapped,source.data(),source.size());allocator.flush(staging,0,source.size());
        value.image=allocator.createImage2D(static_cast<std::uint32_t>(dimensions.x),static_cast<std::uint32_t>(dimensions.y),VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT);
        impl_->rhi.transitionImageLayout(value.image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1);
        impl_->rhi.copyBufferToImage(staging,value.image,static_cast<std::uint32_t>(dimensions.x),static_cast<std::uint32_t>(dimensions.y));
        impl_->rhi.transitionImageLayout(value.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,1);
        value.view=impl_->rhi.createImageView(value.image.image,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_ASPECT_COLOR_BIT,1);
        if(impl_->spareSets.empty())value.set=impl_->rhi.allocateDescriptorSets(impl_->pool,impl_->setLayout,1).front();else{value.set=impl_->spareSets.back();impl_->spareSets.pop_back();}
        impl_->rhi.writeDescriptorImage({value.set,0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,value.view,impl_->sampler,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
        allocator.destroyBuffer(staging);const auto handle=impl_->next++;impl_->textures.emplace(handle,value);return handle;
    }catch(...){allocator.destroyBuffer(staging);impl_->destroy(value);throw;}
}
Rml::TextureHandle GameUiRenderer::LoadTexture(Rml::Vector2i& dimensions,const Rml::String& source){
    DecodedImage image;ImageRgba8 converted;
    try{image=decodeImageFile(std::filesystem::u8path(source));converted=convertImageToRgba8(image);}catch(const std::exception&){return 0;}
    auto& bytes=converted.pixels;
    for(std::size_t i=0;i<bytes.size();i+=4)for(unsigned c=0;c<3;++c)bytes[i+c]=static_cast<unsigned char>(static_cast<unsigned>(bytes[i+c])*bytes[i+3]/255);
    dimensions={static_cast<int>(image.width),static_cast<int>(image.height)};
    return GenerateTexture({bytes.data(),bytes.size()},dimensions);
}
void GameUiRenderer::ReleaseTexture(Rml::TextureHandle texture){auto found=impl_->textures.find(texture);if(found!=impl_->textures.end())found->second.retire=impl_->serial+3;}
void GameUiRenderer::EnableScissorRegion(bool enable){impl_->clipping=enable;}
void GameUiRenderer::SetScissorRegion(Rml::Rectanglei region){impl_->scissor={{region.Left(),region.Top()},{static_cast<std::uint32_t>(std::max(region.Width(),0)),static_cast<std::uint32_t>(std::max(region.Height(),0))}};}
void GameUiRenderer::SetTransform(const Rml::Matrix4f* transform){if(transform)std::copy_n(transform->data(),16,impl_->transform.begin());else impl_->transform={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
void GameUiRenderer::record(rhi::ICommandRecorder& commands){commands.setViewport(static_cast<float>(impl_->extent.width),static_cast<float>(impl_->extent.height));commands.bindPipeline(impl_->pipeline);
    for(const auto& draw:impl_->draws){const auto& mesh=impl_->geometry.at(draw.geometry);const auto& texture=impl_->textures.at(draw.texture);
        const auto buffer=impl_->pages.at(mesh.page).buffer.buffer;
        commands.setScissor(draw.clip.extent,draw.clip.offset);commands.bindDescriptorSet(impl_->layout,texture.set);commands.bindVertexBuffer(buffer,mesh.range.offset);commands.bindIndexBuffer(buffer,mesh.indexOffset);
        commands.pushConstants(impl_->layout,VK_SHADER_STAGE_VERTEX_BIT,0,&draw.push,sizeof(draw.push));commands.drawIndexed(mesh.count,0);++impl_->totalDraws;}
    commands.setScissor(impl_->extent);
}
std::uint64_t GameUiRenderer::drawCalls()const{return impl_->totalDraws;}
GameUiGeometryStatistics GameUiRenderer::statistics()const{
    GameUiGeometryStatistics result;
    for(const auto& item:impl_->geometry){
        const auto& value=item.second;
        if(value.retire)++result.retiredGeometry;else ++result.activeGeometry;
    }
    result.buffers=impl_->pages.size();
    for(const auto& item:impl_->pages)result.bufferBytes+=item.second.buffer.size;
    return result;
}
}
