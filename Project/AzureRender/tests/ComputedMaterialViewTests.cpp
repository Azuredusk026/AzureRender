#include "render/ComputedMaterialView.hpp"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace azurerender;
void check(bool v,const char* message){if(!v)throw std::runtime_error(message);}
int main()try{
    ComputedMaterialViewInputs view{1,0,0,1,2};
    ComputedMaterialViewCapabilities caps{0x10de,true,true,true,false,false,true};
    check(computedMaterialViewEligible(caps,view),"Qualified ordinary view must select the optimized family");
    for(unsigned i=0;i<5;++i){
        auto changed=view;float* values[]={&changed.sceneLighting,&changed.isolationMode,&changed.effectMode,&changed.effectEnabled,&changed.reflectionSource};
        *values[i]=std::nextafter(*values[i],std::numeric_limits<float>::infinity());
        check(!computedMaterialViewEligible(caps,changed),"Eligibility must use exact uploaded values");
        *values[i]=std::numeric_limits<float>::quiet_NaN();check(!computedMaterialViewEligible(caps,changed),"NaN view input must use reference");
    }
    for(unsigned i=0;i<8;++i){auto changed=caps;
        switch(i){case 0:changed.vendorId=0x8086;break;case 1:changed.quadControl=false;break;case 2:changed.bindless=false;break;case 3:changed.singleSample=false;break;case 4:changed.referenceUv=true;break;case 5:changed.experimentalReuse=true;break;case 6:changed.depthAvailable=false;break;case 7:changed.referenceCamera=true;break;}
        check(!computedMaterialViewEligible(changed,view),"Unsupported capability combination must use reference");
    }
    unsigned mapped=0;
    for(const auto prefix:{"mesh_bindless","mesh_object_bindless","character_snapshot_bindless","character_snapshot_object_bindless"})
    for(unsigned flags=0;flags<32;++flags){
        std::string name=prefix;const std::array<const char*,5> suffixes{"_clipmap","_field","_early","_lighting","_coverage"};
        for(unsigned bit=0;bit<5;++bit)if(flags&(1U<<bit))name+=suffixes[bit];
        const auto path="D:/resources/shaders/"+name+".frag.spv";const auto next=computedMaterialViewProgram(path);
        check(next&&*next=="D:/resources/shaders/"+name+"_viewquad.frag.spv","Canonical resource policy must retain its suffixes");++mapped;
    }
    for(const auto path:{"mesh.frag.spv","mesh_bindless_samples.frag.spv","mesh_integral_bindless.frag.spv","mesh_fog_bindless.frag.spv","mesh_bindless_uvquad.frag.spv","mesh_bindless_radiance.frag.spv","mesh_bindless_early_isolation0.frag.spv","mesh_bindless_viewquad.frag.spv","mesh_bindless_coverage_lighting.frag.spv","mesh_bindless_custom.frag.spv","shadow_bindless.frag.spv","mesh_bindless.frag.spv.extra","mesh_bindless.frag"})
        check(!computedMaterialViewProgram(path),"Unknown or unsupported shader name must retain reference");
    check(computedMaterialViewProgram("D:\\shaders\\mesh_bindless.frag.spv")==std::optional<std::string>("D:\\shaders\\mesh_bindless_viewquad.frag.spv"),"Windows resource path mapping failed");
    std::cout<<"Computed material view: exact uploaded values, eight capability fallbacks, "<<mapped<<" canonical programs and thirteen name refusals passed\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
