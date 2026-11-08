#include "GpuTransparentIndexSort.hpp"
#include "GpuStableKeySort.hpp"
#include "rhi/IGpuAllocator.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <stdexcept>

namespace azurerender {
namespace {
constexpr std::uint32_t headerWords=16,metaWords=24,vertexWords=24;
std::uint32_t bits(float value){std::uint32_t result;std::memcpy(&result,&value,4);return result;}
double magnitude(float value){if(!std::isfinite(value))throw std::invalid_argument("Nonfinite transparent geometry input");return std::abs(double(value));}
template<class Values>double maximumMagnitude(const Values& values){double maximum=0;for(auto v:values)maximum=std::max(maximum,magnitude(v));return maximum;}
}
struct GpuTransparentIndexSort::Impl {
    struct MorphSchema {std::uint32_t node=0,offset=0;std::vector<double> bounds;std::vector<float> defaults;};
    struct Geometry {
        const LoadedAsset* identity=nullptr;const void* vertexIdentity=nullptr;const void* indexIdentity=nullptr;
        std::size_t vertexCount=0,indexCount=0;
        std::vector<std::uint32_t> vertices,segments;
        std::vector<TransparentPrimitiveRange> ranges;
        std::vector<std::array<std::uint32_t,6>> triangles;
        std::vector<MorphSchema> morphs;
        std::uint32_t weightCount=0,requiredJoints=0;
        bool legacy=true;
        double positionBound=0,morph0Bound=0,morph1Bound=0,weightSumBound=0;
    };
    struct Frame {
        rhi::GpuBuffer packed{},positions{},keys{},indices{};
        std::array<VkDescriptorSet,3> sets{};
        std::vector<std::uint32_t> geometries,segments;
        bool valid=false,recorded=false;std::uint64_t generation=0;
        std::uint32_t vertices=0,triangles=0;
    };
    rhi::IRhi& rhi;GpuTransparentSortLimits limits{};
    std::vector<std::uint32_t> words;
    std::vector<Geometry> geometry;
    std::vector<Frame> frames;
    std::unique_ptr<GpuStableKeySort> sorter;
    VkDescriptorSetLayout setLayout{};VkDescriptorPool pool{};VkPipelineLayout layout{};
    std::array<VkPipeline,3> pipelines{};
    std::uint32_t weightsBase=0,jobsBase=0,trianglesBase=0,staticBase=0;
    std::uint64_t byteCount=0,wordLimit=0;
    explicit Impl(rhi::IRhi& backend):rhi(backend){}
    ~Impl(){
        for(auto pipeline:pipelines)if(pipeline)rhi.destroyPipeline(pipeline);
        if(layout)rhi.destroyPipelineLayout(layout);
        if(pool)rhi.destroyDescriptorPool(pool);
        if(setLayout)rhi.destroyDescriptorSetLayout(setLayout);
        for(auto& frame:frames){for(auto* buffer:{&frame.packed,&frame.positions,&frame.keys,&frame.indices})rhi.allocator().destroyBuffer(*buffer);}
    }
    void appendSpace(std::size_t count){
        if(count>wordLimit||words.size()>wordLimit-count)throw std::length_error("Transparent GPU source exceeds its pool budget");
        words.resize(words.size()+count);
    }
    void encodeGeometry(const GpuTransparentSortGeometry& input){
        if(!input.asset||!input.topology)throw std::invalid_argument("Transparent geometry source is missing");
        const auto& asset=*input.asset;const auto& topology=*input.topology;
        if(!topology.prepared||topology.sourceIndices!=asset.indices.data()||topology.sourceIndexCount!=asset.indices.size()
            ||topology.sourceVertexCount!=asset.vertices.size()||topology.vertices.size()>GpuStableKeySort::kMaximumRecords)
            throw std::invalid_argument("Transparent topology does not match its source");
        Geometry g;g.identity=&asset;g.vertexIdentity=asset.vertices.data();g.indexIdentity=asset.indices.data();
        g.vertexCount=asset.vertices.size();g.indexCount=asset.indices.size();g.legacy=asset.morphRanges.empty();
        std::map<std::size_t,std::size_t> schemas;
        for(auto original:topology.vertices){
            const auto& vertex=asset.vertices.at(original);
            const auto offset=static_cast<std::uint32_t>(words.size());appendSpace(vertexWords);g.vertices.push_back(offset);
            for(unsigned a=0;a<3;++a){g.positionBound=std::max(g.positionBound,magnitude(vertex.position[a]));words[offset+a]=bits(vertex.position[a]);}
            double sum=0;
            for(unsigned a=0;a<4;++a){
                words[offset+4+a]=vertex.joints[a];words[offset+8+a]=bits(vertex.weights[a]);sum+=magnitude(vertex.weights[a]);
                if(vertex.weights[a]!=0){if(vertex.joints[a]==UINT32_MAX)throw std::invalid_argument("Transparent joint index overflow");
                    g.requiredJoints=std::max(g.requiredJoints,vertex.joints[a]+1);}
            }
            g.weightSumBound=std::max(g.weightSumBound,sum);words[offset+15]=g.legacy?0U:1U;
            if(g.legacy){for(unsigned a=0;a<3;++a){
                g.morph0Bound=std::max(g.morph0Bound,magnitude(vertex.morph0[a]));g.morph1Bound=std::max(g.morph1Bound,magnitude(vertex.morph1[a]));
                words[offset+12+a]=bits(vertex.morph0[a]);words[offset+16+a]=bits(vertex.morph1[a]);}continue;}
            for(std::size_t r=0;r<asset.morphRanges.size();++r){
                const auto& range=asset.morphRanges[r];if(original<range.firstVertex||original-range.firstVertex>=range.vertexCount)continue;
                auto found=schemas.find(r);
                if(found==schemas.end()){
                    if(range.targets.size()>limits.morphWeights-g.weightCount)throw std::length_error("Transparent source morph weight budget exceeded");
                    MorphSchema schema;schema.node=range.node;schema.offset=g.weightCount;schema.bounds.resize(range.targets.size());
                    if(range.node<asset.defaultMorphWeights.size())schema.defaults=asset.defaultMorphWeights[range.node];
                    found=schemas.emplace(r,g.morphs.size()).first;g.weightCount+=static_cast<std::uint32_t>(range.targets.size());g.morphs.push_back(std::move(schema));
                }
                auto& schema=g.morphs[found->second];words[offset+19]=static_cast<std::uint32_t>(words.size());
                words[offset+20]=schema.offset;words[offset+21]=static_cast<std::uint32_t>(range.targets.size());
                for(std::size_t t=0;t<range.targets.size();++t){
                    const auto at=words.size();appendSpace(4);const auto& delta=range.targets[t].positions;
                    if(!delta.empty()){
                        const auto& change=delta.at(original-range.firstVertex);words[at+3]=1;
                        for(unsigned a=0;a<3;++a){schema.bounds[t]=std::max(schema.bounds[t],magnitude(change[a]));words[at+a]=bits(change[a]);}
                    }
                }
                break;
            }
        }
        for(const auto& primitive:topology.primitives){
            const auto& range=primitive.range;
            if(range.indexCount%3||std::uint64_t(range.firstIndex)+range.indexCount>asset.indices.size()
                ||primitive.localIndices.size()!=range.indexCount)
                throw std::invalid_argument("GPU transparent topology requires complete source triangles");
            if(g.triangles.size()+range.indexCount/3>GpuStableKeySort::kMaximumRecords)throw std::length_error("Transparent source triangle budget exceeded");
            g.ranges.push_back(range);g.segments.push_back(range.indexCount/3);
            for(unsigned t=0;t<range.indexCount/3;++t){
                std::array<std::uint32_t,6> triangle{};
                for(unsigned a=0;a<3;++a){const auto local=primitive.localIndices[t*3+a];
                    if(local>=g.vertices.size())throw std::invalid_argument("Transparent local vertex is out of bounds");
                    const auto index=asset.indices[range.firstIndex+t*3+a];
                    if(topology.vertices[local]!=index)throw std::invalid_argument("Transparent local topology changed its source index");
                    triangle[a]=local;triangle[3+a]=index;
                }g.triangles.push_back(triangle);
            }
        }
        geometry.push_back(std::move(g));
    }
    void initialize(const std::vector<GpuTransparentSortGeometry>& sources,GpuTransparentSortLimits config,const GpuTransparentSortShaders& shaders){
        limits=config;
        if(!limits.vertices||limits.vertices>GpuStableKeySort::kMaximumRecords||!limits.instances||limits.instances>65536
            ||!limits.morphWeights||limits.morphWeights>1048576||sources.size()>65536
            ||!GpuStableKeySort::supported(limits.triangles,limits.slots))throw std::invalid_argument("Invalid transparent GPU capacities");
        const auto other=std::uint64_t(limits.slots)*(16ULL*limits.vertices+44ULL*limits.triangles);
        if(other>=kMaximumBytes)throw std::length_error("Transparent GPU scratch exceeds its pool budget");
        wordLimit=(kMaximumBytes-other)/limits.slots/4;
        weightsBase=headerWords+limits.instances*metaWords;jobsBase=weightsBase+limits.morphWeights;
        trianglesBase=jobsBase+limits.vertices*2;staticBase=trianglesBase+limits.triangles*6;
        appendSpace(staticBase);for(const auto& source:sources)encodeGeometry(source);
        words[0]=jobsBase;words[1]=trianglesBase;
        sorter=std::make_unique<GpuStableKeySort>(rhi,limits.triangles,limits.slots,shaders.sort);
        setLayout=rhi.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},
            {1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},{2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
        layout=rhi.createPipelineLayout(setLayout,nullptr);
        const std::array<const std::vector<char>*,3> codes{&shaders.deform,&shaders.keys,&shaders.scatter};
        for(unsigned i=0;i<3;++i){auto module=rhi.createShaderModule(*codes[i]);
            try{pipelines[i]=rhi.createComputePipeline({module,layout});}catch(...){rhi.destroyShaderModule(module);throw;}
            rhi.destroyShaderModule(module);}
        rhi::DescriptorPoolDesc description;description.maxSets=limits.slots*3;description.sizes={{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,limits.slots*9}};
        pool=rhi.createDescriptorPool(description);frames.resize(limits.slots);
        for(auto& frame:frames){
            frame.packed=rhi.allocator().createBuffer(words.size()*4,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
            if(!frame.packed.mapped)throw std::runtime_error("Transparent source upload is not mapped");
            std::memcpy(frame.packed.mapped,words.data(),words.size()*4);rhi.allocator().flush(frame.packed,0,frame.packed.size);
            frame.positions=rhi.allocator().createBuffer(VkDeviceSize(limits.vertices)*16,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT,false);
            frame.keys=rhi.allocator().createBuffer(VkDeviceSize(limits.triangles)*8,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT,false);
            frame.indices=rhi.allocator().createBuffer(VkDeviceSize(limits.triangles)*12,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_INDEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT,false);
            const auto sets=rhi.allocateDescriptorSets(pool,setLayout,3);std::copy(sets.begin(),sets.end(),frame.sets.begin());
            for(auto set:frame.sets)rhi.writeDescriptorBuffer({set,0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,frame.packed.buffer,frame.packed.size});
            const auto bind=[&](unsigned set,unsigned number,const rhi::GpuBuffer& buffer){
                rhi.writeDescriptorBuffer({frame.sets[set],number,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,buffer.buffer,buffer.size});};
            bind(0,2,frame.positions);bind(1,1,frame.positions);bind(1,2,frame.keys);bind(2,2,frame.indices);
        }
        byteCount=std::uint64_t(limits.slots)*(words.size()*4+16ULL*limits.vertices+20ULL*limits.triangles)+sorter->bytes();
    }
};
GpuTransparentIndexSort::GpuTransparentIndexSort(rhi::IRhi& backend,const std::vector<GpuTransparentSortGeometry>& geometry,
    GpuTransparentSortLimits limits,const GpuTransparentSortShaders& shaders):impl_(std::make_unique<Impl>(backend)){impl_->initialize(geometry,limits,shaders);}
GpuTransparentIndexSort::~GpuTransparentIndexSort()=default;
bool GpuTransparentIndexSort::matchesGeometry(unsigned index,const LoadedAsset& asset,const TransparentTopology& topology)const{
    if(index>=impl_->geometry.size())return false;const auto& g=impl_->geometry[index];
    if(!topology.prepared||g.identity!=&asset||g.vertexIdentity!=asset.vertices.data()||g.indexIdentity!=asset.indices.data()
        ||g.vertexCount!=asset.vertices.size()||g.indexCount!=asset.indices.size()||g.ranges.size()!=topology.primitives.size())return false;
    for(std::size_t i=0;i<g.ranges.size();++i)if(g.ranges[i].firstIndex!=topology.primitives[i].range.firstIndex||g.ranges[i].indexCount!=topology.primitives[i].range.indexCount)return false;
    return true;
}
GpuTransparentIndexSort::Ticket GpuTransparentIndexSort::prepare(unsigned slot,const rhi::GpuBuffer& joints,
    const std::vector<GpuTransparentSortInstance>& instances,const internal::Vector3& camera,const internal::Vector3& forward){
    auto& p=*impl_;auto& frame=p.frames.at(slot);frame.valid=false;
    if(instances.size()>p.limits.instances)throw std::length_error("Transparent GPU instance budget exceeded");
    const auto cameraBound=maximumMagnitude(camera),forwardBound=maximumMagnitude(forward);
    std::uint64_t vertices=0,triangles=0,weights=0,requiredJointBytes=0;std::vector<std::uint32_t> geometryKeys,segments;
    std::vector<std::vector<float>> instanceWeights;instanceWeights.reserve(instances.size());
    for(const auto& instance:instances){
        const auto& g=p.geometry.at(instance.geometry);geometryKeys.push_back(instance.geometry);
        vertices+=g.vertices.size();triangles+=g.triangles.size();weights+=g.weightCount;
        if(vertices>p.limits.vertices||triangles>p.limits.triangles||weights>p.limits.morphWeights
            ||segments.size()+g.segments.size()>GpuStableKeySort::kMaximumSegments)throw std::length_error("Transparent GPU frame budget exceeded");
        segments.insert(segments.end(),g.segments.begin(),g.segments.end());
        const auto modelBound=maximumMagnitude(instance.model);maximumMagnitude(instance.morph);
        const auto jointCount=instance.pose?instance.pose->jointMatrices.size():0;
        double jointBound=0;
        if(jointCount){
            if(jointCount<g.requiredJoints||jointCount>UINT32_MAX||std::uint64_t(instance.jointBase)+jointCount>UINT32_MAX)
                throw std::invalid_argument("Transparent GPU pose does not cover its joints");
            requiredJointBytes=std::max(requiredJointBytes,(std::uint64_t(instance.jointBase)+jointCount)*64);
            for(const auto& matrix:instance.pose->jointMatrices)jointBound=std::max(jointBound,maximumMagnitude(matrix));
        }
        auto& values=instanceWeights.emplace_back(g.weightCount);double positionBound=g.positionBound;
        if(g.legacy)positionBound+=g.morph0Bound*std::abs(double(instance.morph[0]))+g.morph1Bound*std::abs(double(instance.morph[1]));
        for(const auto& schema:g.morphs){
            const auto& source=instance.pose?instance.pose->morphWeights.at(schema.node):schema.defaults;
            if(source.size()!=schema.bounds.size())throw std::invalid_argument("Transparent GPU morph pose shape mismatch");
            for(std::size_t t=0;t<source.size();++t){positionBound+=schema.bounds[t]*magnitude(source[t]);values[schema.offset+t]=source[t];}
        }
        if(jointCount)positionBound=(3*jointBound*positionBound+jointBound)*g.weightSumBound;
        const auto worldBound=3*modelBound*positionBound+modelBound;
        if(worldBound>double(std::numeric_limits<float>::max())/8
            ||3*forwardBound*(worldBound+cameraBound)>double(std::numeric_limits<float>::max())/8)
            throw std::invalid_argument("Transparent GPU transform may overflow binary32");
    }
    if(requiredJointBytes&&(!joints.buffer||joints.size<requiredJointBytes))throw std::invalid_argument("Transparent GPU joint buffer is too small");
    if(requiredJointBytes)for(const auto& owned:p.frames)for(const auto* buffer:{&owned.packed,&owned.positions,&owned.keys,&owned.indices})
        if(joints.buffer==buffer->buffer)throw std::invalid_argument("Transparent GPU joints alias internal storage");
    if(frame.generation==UINT64_MAX)throw std::overflow_error("Transparent GPU generation exhausted");
    auto* data=static_cast<std::uint32_t*>(frame.packed.mapped);const bool changed=frame.geometries!=geometryKeys;
    if(changed){
        frame.geometries.clear();frame.segments.clear();std::uint32_t vertexBase=0,triangleBase=0;
        for(std::size_t i=0;i<instances.size();++i){const auto& g=p.geometry[instances[i].geometry];
            for(std::size_t v=0;v<g.vertices.size();++v){const auto job=p.jobsBase+(vertexBase+v)*2;data[job]=headerWords+static_cast<std::uint32_t>(i)*metaWords;data[job+1]=g.vertices[v];}
            for(const auto& triangle:g.triangles){const auto at=p.trianglesBase+triangleBase++*6;
                for(unsigned a=0;a<3;++a){data[at+a]=vertexBase+triangle[a];data[at+3+a]=triangle[3+a];}}
            vertexBase+=static_cast<std::uint32_t>(g.vertices.size());
        }
    }
    data[2]=static_cast<std::uint32_t>(vertices);data[3]=static_cast<std::uint32_t>(triangles);
    for(unsigned a=0;a<3;++a){data[4+a]=bits(camera[a]);data[8+a]=bits(forward[a]);}
    std::uint32_t weightBase=p.weightsBase;
    for(std::size_t i=0;i<instances.size();++i){const auto& instance=instances[i];const auto meta=headerWords+i*metaWords;
        for(unsigned a=0;a<16;++a)data[meta+a]=bits(instance.model[a]);
        data[meta+16]=instance.jointBase;data[meta+17]=instance.pose?static_cast<std::uint32_t>(instance.pose->jointMatrices.size()):0;
        data[meta+18]=bits(instance.morph[0]);data[meta+19]=bits(instance.morph[1]);data[meta+20]=weightBase;
        for(auto weight:instanceWeights[i])data[weightBase++]=bits(weight);
    }
    p.rhi.allocator().flush(frame.packed,0,VkDeviceSize(changed?p.staticBase:p.jobsBase)*4);
    if(changed){frame.geometries.swap(geometryKeys);frame.segments.swap(segments);}
    const auto& jointSource=requiredJointBytes?joints:frame.packed;
    p.rhi.writeDescriptorBuffer({frame.sets[0],1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,jointSource.buffer,jointSource.size});
    frame.vertices=static_cast<std::uint32_t>(vertices);frame.triangles=static_cast<std::uint32_t>(triangles);
    frame.valid=true;frame.recorded=false;++frame.generation;
    return {slot,frame.generation,frame.vertices,frame.triangles*3};
}
void GpuTransparentIndexSort::record(rhi::ICommandRecorder& commands,Ticket ticket){
    auto& p=*impl_;auto& frame=p.frames.at(ticket.slot);
    if(!frame.valid||frame.recorded||frame.generation!=ticket.generation||frame.vertices!=ticket.vertexCount||frame.triangles*3!=ticket.indexCount)
        throw std::logic_error("Transparent GPU recording ticket is stale or consumed");
    frame.recorded=true;if(!frame.vertices&&!frame.triangles)return;
    commands.bufferBarrier({frame.packed.buffer,0,frame.packed.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
    const auto dispatch=[&](unsigned stage,std::uint32_t count){commands.bindComputePipeline(p.pipelines[stage]);commands.bindComputeDescriptorSet(p.layout,frame.sets[stage]);commands.dispatch((count+127)/128,1,1);};
    const auto publish=[&](const rhi::GpuBuffer& buffer){commands.bufferBarrier({buffer.buffer,0,buffer.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});};
    if(frame.vertices){dispatch(0,frame.vertices);publish(frame.positions);}
    if(!frame.triangles)return;
    dispatch(1,frame.triangles);publish(frame.keys);
    const auto sorted=p.sorter->record(commands,ticket.slot,frame.keys,frame.segments);
    p.rhi.writeDescriptorBuffer({frame.sets[2],1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,sorted.buffer->buffer,sorted.buffer->size});
    dispatch(2,frame.triangles);
    commands.bufferBarrier({frame.indices.buffer,0,frame.indices.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
}
const rhi::GpuBuffer& GpuTransparentIndexSort::indices(unsigned slot)const{return impl_->frames.at(slot).indices;}
const rhi::GpuBuffer& GpuTransparentIndexSort::positions(unsigned slot)const{return impl_->frames.at(slot).positions;}
const rhi::GpuBuffer& GpuTransparentIndexSort::keys(unsigned slot)const{return impl_->frames.at(slot).keys;}
std::uint64_t GpuTransparentIndexSort::bytes()const noexcept{return impl_->byteCount;}
}
