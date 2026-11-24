#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
namespace azurerender {
struct ComputedMaterialViewInputs {
    float sceneLighting=0,isolationMode=0,effectMode=0,effectEnabled=0,reflectionSource=0;
};
struct ComputedMaterialViewCapabilities {
    std::uint32_t vendorId=0;
    bool quadControl=false,bindless=false,singleSample=false,referenceUv=false,experimentalReuse=false,depthAvailable=false;
    bool referenceCamera=false;
};
// Inputs are the exact values uploaded to the current material camera buffer.
constexpr bool computedMaterialViewEligible(const ComputedMaterialViewCapabilities& caps,const ComputedMaterialViewInputs& input) noexcept {
    return caps.vendorId==0x10deU&&caps.quadControl&&caps.bindless&&caps.singleSample
        &&!caps.referenceUv&&!caps.referenceCamera&&!caps.experimentalReuse&&caps.depthAvailable
        &&input.sceneLighting==1.F&&input.isolationMode==0.F&&input.effectMode==0.F
        &&input.effectEnabled==1.F&&input.reflectionSource==2.F;
}
// Only the canonical single-sample bindless resource policies have aliases.
// Unknown/custom names retain their original program and diagnostics.
inline std::optional<std::string> computedMaterialViewProgram(std::string path){
    const auto slash=path.find_last_of("/\\");
    const auto start=slash==std::string::npos?0:slash+1;
    std::string_view name(path.data()+start,path.size()-start);
    constexpr std::string_view ending=".frag.spv";
    if(name.size()<ending.size()||name.substr(name.size()-ending.size())!=ending)return std::nullopt;
    name.remove_suffix(ending.size());
    bool found=false;
    for(const auto prefix:{std::string_view("character_snapshot_object_bindless"),std::string_view("character_snapshot_bindless"),std::string_view("mesh_object_bindless"),std::string_view("mesh_bindless")}){
        if(name.substr(0,prefix.size())==prefix){name.remove_prefix(prefix.size());found=true;break;}
    }
    if(!found)return std::nullopt;
    for(const auto suffix:{std::string_view("_clipmap"),std::string_view("_field"),std::string_view("_early"),std::string_view("_lighting"),std::string_view("_coverage")})
        if(name.substr(0,suffix.size())==suffix)name.remove_prefix(suffix.size());
    if(!name.empty())return std::nullopt;
    path.insert(path.size()-ending.size(),"_viewquad");return path;
}
}
