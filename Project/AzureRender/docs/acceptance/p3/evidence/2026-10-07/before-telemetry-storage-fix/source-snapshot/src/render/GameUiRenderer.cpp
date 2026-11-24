#include "render/GameUiRenderer.hpp"
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
    struct Geometry {rhi::GpuBuffer vertices,indices;std::uint32_t count=0;std::uint64_t retire=0;};
    struct Texture {rhi::GpuImage image;VkImageView view=VK_NULL_HANDLE;VkDescriptorSet set=VK_NULL_HANDLE;std::uint64_t retire=0;};
    std::map<std::uintptr_t,Geometry> geometry;
    std::map<std::uintptr_t,Texture> textures;
    std::vector<VkDescriptorSet> spareSets;
    struct Push {std::array<float,16> matrix;std::array<float,2> translation;std::array<float,2> extent;};
    struct Draw {std::uintptr_t geometry,texture;Push push;VkRect2D clip;};
    std::vector<Draw> draws;
    std::array<float,16> transform{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    VkExtent2D extent{640,360};VkRect2D scissor{{0,0},{640,360}};
    bool clipping=false;
    std::uint64_t serial=0,totalDraws=0;
    std::uintptr_t next=1,white=0;
    explicit Impl(rhi::IRhi& backend):rhi(backend){}
    void destroy(Geometry& value){rhi.allocator().destroyBuffer(value.vertices);rhi.allocator().destroyBuffer(value.indices);}
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
    Impl::Geometry value;auto& allocator=impl_->rhi.allocator();
    value.vertices=allocator.createBuffer(vertices.size()*sizeof(Rml::Vertex),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,true);
    try{value.indices=allocator.createBuffer(indices.size()*sizeof(int),VK_BUFFER_USAGE_INDEX_BUFFER_BIT,true);
        std::memcpy(value.vertices.mapped,vertices.data(),static_cast<std::size_t>(value.vertices.size));std::memcpy(value.indices.mapped,indices.data(),static_cast<std::size_t>(value.indices.size));
        allocator.flush(value.vertices,0,value.vertices.size);allocator.flush(value.indices,0,value.indices.size);value.count=static_cast<std::uint32_t>(indices.size());
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
Rml::TextureHandle GameUiRenderer::LoadTexture(Rml::Vector2i& dimensions,const Rml::String& source){int width=0,height=0,channels=0;auto* bytes=stbi_load(source.c_str(),&width,&height,&channels,4);if(!bytes)return 0;
    for(std::size_t i=0;i<static_cast<std::size_t>(width)*height;++i)for(unsigned c=0;c<3;++c)bytes[i*4+c]=static_cast<unsigned char>(static_cast<unsigned>(bytes[i*4+c])*bytes[i*4+3]/255);
    try{const auto result=GenerateTexture({bytes,static_cast<std::size_t>(width)*height*4},{width,height});stbi_image_free(bytes);dimensions={width,height};return result;}catch(...){stbi_image_free(bytes);throw;}
}
void GameUiRenderer::ReleaseTexture(Rml::TextureHandle texture){auto found=impl_->textures.find(texture);if(found!=impl_->textures.end())found->second.retire=impl_->serial+3;}
void GameUiRenderer::EnableScissorRegion(bool enable){impl_->clipping=enable;}
void GameUiRenderer::SetScissorRegion(Rml::Rectanglei region){impl_->scissor={{region.Left(),region.Top()},{static_cast<std::uint32_t>(std::max(region.Width(),0)),static_cast<std::uint32_t>(std::max(region.Height(),0))}};}
void GameUiRenderer::SetTransform(const Rml::Matrix4f* transform){if(transform)std::copy_n(transform->data(),16,impl_->transform.begin());else impl_->transform={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
void GameUiRenderer::record(rhi::ICommandRecorder& commands){commands.setViewport(static_cast<float>(impl_->extent.width),static_cast<float>(impl_->extent.height));commands.bindPipeline(impl_->pipeline);
    for(const auto& draw:impl_->draws){const auto& mesh=impl_->geometry.at(draw.geometry);const auto& texture=impl_->textures.at(draw.texture);
        commands.setScissor(draw.clip.extent,draw.clip.offset);commands.bindDescriptorSet(impl_->layout,texture.set);commands.bindVertexBuffer(mesh.vertices.buffer,0);commands.bindIndexBuffer(mesh.indices.buffer,0);
        commands.pushConstants(impl_->layout,VK_SHADER_STAGE_VERTEX_BIT,0,&draw.push,sizeof(draw.push));commands.drawIndexed(mesh.count,0);++impl_->totalDraws;}
    commands.setScissor(impl_->extent);
}
std::uint64_t GameUiRenderer::drawCalls()const{return impl_->totalDraws;}
}
