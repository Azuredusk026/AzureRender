#include "render/IrradianceGridGpuData.hpp"
#include "resources/BinaryFile.hpp"
#include "support/VulkanComputeFixture.hpp"
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
using namespace azurerender;

struct GridQuery {
    std::array<float,3> position,normal;
    std::array<float,4> expected;
};

int main(int argc,char** argv)try {
    if(argc<2||argc>4)throw std::invalid_argument("Expected shader, optional source index and device name");
    test::VulkanComputeFixture host(false,false,false,false,false,false,false,false,false,argc==4?argv[3]:"");
    auto& backend=*host.backend;
    rhi::GpuBuffer input{},output{},cells{};
    VkDescriptorSetLayout setLayout{};
    VkDescriptorPool pool{};
    VkPipelineLayout layout{};
    VkShaderModule shader{};
    VkPipeline pipeline{};
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*) {
        if(pipeline)backend.destroyPipeline(pipeline);
        if(shader)backend.destroyShaderModule(shader);
        if(layout)backend.destroyPipelineLayout(layout);
        if(pool)backend.destroyDescriptorPool(pool);
        if(setLayout)backend.destroyDescriptorSetLayout(setLayout);
        host.allocator.destroyBuffer(input);
        host.allocator.destroyBuffer(output);
        host.allocator.destroyBuffer(cells);
    });
    setLayout=backend.createDescriptorSetLayout({
        {0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},
        {1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},
        {24,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
    rhi::DescriptorPoolDesc poolDesc;
    poolDesc.maxSets=1;
    poolDesc.sizes={{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,3}};
    pool=backend.createDescriptorPool(poolDesc);
    const auto set=backend.allocateDescriptorSets(pool,setLayout,1).front();
    layout=backend.createPipelineLayout(setLayout,nullptr);
    shader=backend.createShaderModule(readBinaryFile(argv[1]));
    pipeline=backend.createComputePipeline({shader,layout});
    float maximumCpuError=0,maximumSourceError=0;
    unsigned queryCount=0,sourceCount=0,gridCount=0;
    auto check=[&](const IrradianceVolume& volume,const std::vector<GridQuery>& queries,bool source) {
        const auto packed=packIrradianceGridCells(volume);
        std::vector<std::array<float,20>> inputs(queries.size());
        for(std::size_t index=0;index<queries.size();++index) {
            auto& values=inputs[index];
            std::copy(queries[index].position.begin(),queries[index].position.end(),values.begin());
            std::copy(queries[index].normal.begin(),queries[index].normal.end(),values.begin()+4);
            std::copy(volume.origin.begin(),volume.origin.end(),values.begin()+8);
            values[11]=volume.spacing;
            for(unsigned axis=0;axis<3;++axis)values[12+axis]=float(volume.dimensions[axis]);
            values[15]=volume.basis==IrradianceBasis::Quadratic?1.F:0.F;
            values[16]=1;
        }
        input=host.allocator.createBuffer(inputs.size()*sizeof(inputs[0]),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
        output=host.allocator.createBuffer(queries.size()*16,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
        cells=host.allocator.createBuffer(packed.size()*sizeof(packed[0]),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
        std::memcpy(input.mapped,inputs.data(),input.size);
        std::memcpy(cells.mapped,packed.data(),cells.size);
        host.allocator.flush(input,0,input.size);
        host.allocator.flush(cells,0,cells.size);
        backend.writeDescriptorBuffer({set,0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,input.buffer,input.size});
        backend.writeDescriptorBuffer({set,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,output.buffer,output.size});
        backend.writeDescriptorBuffer({set,24,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,cells.buffer,cells.size});
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer) {
            commands.bindComputePipeline(pipeline);
            commands.bindComputeDescriptorSet(layout,set);
            commands.dispatch(static_cast<unsigned>(queries.size()),1,1);
            commands.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
        });
        host.allocator.invalidate(output,0,output.size);
        const auto* values=static_cast<const float*>(output.mapped);
        for(std::size_t index=0;index<queries.size();++index) {
            const auto sample=volume.query(queries[index].position);
            const auto evaluated=sample.evaluate(queries[index].normal);
            for(unsigned channel=0;channel<4;++channel) {
                const float actual=values[index*4+channel];
                if(!std::isfinite(actual))throw std::runtime_error("Nonfinite GPU irradiance");
                const float expected=channel<3?evaluated[channel]:sample.weight;
                maximumCpuError=std::max(maximumCpuError,std::abs(actual-expected));
                if(source)maximumSourceError=std::max(maximumSourceError,std::abs(actual-queries[index].expected[channel]));
            }
        }
        queryCount+=static_cast<unsigned>(queries.size());
        if(source)sourceCount+=static_cast<unsigned>(queries.size());
        ++gridCount;
        host.allocator.destroyBuffer(input);input={};
        host.allocator.destroyBuffer(output);output={};
        host.allocator.destroyBuffer(cells);cells={};
    };
    for(auto basis:{IrradianceBasis::Linear,IrradianceBasis::Quadratic}) {
        IrradianceVolume volume;
        volume.basis=basis;
        volume.dimensions={2,2,2};
        volume.cells.resize(8);
        if(basis==IrradianceBasis::Quadratic)volume.quadraticCells.resize(8);
        for(unsigned index=0;index<8;++index) {
            volume.cells[index].weight=index==3?0:float(index+1)/8;
            for(unsigned channel=0;channel<3;++channel) {
                for(unsigned term=0;term<4;++term)
                    volume.cells[index].coefficients[channel][term]=float(int(index+channel+term)%5-2)*.2F;
                if(basis==IrradianceBasis::Quadratic)
                    for(unsigned term=0;term<5;++term)
                        volume.quadraticCells[index][channel][term]=float(int(index+2*channel+term)%7-3)*.15F;
            }
        }
        std::vector<GridQuery> queries;
        const float diagonal=std::sqrt(1.F/3.F);
        for(const auto position:std::vector<std::array<float,3>>{{0,0,0},{1,1,1},{1,1,0},{.2F,.6F,.8F},{-.1F,0,0},{1.1F,1,1}})
            for(const auto normal:std::vector<std::array<float,3>>{{1,0,0},{0,1,0},{0,0,-1},{diagonal,-diagonal,diagonal}})
                queries.push_back({position,normal,{}});
        check(volume,queries,false);
    }
    if(argc>=3) {
        std::ifstream indexFile(std::filesystem::u8path(argv[2]));
        if(!indexFile)throw std::runtime_error("Cannot read source grid index");
        std::string line;
        while(std::getline(indexFile,line)) {
            if(line.empty())continue;
            std::istringstream entry(line);
            std::string gridPath,directionsPath;
            if(!(entry>>std::quoted(gridPath)>>std::quoted(directionsPath)))throw std::runtime_error("Invalid source grid index");
            auto volume=IrradianceVolume::load(std::filesystem::u8path(gridPath));
            std::ifstream directions(std::filesystem::u8path(directionsPath));
            if(!directions)throw std::runtime_error("Cannot read source directions");
            std::vector<GridQuery> queries;
            GridQuery query{{0,2,8},{},{0,0,0,1}};
            while(directions>>query.normal[0]>>query.normal[1]>>query.normal[2]
                >>query.expected[0]>>query.expected[1]>>query.expected[2]) {
                for(unsigned channel=0;channel<3;++channel)query.expected[channel]=std::max(0.F,query.expected[channel]);
                queries.push_back(query);
            }
            if(!directions.eof()||queries.size()!=26)throw std::runtime_error("Expected 26 source directions");
            check(volume,queries,true);
        }
        if(sourceCount!=624)throw std::runtime_error("Expected 624 original source directions");
    }
    if(maximumCpuError>1e-5F||maximumSourceError>1e-5F)throw std::runtime_error("GPU irradiance differs from contract");
    cleanup.reset();
    if(host.errors.load()||host.allocator.statistics().liveImages||host.allocator.statistics().liveBuffers)
        throw std::runtime_error("Irradiance GPU validation or lifetime failure");
    std::cout<<"Device="<<host.properties.deviceName<<", grids="<<gridCount<<", queries="<<queryCount
        <<", original directions="<<sourceCount<<", CPU error="<<maximumCpuError<<", original error="<<maximumSourceError<<'\n';
    return 0;
}catch(const std::exception& error) {
    std::cerr<<error.what()<<'\n';
    return 1;
}
