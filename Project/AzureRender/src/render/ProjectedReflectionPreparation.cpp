#include "render/ProjectedReflectionPreparation.hpp"
#include "render/ComputePass.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace azurerender {
namespace {
constexpr std::array<VkFormat,4> formats{{VK_FORMAT_R32_SFLOAT,VK_FORMAT_R32G32B32A32_SFLOAT,VK_FORMAT_R8_UNORM,VK_FORMAT_R16G16_UNORM}};
constexpr auto usage=VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
void require(bool value,const char* message){if(!value)throw std::invalid_argument(message);}
}
ProjectedReflectionPreparationFrame ProjectedReflectionPreparationParameters::frame(VkExtent2D source,VkExtent2D trace,const std::array<float,2>& depth) const{
    const bool same=source.width==trace.width&&source.height==trace.height;
    const bool half=std::max(1U,source.width/2)==trace.width&&std::max(1U,source.height/2)==trace.height;
    require(source.width&&source.height&&source.width<=8192&&source.height<=8192&&trace.width&&trace.height&&(same||half),"Preparation requires matching or floor-half extents within 1..8192");
    require(std::isfinite(depth[0])&&std::isfinite(depth[1])&&std::isfinite(depth[0]+depth[1])&&depth[0]>0&&depth[1]>=0,"Preparation requires positive reciprocal-depth scale and nonnegative offset");
    require(std::isfinite(maxRoughness)&&maxRoughness>=.05F&&maxRoughness<=1&&std::isfinite(strength)&&strength>=0&&strength<=1,"Preparation roughness or strength outside range");
    return {{{float(trace.width),float(trace.height),float(source.width),float(source.height)},{depth[0],depth[1],maxRoughness,strength}}};
}
ProjectedReflectionPreparationGpu::ProjectedReflectionPreparationGpu(rhi::IRhi& backend,VkExtent2D source,unsigned frames,const std::vector<char>& code,unsigned divisor)
    :backend_(backend),source_(source){
    require((divisor==1||divisor==2)&&frames>=1&&frames<=8,"Preparation requires divisor one or two and one to eight slots");
    extent_={std::max(1U,source.width/divisor),std::max(1U,source.height/divisor)};
    (void)ProjectedReflectionPreparationParameters{}.frame(source_,extent_,{1,0});
    bytes_=std::uint64_t(extent_.width)*extent_.height*kBytesPerPixel*frames;
    require(bytes_<=kMaximumBytes,"Projected preparation exceeds 256 MiB");
    const unsigned columns=(extent_.width+7)/8,rows=(extent_.height+7)/8;tileCount_=columns*rows;
    require(tileCount_<=65535,"Projected preparation exceeds 65535 tiles");
    for(const auto format:formats)if(!backend.supportsStorageImage(format)||!backend.supportsSampledImage(format,false)||!(backend.imageSampleCounts(format,usage)&VK_SAMPLE_COUNT_1_BIT))
        throw std::runtime_error("Projected preparation sampled storage format unavailable");
    std::uint32_t magic=0;if(code.size()>=4)std::memcpy(&magic,code.data(),4);
    require(code.size()>=20&&code.size()<=1024*1024&&code.size()%4==0&&magic==0x07230203U,"Projected preparation requires bounded SPIR-V");
    try{
        rhi::SamplerDesc point;point.filter=VK_FILTER_NEAREST;point.mipmapLinear=false;point.maxLod=0;
        point.addressU=point.addressV=point.addressW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;point_=backend.createSampler(point);
        descriptors_=backend.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT},
            {1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT},{2,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT},
            {3,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT},{4,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT},
            {5,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
        rhi::DescriptorPoolDesc pool;pool.maxSets=frames;pool.sizes={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,frames},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,4*frames},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,frames}};
        pool_=backend.createDescriptorPool(pool);const auto sets=backend.allocateDescriptorSets(pool_,descriptors_,frames);
        layout_=backend.createPipelineLayout(descriptors_,nullptr);shader_=backend.createShaderModule(code);pipeline_=backend.createComputePipeline({shader_,layout_});
        tiles_=backend.allocator().createBuffer(tileCount_*4,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
        auto* work=static_cast<std::uint32_t*>(tiles_.mapped);for(unsigned y=0;y<rows;++y)for(unsigned x=0;x<columns;++x)work[y*columns+x]=(x<<16)|y;
        backend.allocator().flush(tiles_,0,tiles_.size);slots_.resize(frames);
        for(unsigned i=0;i<frames;++i){auto& slot=slots_[i];slot.set=sets.at(i);
            for(unsigned attachment=0;attachment<formats.size();++attachment){slot.images[attachment]=backend.allocator().createImage2D(extent_.width,extent_.height,formats[attachment],usage);
                slot.views[attachment]=backend.createImageView(slot.images[attachment].image,formats[attachment],VK_IMAGE_ASPECT_COLOR_BIT,1);}
            slot.parameters=backend.allocator().createBuffer(sizeof(ProjectedReflectionPreparationFrame),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,true);
            backend.writeDescriptorBuffer({slot.set,5,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,slot.parameters.buffer,slot.parameters.size});
        }
    }catch(...){release();throw;}
}
ProjectedReflectionPreparationGpu::~ProjectedReflectionPreparationGpu(){release();}
void ProjectedReflectionPreparationGpu::release() noexcept{
    if(pool_)backend_.destroyDescriptorPool(pool_);pool_={};
    for(auto& slot:slots_){for(unsigned i=0;i<slot.images.size();++i){if(slot.views[i])backend_.destroyImageView(slot.views[i]);backend_.allocator().destroyImage(slot.images[i]);}
        backend_.allocator().destroyBuffer(slot.parameters);}slots_.clear();backend_.allocator().destroyBuffer(tiles_);
    if(pipeline_)backend_.destroyPipeline(pipeline_);pipeline_={};if(shader_)backend_.destroyShaderModule(shader_);shader_={};
    if(layout_)backend_.destroyPipelineLayout(layout_);layout_={};if(descriptors_)backend_.destroyDescriptorSetLayout(descriptors_);descriptors_={};
    if(point_)backend_.destroySampler(point_);point_={};
}
ProjectedReflectionPrepared ProjectedReflectionPreparationGpu::output(unsigned index) const{
    const auto& slot=slots_.at(index);return {slot.images[0].image,slot.images[1].image,slot.images[2].image,slot.images[3].image,
        slot.views[0],slot.views[1],slot.views[2],slot.views[3],extent_,tiles_.buffer,tiles_.size,tileCount_};
}
void ProjectedReflectionPreparationGpu::record(rhi::ICommandRecorder& commands,unsigned index,VkImageView surface,const ProjectedReflectionEligibilityTarget& eligibility,
    const std::array<float,2>& depth,const ProjectedReflectionPreparationParameters& parameters){
    auto& slot=slots_.at(index);const auto frame=parameters.frame(source_,extent_,depth);
    require(surface&&eligibility.image&&eligibility.view&&surface!=eligibility.view&&eligibility.extent.width==extent_.width&&eligibility.extent.height==extent_.height,"Preparation requires a surface and matching R8 eligibility target");
    for(const auto& own:slots_)for(unsigned i=0;i<own.views.size();++i)
        require(surface!=own.views[i]&&eligibility.view!=own.views[i]&&eligibility.image!=own.images[i].image,"Preparation input or eligibility aliases owned images");
    std::memcpy(slot.parameters.mapped,&frame,sizeof(frame));backend_.allocator().flush(slot.parameters,0,sizeof(frame));
    backend_.writeDescriptorImage({slot.set,0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,surface,point_,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    for(unsigned i=0;i<3;++i)backend_.writeDescriptorImage({slot.set,i+1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,slot.views[i],VK_NULL_HANDLE,VK_IMAGE_LAYOUT_GENERAL});
    backend_.writeDescriptorImage({slot.set,4,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,eligibility.view,VK_NULL_HANDLE,VK_IMAGE_LAYOUT_GENERAL});
    for(const auto& image:slot.images)commands.imageBarrier({image.image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,1,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,VK_ACCESS_SHADER_WRITE_BIT});
    commands.bufferBarrier({tiles_.buffer,0,tiles_.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
    ComputePass({extent_.width,extent_.height,8,8,1,true}).record(commands,pipeline_,layout_,slot.set);
    for(unsigned i=0;i<3;++i)commands.imageBarrier({slot.images[i].image,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,1,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
    commands.imageBarrier({eligibility.image,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_GENERAL,1,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT});
}
}
