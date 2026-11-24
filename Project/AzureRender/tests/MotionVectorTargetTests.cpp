#include "render/MotionVectorTarget.hpp"
#include "rhi/NullRhi.hpp"
#include <iostream>
#include <stdexcept>
using namespace azurerender;
void checkMotionTarget(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<class F>void rejectMotionTarget(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}checkMotionTarget(failed,"Invalid motion targets must fail before allocation");}
int main()try{
    rhi::NullRhi rhi;const auto a=reinterpret_cast<VkImageView>(0x1000),b=reinterpret_cast<VkImageView>(0x2000);
    {
        MotionVectorTarget target(rhi,rhi.allocator(),{32,24},VK_FORMAT_D32_SFLOAT,{a,b});
        checkMotionTarget(target.image().width==32&&target.image().height==24,"Motion attachment preserves the requested extent");
        checkMotionTarget(target.surfaceDepthImage().width==32&&target.surfaceDepthView()!=VK_NULL_HANDLE,
            "Non-depth-writing outlines retain a separate precise color-surface depth");
        checkMotionTarget(target.view()!=VK_NULL_HANDLE&&target.renderPass()!=VK_NULL_HANDLE,"Motion attachment owns its view and pass");
        checkMotionTarget(target.framebuffer(0)!=target.framebuffer(1),"Source depth views retain independent framebuffer bindings");
        checkMotionTarget(target.byteSize()==32*24*20,"RGBA32F motion and R32F color-surface depth have an explicit byte budget");
        rejectMotionTarget([&]{target.framebuffer(2);});
        checkMotionTarget(rhi.allocator().statistics().liveImages==2,"Motion and color-surface depth are owned while hardware depth is borrowed");
    }
    checkMotionTarget(rhi.allocator().statistics().liveImages==0,"Motion attachment releases its image");
    unsigned destroyedViews=0,destroyedPasses=0,destroyedFramebuffers=0;
    for(const auto& call:rhi.calls){destroyedViews+=call.name=="destroyImageView";destroyedPasses+=call.name=="destroyRenderPass";destroyedFramebuffers+=call.name=="destroyFramebuffer";}
    checkMotionTarget(destroyedViews==2&&destroyedPasses==1&&destroyedFramebuffers==2,"Only owned motion resources are destroyed");
    const auto before=rhi.calls.size();
    rejectMotionTarget([&]{MotionVectorTarget t(rhi,rhi.allocator(),{0,24},VK_FORMAT_D32_SFLOAT,{a});});
    rejectMotionTarget([&]{MotionVectorTarget t(rhi,rhi.allocator(),{32,24},VK_FORMAT_UNDEFINED,{a});});
    rejectMotionTarget([&]{MotionVectorTarget t(rhi,rhi.allocator(),{32,24},VK_FORMAT_R8_UNORM,{a});});
    rejectMotionTarget([&]{MotionVectorTarget t(rhi,rhi.allocator(),{32,24},VK_FORMAT_D32_SFLOAT,{});});
    rejectMotionTarget([&]{MotionVectorTarget t(rhi,rhi.allocator(),{32,24},VK_FORMAT_D32_SFLOAT,{a,VK_NULL_HANDLE});});
    rejectMotionTarget([&]{MotionVectorTarget t(rhi,rhi.allocator(),{UINT32_MAX,UINT32_MAX},VK_FORMAT_D32_SFLOAT,{a});});
    checkMotionTarget(rhi.calls.size()==before&&rhi.allocator().statistics().liveImages==0,"Invalid shapes fail before any RHI creation");
    rhi.unavailableSampledFormats.insert(MotionVectorTarget::kFormat);
    rejectMotionTarget([&]{MotionVectorTarget t(rhi,rhi.allocator(),{32,24},VK_FORMAT_D32_SFLOAT,{a});});
    checkMotionTarget(rhi.allocator().statistics().liveImages==0,"Unsupported motion format leaves no allocations");
    std::cout<<"Motion target precision, source binding, ownership and input checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
