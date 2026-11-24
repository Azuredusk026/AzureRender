#include "support/VulkanComputeFixture.hpp"
#include "render/ShaderSharedTypes.hpp"
#include "render/ComputePass.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>

using namespace azurerender;
using test::vkRequire;
namespace {
std::vector<char> bytes(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary);
    if(!file) throw std::runtime_error("Required compiled shader is unavailable: "+path.string());
    return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
float halfFloat(std::uint16_t value) {
    const auto exponent=(value>>10)&31U,mantissa=value&1023U;
    const float magnitude=exponent==0?std::ldexp(static_cast<float>(mantissa),-24)
        :std::ldexp(1.F+static_cast<float>(mantissa)/1024.F,static_cast<int>(exponent)-15);
    return (value&0x8000U)?-magnitude:magnitude;
}
std::uint16_t half(float value) {
    // Fixture inputs are positive quarter/eighth units with exact half encoding.
    if(value==0) return 0;
    std::uint32_t bits=0;std::memcpy(&bits,&value,sizeof(bits));
    return static_cast<std::uint16_t>(((bits>>23)-127+15)<<10 | ((bits>>13)&1023));
}
class BloomProbe {
public:
    test::VulkanComputeFixture& host;
    const std::uint32_t width,height,outWidth,outHeight;
    std::vector<std::uint16_t> input;
    rhi::GpuBuffer upload{},readback{};
    rhi::GpuImage source{},destination{};
    VkImageView sourceView=VK_NULL_HANDLE,destinationView=VK_NULL_HANDLE;
    VkSampler sampler=VK_NULL_HANDLE;
    VkDescriptorSetLayout setLayout=VK_NULL_HANDLE;
    VkDescriptorPool pool=VK_NULL_HANDLE;
    VkDescriptorSet set=VK_NULL_HANDLE;
    VkPipelineLayout layout=VK_NULL_HANDLE;
    VkPipeline pipeline=VK_NULL_HANDLE;
    VkQueryPool timestamps=VK_NULL_HANDLE;
    bool rendered=false;
    double gpuMs=0,recordMs=0;

    BloomProbe(test::VulkanComputeFixture& context,std::uint32_t w,std::uint32_t h,const std::vector<char>& code)
        :host(context),width(w),height(h),outWidth((w+1)/2),outHeight((h+1)/2) {
        try {
            auto& gpu=*host.backend;
            input.resize(static_cast<std::size_t>(width)*height*4);
            for(std::size_t index=0;index<input.size()/4;++index) {
                input[index*4]=half(static_cast<float>(index%29)*.25F);
                input[index*4+1]=half(static_cast<float>(index%17)*.125F);
                input[index*4+2]=half(static_cast<float>(index%13)*.5F);input[index*4+3]=half(1);
            }
            upload=host.allocator.createBuffer(input.size()*2,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true);
            std::memcpy(upload.mapped,input.data(),input.size()*2);host.allocator.flush(upload,0,upload.size);
            readback=host.allocator.createBuffer(static_cast<VkDeviceSize>(outWidth)*outHeight*8,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
            source=host.allocator.createImage2D(width,height,VK_FORMAT_R16G16B16A16_SFLOAT,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT);
            destination=host.allocator.createImage2D(outWidth,outHeight,VK_FORMAT_R16G16B16A16_SFLOAT,VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
            gpu.transitionImageLayout(source,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1);
            gpu.copyBufferToImage(upload,source,width,height);
            gpu.transitionImageLayout(source,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,1);
            sourceView=gpu.createImageView(source.image,VK_FORMAT_R16G16B16A16_SFLOAT,VK_IMAGE_ASPECT_COLOR_BIT,1);
            destinationView=gpu.createImageView(destination.image,VK_FORMAT_R16G16B16A16_SFLOAT,VK_IMAGE_ASPECT_COLOR_BIT,1);
            sampler=gpu.createSampler({});
            setLayout=gpu.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT},
                {1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT}});
            rhi::DescriptorPoolDesc description;
            description.sizes={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1}};description.maxSets=1;
            pool=gpu.createDescriptorPool(description);set=gpu.allocateDescriptorSets(pool,setLayout,1).front();
            gpu.writeDescriptorImage({set,0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,sourceView,sampler,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
            gpu.writeDescriptorImage({set,1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,destinationView,VK_NULL_HANDLE,VK_IMAGE_LAYOUT_GENERAL});
            const rhi::PushConstantRangeDesc range{VK_SHADER_STAGE_COMPUTE_BIT,sizeof(shader::BloomParameters)};
            layout=gpu.createPipelineLayout(setLayout,&range);
            const auto module=gpu.createShaderModule(code);
            try {pipeline=gpu.createComputePipeline({module,layout});} catch(...) {gpu.destroyShaderModule(module);throw;}
            gpu.destroyShaderModule(module);
            VkQueryPoolCreateInfo query{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};query.queryType=VK_QUERY_TYPE_TIMESTAMP;query.queryCount=2;
            vkRequire(vkCreateQueryPool(host.device,&query,nullptr,&timestamps),"vkCreateQueryPool");
        } catch(...) {release();throw;}
    }
    ~BloomProbe(){release();}
    std::vector<std::uint16_t> run(shader::BloomParameters parameters) {
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer command){
            const auto start=std::chrono::steady_clock::now();
            vkCmdResetQueryPool(command,timestamps,0,2);
            commands.imageBarrier({destination.image,rendered?VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_GENERAL,1,static_cast<VkPipelineStageFlags>(rendered?VK_PIPELINE_STAGE_TRANSFER_BIT:VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT),
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,rendered?VK_ACCESS_TRANSFER_READ_BIT:0U,VK_ACCESS_SHADER_WRITE_BIT});
            commands.pushConstants(layout,VK_SHADER_STAGE_COMPUTE_BIT,0,&parameters,sizeof(parameters));
            commands.writeTimestamp(timestamps,0,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            ComputePass({outWidth,outHeight,8,8,1,true}).record(commands,pipeline,layout,set);
            commands.writeTimestamp(timestamps,1,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            commands.imageBarrier({destination.image,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,1,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT});
            commands.copyImageToBuffer(destination.image,readback.buffer,{outWidth,outHeight});
            commands.bufferBarrier({readback.buffer,0,readback.size,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
            recordMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        });
        rendered=true;
        std::array<std::uint64_t,2> time{};
        vkRequire(vkGetQueryPoolResults(host.device,timestamps,0,2,sizeof(time),time.data(),sizeof(time[0]),VK_QUERY_RESULT_64_BIT|VK_QUERY_RESULT_WAIT_BIT),"vkGetQueryPoolResults");
        const auto mask=host.timestampBits>=64?std::numeric_limits<std::uint64_t>::max():((std::uint64_t{1}<<host.timestampBits)-1);
        gpuMs=static_cast<double>((time[1]-time[0])&mask)*host.properties.limits.timestampPeriod/1000000.;
        host.allocator.invalidate(readback,0,readback.size);
        const auto* data=static_cast<const std::uint16_t*>(readback.mapped);
        return {data,data+readback.size/2};
    }
    void reference(const std::vector<std::uint16_t>& output,shader::BloomParameters parameters) const {
        for(std::uint32_t y=0;y<outHeight;++y) for(std::uint32_t x=0;x<outWidth;++x) for(std::size_t channel=0;channel<4;++channel) {
            float expected=1;
            if(channel<3) {
                expected=0;
                for(std::uint32_t dy=0;dy<2;++dy) for(std::uint32_t dx=0;dx<2;++dx) {
                    const auto sx=std::min(x*2+dx,width-1),sy=std::min(y*2+dy,height-1);
                    expected+=halfFloat(input[(static_cast<std::size_t>(sy)*width+sx)*4+channel]);
                }
                expected*=.25F;
                if(parameters.extractBright) expected=std::max(expected-std::max(parameters.threshold,0.F),0.F);
            }
            const auto actual=halfFloat(output[(static_cast<std::size_t>(y)*outWidth+x)*4+channel]);
            // RGBA16F storage conversion may round toward zero or to nearest.
            // Accept only the two half values bracketing the scalar result.
            // Cross-variant comparison below remains byte-exact.
            const auto lowerBits=half(expected);
            const auto lower=halfFloat(lowerBits);
            const auto upper=expected==lower?lower:halfFloat(static_cast<std::uint16_t>(lowerBits+1));
            if(!std::isfinite(actual) || (actual!=lower && actual!=upper))
                throw std::runtime_error("GPU bloom output differs from independent scalar reference: source="
                    +std::to_string(width)+"x"+std::to_string(height)+", pixel="+std::to_string(x)+","+std::to_string(y)
                    +", channel="+std::to_string(channel)+", expected="+std::to_string(expected)+", actual="+std::to_string(actual)
                    +", bits="+std::to_string(output[(static_cast<std::size_t>(y)*outWidth+x)*4+channel]));
        }
    }
private:
    void release() noexcept {
        if(!host.backend) return;
        auto& gpu=*host.backend;
        if(timestamps) vkDestroyQueryPool(host.device,timestamps,nullptr);
        if(pipeline) gpu.destroyPipeline(pipeline);
        if(layout) gpu.destroyPipelineLayout(layout);
        if(pool) gpu.destroyDescriptorPool(pool);
        if(setLayout) gpu.destroyDescriptorSetLayout(setLayout);
        if(sampler) gpu.destroySampler(sampler);
        if(sourceView) gpu.destroyImageView(sourceView);
        if(destinationView) gpu.destroyImageView(destinationView);
        host.allocator.destroyImage(destination);host.allocator.destroyImage(source);
        host.allocator.destroyBuffer(readback);host.allocator.destroyBuffer(upload);
    }
};
}

int main(int argc,char** argv) {
    try {
        if(argc!=5) throw std::runtime_error("Usage: shader-composition GLSL.spv direct.spv cached.spv report.json");
        const std::array<std::vector<char>,3> shaders{bytes(argv[1]),bytes(argv[2]),bytes(argv[3])};
        test::VulkanComputeFixture host;
        nlohmann::json report={{"device",host.properties.deviceName},{"validationLayer",true},{"synchronizationValidation",true},
            {"sourceFormat","RGBA16F"},{"variants",nlohmann::json::array()}};
        unsigned cases=0;
        for(const auto extent:{std::array<std::uint32_t,2>{1,1},{7,9},{64,32}}) {
            std::array<std::unique_ptr<BloomProbe>,3> probes;
            for(std::size_t index=0;index<3;++index) probes[index]=std::make_unique<BloomProbe>(host,extent[0],extent[1],shaders[index]);
            for(const auto parameters:{shader::BloomParameters{1.2F,0},shader::BloomParameters{1.2F,1},
                                      shader::BloomParameters{-.5F,1},shader::BloomParameters{8.F,1}}) {
                const auto expected=probes[0]->run(parameters);probes[0]->reference(expected,parameters);
                for(std::size_t index=1;index<3;++index) {
                    const auto actual=probes[index]->run(parameters);probes[index]->reference(actual,parameters);
                    if(actual!=expected) throw std::runtime_error("GLSL and composed shader pixels differ");
                }
                ++cases;
            }
        }
        for(std::size_t index=0;index<3;++index) {
            BloomProbe probe(host,512,512,shaders[index]);std::vector<double> gpu,cpu;
            for(unsigned frame=0;frame<36;++frame) {
                const auto result=probe.run({1.2F,1});
                if(frame==0) probe.reference(result,{1.2F,1});
                if(frame>=4){gpu.push_back(probe.gpuMs);cpu.push_back(probe.recordMs);}
            }
            if(std::any_of(gpu.begin(),gpu.end(),[](double value){return !std::isfinite(value)||value<=0;}))
                throw std::runtime_error("Invalid GPU timestamp samples");
            auto sorted=gpu;std::sort(sorted.begin(),sorted.end());
            report["variants"].push_back({{"name",std::array<const char*,3>{"glsl","direct","cached"}[index]},
                {"gpuSamplesMs",gpu},{"cpuRecordingSamplesMs",cpu},{"gpuMedianMs",sorted[sorted.size()/2]},
                {"sourceExtent",{512,512}},{"warmup",4},{"samples",32}});
        }
        if(host.allocator.statistics().liveBuffers || host.allocator.statistics().liveImages)
            throw std::runtime_error("Compute fixture leaked GPU allocations");
        host.close();
        if(host.errors) throw std::runtime_error("Real Vulkan synchronization or validation failed");
        report["status"]="passed";report["pixelCases"]=cases;report["released"]=true;
        std::ofstream output(argv[4]);output<<report.dump(2)<<'\n';
        if(!output) throw std::runtime_error("Cannot persist comparison report");
        std::cout<<"Three shader variants passed "<<cases<<" pixel cases with real validation and complete release\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
