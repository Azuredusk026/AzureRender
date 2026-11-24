#include "render/CascadedShadow.hpp"

#include <cassert>
#include <cmath>
#include <limits>

int main() {
    const auto splits = azurerender::computeCascadeSplits(0.1F, 100.0F, 4, 0.65F);
    assert(splits.size() == 4);
    assert(splits.front() > 0.1F);
    assert(splits.back() == 100.0F);
    for (std::size_t i = 1; i < splits.size(); ++i) assert(splits[i] > splits[i - 1]);
    const auto actual=azurerender::computeCascadeDepthFit(11.75F,50.F);
    const float blockerDepth=actual.eyeDistance-24.133231F;
    if(!(blockerDepth>actual.nearPlane&&blockerDepth<actual.farPlane))
        throw std::runtime_error("Upstream scene blocker must remain inside cascade depth");
    const auto ortho=azurerender::computeCascadeDepthFit(2.F,30.F);
    if(!(ortho.eyeDistance-25.F>ortho.nearPlane))
        throw std::runtime_error("A narrow orthographic receiver must retain a distant caster");
    const auto unchanged=azurerender::computeCascadeDepthFit(64.F,10.F);
    if(std::abs(unchanged.eyeDistance-128.1F)>1e-4F||std::abs(unchanged.farPlane-262.4F)>1e-4F)
        throw std::runtime_error("Existing wider depth coverage must be retained");
    for(float invalid:{-1.F,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
        bool rejected=false;try{(void)azurerender::computeCascadeDepthFit(2.F,invalid);}catch(const std::invalid_argument&){rejected=true;}
        if(!rejected)throw std::runtime_error("Invalid caster distance must be rejected");
    }
    for(float invalid:{0.F,-1.F,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::max()}){
        bool rejected=false;try{(void)azurerender::computeCascadeDepthFit(invalid,10.F);}catch(const std::invalid_argument&){rejected=true;}
        if(!rejected)throw std::runtime_error("Invalid or overflowing cascade radius must be rejected");
    }
    return 0;
}
