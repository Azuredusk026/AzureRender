#include "render/VisibilityPrototype.hpp"
#include "render/GpuCullingResources.hpp"
#include "resources/BinaryFile.hpp"
#include "support/VulkanComputeFixture.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <cstring>
#include <fstream>
#include <cmath>
using namespace azurerender;
int main(int argc,char** argv) try {
    if(argc!=4) throw std::invalid_argument("Expected cull shader, visibility shader and report path");
    test::VulkanComputeFixture host;
    nlohmann::json report={{"schemaVersion",1},{"device",host.properties.deviceName},{"runs",nlohmann::json::array()}};
    for(std::uint32_t count:{100U,500U,10000U}) {
        std::vector<GpuCullBounds> bounds;
        std::vector<VkDrawIndexedIndirectCommand> commands;
        std::vector<GpuSurfaceIdentity> identities;
        for(std::uint32_t i=0;i<count;++i) {
            const float x=i%4==0?10.F:0.F;
            bounds.push_back({{x-1,-1,-1,0},{x+1,1,1,0}});
            commands.push_back({6,1,i*6,0,i});identities.push_back({i,i%3,i%7,0});
        }
        GpuCullParameters params;
        params.planes={{{1,0,0,2},{-1,0,0,2},{0,1,0,2},{0,-1,0,2},{0,0,1,2},{0,0,-1,2}}};
        VisibilityPrototype mapping({1,2,3},bounds,commands,identities);
        const auto expected=mapping.reference(params);
        std::vector<VkDrawIndexedIndirectCommand> baseline;
        for(bool prototype:{false,true}) {
            GpuCullingResources resources(*host.backend);
            resources.initialize(readBinaryFile(argv[prototype?2:1]),count,prototype);
            resources.upload(bounds,commands,prototype?identities:std::vector<GpuSurfaceIdentity>{});
            VkQueryPool query=VK_NULL_HANDLE;
            VkQueryPoolCreateInfo info{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};info.queryType=VK_QUERY_TYPE_TIMESTAMP;info.queryCount=2;
            test::vkRequire(vkCreateQueryPool(host.device,&info,nullptr,&query),"vkCreateQueryPool");
            double gpuMs=0,recordMs=0;
            std::vector<double> gpuSamples,cpuSamples;
            for(unsigned iteration=0;iteration<32;++iteration) {
                host.submit([&](rhi::ICommandRecorder& recorder,VkCommandBuffer command){
                    const auto start=std::chrono::steady_clock::now();
                    for(const auto* input:{&resources.bounds(),&resources.source()}) {
                        host.allocator.flush(*input,0,input->size);
                        recorder.bufferBarrier({input->buffer,0,input->size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
                    }
                    if(prototype) {
                        const auto& input=resources.surfaceSource();host.allocator.flush(input,0,input.size);
                        recorder.bufferBarrier({input.buffer,0,input.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
                    }
                    vkCmdResetQueryPool(command,query,0,2);
                    recorder.writeTimestamp(query,0,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
                    resources.record(recorder,params);
                    recorder.writeTimestamp(query,1,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
                    const auto readBarrier=[&](const rhi::GpuBuffer& output){
                        recorder.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,
                            VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
                    };
                    readBarrier(resources.output());if(prototype)readBarrier(resources.surfaceOutput());
                    if(iteration>=2) {
                        const auto duration=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
                        recordMs+=duration;cpuSamples.push_back(duration);
                    }
                });
                std::uint64_t time[2]{};
                test::vkRequire(vkGetQueryPoolResults(host.device,query,0,2,sizeof(time),time,sizeof(time[0]),VK_QUERY_RESULT_64_BIT|VK_QUERY_RESULT_WAIT_BIT),"vkGetQueryPoolResults");
                const auto mask=host.timestampBits>=64?~std::uint64_t{0}:((std::uint64_t{1}<<host.timestampBits)-1);
                if(iteration>=2) {
                    const auto duration=static_cast<double>((time[1]-time[0])&mask)*host.properties.limits.timestampPeriod/1000000.;
                    gpuMs+=duration;gpuSamples.push_back(duration);
                }
            }
            vkDestroyQueryPool(host.device,query,nullptr);
            host.allocator.invalidate(resources.output(),0,resources.output().size);
            const auto* actual=static_cast<const VkDrawIndexedIndirectCommand*>(resources.output().mapped);
            for(std::uint32_t i=0;i<count;++i) {
                const auto visible=i%4==0?0U:1U;
                if(actual[i].firstInstance!=i || actual[i].instanceCount!=visible || actual[i].firstIndex!=i*6)
                    throw std::runtime_error("GPU visibility changed stable draw identity or visibility");
            }
            if(!prototype) baseline.assign(actual,actual+count);
            else {
                if(std::memcmp(baseline.data(),actual,count*sizeof(*actual)))throw std::runtime_error("GPU variants differ");
                const auto& output=resources.surfaceOutput();host.allocator.invalidate(output,0,output.size);
                const auto* surface=static_cast<const GpuSurfaceIdentity*>(output.mapped);
                std::vector<GpuSurfaceIdentity> values(surface,surface+count);
                if(std::memcmp(values.data(),expected.data(),count*sizeof(*surface)))throw std::runtime_error("GPU surface identities differ from CPU reference");
                if(!mapping.pick({1,2,3},values,1) || mapping.pick({1,2,3},values,0))throw std::runtime_error("GPU picking mismatch");
            }
            if(!std::isfinite(gpuMs) || gpuMs<=0)throw std::runtime_error("Invalid timestamp samples");
            report["runs"].push_back({{"instances",count},{"prototype",prototype},{"samples",30},
                {"gpuMeanMs",gpuMs/30},{"cpuRecordMeanMs",recordMs/30},{"visible",count*3/4},
                {"gpuSamplesMs",gpuSamples},{"cpuRecordSamplesMs",cpuSamples}});
        }
        if(host.allocator.statistics().liveBuffers || host.allocator.statistics().liveImages)
            throw std::runtime_error("Visibility resources leaked after unload");
    }
    if(host.errors)throw std::runtime_error("GPU validation errors");
    report["validationErrors"]=host.errors.load();report["status"]="passed";
    std::ofstream(argv[3])<<report.dump(2)<<'\n';
    std::cout<<report.dump(2)<<'\n';return 0;
} catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
