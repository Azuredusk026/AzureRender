#include "render/TemporalViewState.hpp"
#include "render/MaterialSampling.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>
#include <iostream>
using namespace azurerender;
void checkTemporal(bool value,const char* why){if(!value)throw std::runtime_error(why);}
int main()try{
    constexpr auto authored=(1U<<23U)|3U;
    checkTemporal(materialSamplingFeatures(authored,false)==authored,"First-frame shading retains authored feature bits");
    checkTemporal(materialSamplingFeatures(authored,true)==(authored|kTemporalMaterialSamples),"Temporal shading adds only its reserved runtime bit");
    checkTemporal(materialSamplingFeatures(authored|kTemporalMaterialSamples,false)==authored,"History invalidation clears runtime sampling without changing authored features");
    checkTemporal(materialSamplingFeatures(authored,false,true)==(authored|kTemporalAlphaCoverage),"Coverage filtering is independent of history sampling");
    checkTemporal(materialSamplingFeatures(authored|kTemporalAlphaCoverage,false,false)==authored,"Coverage runtime policy cannot survive in authored features");
    TemporalViewState main,aux;TemporalFrameInfo a;a.width=2560;a.height=1440;a.sceneGeneration=1;
    const auto first=main.prepare(a);
    checkTemporal(first.reset==TemporalReset::First&&!first.previous&&first.sampleIndex==0,"First frame has no history");
    checkTemporal(!main.commit(0),"Submission zero cannot publish history");
    checkTemporal(main.prepare(a).sampleIndex==0,"Preparing twice cannot advance the jitter sequence");
    checkTemporal(main.commit(3),"Actual submission commits the prepared state");
    checkTemporal(!main.commit(3),"Duplicate commit cannot advance history");
    a.viewProjection[12]=.125F;a.exposureScale=2;
    const auto moved=main.prepare(a);
    checkTemporal(moved.previous&&moved.reset==TemporalReset::None&&moved.previousSubmission==3&&moved.sampleIndex==1,
        "Camera movement and exposure changes preserve the last rendered view");
    checkTemporal(moved.previous->viewProjection[12]==0&&moved.previous->exposureScale==1,"Previous values remain frozen");
    checkTemporal(main.commit(19),"Skipped global submissions still advance only once");
    auto resized=a;resized.width=1280;
    checkTemporal(main.prepare(resized).reset==TemporalReset::Extent,"Resize rejects incompatible history");
    main.cancel();
    const auto afterCancel=main.prepare(a);
    checkTemporal(afterCancel.previous&&afterCancel.sampleIndex==2&&afterCancel.previousSubmission==19,"Canceled recording preserves committed history");
    checkTemporal(!main.commit(18),"Out-of-order submission is rejected");
    checkTemporal(main.commit(20),"Rejected submission leaves the pending state available");
    auto changed=a;changed.sceneGeneration=2;
    checkTemporal(main.prepare(changed).reset==TemporalReset::Scene,"Scene generation invalidates history");
    changed=a;changed.cameraCut=1;
    checkTemporal(main.prepare(changed).reset==TemporalReset::CameraCut,"Explicit camera cuts invalidate history");
    changed=a;changed.projection[0]=2;
    checkTemporal(main.prepare(changed).reset==TemporalReset::Projection,"Lens changes invalidate history");
    checkTemporal(main.commit(21),"Reset frame can commit");
    checkTemporal(main.prepare(changed).sampleIndex==1,"Reset jitter starts a new per-view sequence");
    checkTemporal(aux.prepare(a).sampleIndex==0&&!aux.prepare(a).previous,"Auxiliary view has independent first-frame history");
    main.clear();checkTemporal(!main.prepare(a).previous,"Resource recreation drops history");
    for(int kind=0;kind<5;++kind){auto invalid=a;
        if(kind==0)invalid.width=0;if(kind==1)invalid.sceneGeneration=0;if(kind==2)invalid.exposureScale=0;
        if(kind==3)invalid.projection[3]=std::numeric_limits<float>::infinity();
        if(kind==4)invalid.viewProjection[2]=std::numeric_limits<float>::quiet_NaN();
        bool rejected=false;try{main.prepare(invalid);}catch(const std::invalid_argument&){rejected=true;}
        checkTemporal(rejected,"Invalid temporal metadata must fail before replacing a prepared frame");
    }
    checkTemporal(main.commit(22),"Invalid preparation preserves the preceding valid pending frame");
    std::array<float,2> mean{};
    for(unsigned i=0;i<16;++i){const auto jitter=temporalJitterPixels(i);
        if(i<8){mean[0]+=jitter[0];mean[1]+=jitter[1];}
        checkTemporal(std::abs(jitter[0])<=.5F&&std::abs(jitter[1])<=.5F,"Jitter remains subpixel");
        checkTemporal(jitter==temporalJitterPixels(i+8),"Eight-phase sampling has explicit period");}
    checkTemporal(std::abs(mean[0])<1e-6F&&std::abs(mean[1])<1e-6F,"Repeated temporal samples must preserve the pixel centre without persistent projection bias");
    std::cout<<"Temporal view state contract passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
