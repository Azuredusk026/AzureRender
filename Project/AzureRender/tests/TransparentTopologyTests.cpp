#include "render/TransparentTopology.hpp"
#include <cstring>
#include <iostream>
using namespace azurerender;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void reject(F call){bool failed=false;try{call();}catch(const std::exception&){failed=true;}check(failed,"Invalid transparent topology must fail");}
int main(){try{
    std::vector<std::uint32_t> indices={0,1,2,2,1,3,3,4,2};
    auto topology=prepareTransparentTopology(indices,20,{{0,6},{6,3}});
    check(topology.vertices==std::vector<std::uint32_t>({0,1,2,3,4}),"Vertex union must retain first-reference order");
    check(topology.primitives[0].localIndices==std::vector<std::uint32_t>({0,1,2,2,1,3}),"Local indices must preserve source triangles");
    check(topology.primitives[1].localIndices==std::vector<std::uint32_t>({3,4,2}),"Primitives must share the same compact vertex union");
    TransparentTopologyWorkspace scratch;std::vector<std::uint32_t> output(indices.size());unsigned calls=0;
    const std::array<internal::Vector3,5> positions={internal::Vector3{0,0,1},{1,0,1},{0,1,1},{1,1,4},{0,2,2}};
    writeSortedTransparentIndices(topology,[&](std::uint32_t index){++calls;return positions.at(index);},
        {0,0,0},{0,0,1},output.data(),scratch);
    check(calls==5&&scratch.worldPositions.size()==5,"Deformation must visit only distinct referenced vertices");
    check(output==std::vector<std::uint32_t>({2,1,3,0,1,2,3,4,2}),"Depth order and primitive order must preserve original index payloads");
    const auto* retained=scratch.worldPositions.data();
    writeSortedTransparentIndices(topology,[&](std::uint32_t index){return positions.at(index);},
        {0,0,0},{0,0,0},output.data(),scratch);
    check(output==indices&&scratch.worldPositions.data()==retained,"Equal depth keeps source order and workspace storage");
    auto empty=prepareTransparentTopology(indices,20,{});writeSortedTransparentIndices(empty,
        [&](std::uint32_t)->internal::Vector3{throw std::runtime_error("Empty topology requested a vertex");},{},{},nullptr,scratch);
    reject([&]{prepareTransparentTopology(indices,4,{{6,3}});});
    reject([&]{prepareTransparentTopology(indices,20,{{8,3}});});
    reject([&]{writeSortedTransparentIndices(TransparentTopology{},[&](std::uint32_t){return internal::Vector3{};},{},{},nullptr,scratch);});
    std::vector<std::uint32_t> largeIndices(8190);for(std::size_t i=0;i<largeIndices.size();++i)largeIndices[i]=static_cast<std::uint32_t>(i);
    auto large=prepareTransparentTopology(largeIndices,8192,{{0,8190}});std::vector<std::uint32_t> serial(8190),parallel(8190);
    TransparentTopologyWorkspace a,b;RecordingWorkerPool workers(4);
    const auto vertex=[](std::uint32_t index){return internal::Vector3{float(index),2,3};};
    writeSortedTransparentIndices(large,vertex,{},{0,0,1},serial.data(),a);
    writeSortedTransparentIndices(large,vertex,{},{0,0,1},parallel.data(),b,&workers);
    check(serial==parallel&&a.worldPositions==b.worldPositions&&workers.workerWakeBatches()>0,"Parallel vertex ranges must preserve exact positions and indices");
    reject([&]{writeSortedTransparentIndices(large,[&](std::uint32_t index){if(index==1234)throw std::runtime_error("vertex failure");return vertex(index);},{},{0,0,1},parallel.data(),b,&workers);});
    writeSortedTransparentIndices(large,vertex,{},{0,0,1},parallel.data(),b,&workers);
    check(parallel==serial,"Preparation workers must recover after a failed range");
    std::cout<<"Compact transparent topology, ordering and storage contracts passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
