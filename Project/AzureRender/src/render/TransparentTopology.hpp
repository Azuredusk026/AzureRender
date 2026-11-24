#pragma once
#include "render/LinearMath.hpp"
#include "render/StableDepthOrder.hpp"
#include "render/RecordingWorkerPool.hpp"
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace azurerender {
struct TransparentPrimitiveRange {std::uint32_t firstIndex=0,indexCount=0;};
struct TransparentPrimitiveTopology {
    TransparentPrimitiveRange range;
    std::vector<std::uint32_t> localIndices;
};
// The source index storage is immutable and outlives this prepared topology.
struct TransparentTopology {
    const std::uint32_t* sourceIndices=nullptr;
    std::size_t sourceIndexCount=0,sourceVertexCount=0;
    bool prepared=false;
    std::vector<std::uint32_t> vertices;
    std::vector<TransparentPrimitiveTopology> primitives;
};
inline TransparentTopology prepareTransparentTopology(const std::vector<std::uint32_t>& indices,
    std::size_t vertexCount,const std::vector<TransparentPrimitiveRange>& ranges){
    if(vertexCount>UINT32_MAX)throw std::length_error("Transparent vertices exceed index addressing");
    TransparentTopology result;result.sourceIndices=indices.data();result.sourceIndexCount=indices.size();result.sourceVertexCount=vertexCount;
    if(ranges.empty()){result.prepared=true;return result;}
    std::vector<std::uint32_t> remap(vertexCount,UINT32_MAX);result.primitives.reserve(ranges.size());
    for(const auto& range:ranges){
        if(std::uint64_t(range.firstIndex)+range.indexCount>indices.size())throw std::out_of_range("Transparent primitive exceeds source indices");
        TransparentPrimitiveTopology primitive;primitive.range=range;const auto complete=(range.indexCount/3)*3;
        primitive.localIndices.reserve(complete);
        for(std::uint32_t i=0;i<complete;++i){const auto original=indices[range.firstIndex+i];
            if(original>=vertexCount)throw std::out_of_range("Transparent index exceeds source vertices");
            auto& local=remap[original];if(local==UINT32_MAX){local=static_cast<std::uint32_t>(result.vertices.size());result.vertices.push_back(original);}
            primitive.localIndices.push_back(local);
        }
        result.primitives.push_back(std::move(primitive));
    }
    result.prepared=true;return result;
}
struct TransparentTopologyWorkspace {
    std::vector<internal::Vector3> worldPositions;
    std::vector<std::uint32_t> order;
    std::vector<float> depths;
    DepthOrderWorkspace sort;
};
template<class Position>
void writeSortedTransparentIndices(const TransparentTopology& topology,Position&& position,
    const internal::Vector3& cameraPosition,const internal::Vector3& cameraForward,
    std::uint32_t* output,TransparentTopologyWorkspace& scratch,RecordingWorkerPool* workers=nullptr){
    if(!topology.prepared)throw std::logic_error("Transparent topology is not prepared");
    if(!output)for(const auto& primitive:topology.primitives)
        if(primitive.range.indexCount)throw std::invalid_argument("Transparent output storage is missing");
    scratch.worldPositions.resize(topology.vertices.size());
    const auto fill=[&](std::size_t first,std::size_t last){
        for(std::size_t i=first;i<last;++i)scratch.worldPositions[i]=position(topology.vertices[i]);};
    if(workers&&topology.vertices.size()>=4096){
        const auto tasks=std::min(workers->recordingThreadCount()*3,(topology.vertices.size()+1023)/1024);
        const auto span=(topology.vertices.size()+tasks-1)/tasks;
        std::vector<RecordingWorkerPool::Task> work;work.reserve(tasks);
        for(std::size_t first=0;first<topology.vertices.size();first+=span){const auto last=std::min(first+span,topology.vertices.size());
            work.push_back([&,first,last](std::size_t){fill(first,last);});}
        workers->runWithCaller(std::move(work));
    }else fill(0,topology.vertices.size());
    for(const auto& primitive:topology.primitives){
        const auto triangles=primitive.range.indexCount/3;scratch.order.resize(triangles);scratch.depths.resize(triangles);
        for(std::uint32_t triangle=0;triangle<triangles;++triangle){
            scratch.order[triangle]=triangle;const auto base=triangle*3;
            const auto& v0=scratch.worldPositions[primitive.localIndices[base]];
            const auto& v1=scratch.worldPositions[primitive.localIndices[base+1]];
            const auto& v2=scratch.worldPositions[primitive.localIndices[base+2]];
            const internal::Vector3 centroid={(v0[0]+v1[0]+v2[0])*(1.0F/3.0F),
                (v0[1]+v1[1]+v2[1])*(1.0F/3.0F),(v0[2]+v1[2]+v2[2])*(1.0F/3.0F)};
            scratch.depths[triangle]=internal::dot(internal::subtract(centroid,cameraPosition),cameraForward);
        }
        stableDepthOrder(scratch.order,scratch.depths,scratch.sort);
        for(std::uint32_t triangle=0;triangle<triangles;++triangle){const auto base=primitive.range.firstIndex+scratch.order[triangle]*3;
            output[triangle*3]=topology.sourceIndices[base];output[triangle*3+1]=topology.sourceIndices[base+1];output[triangle*3+2]=topology.sourceIndices[base+2];}
        if(primitive.range.indexCount)output+=primitive.range.indexCount;
    }
}
}
