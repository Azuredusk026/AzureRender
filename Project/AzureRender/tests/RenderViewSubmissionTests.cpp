#include "render/RenderViewService.hpp"
#include "rhi/NullRhi.hpp"
#include <iostream>
using namespace azurerender;
struct Probe {unsigned recordings=0,commits=0,cancels=0;std::uint64_t serial=0;bool fail=false;};
class ProbeRenderer final:public ISceneRenderer {
public:
    explicit ProbeRenderer(std::shared_ptr<Probe> p):p_(std::move(p)){}
    std::string_view name()const noexcept override{return "submission-probe";}
    SceneRendererCapabilities capabilities()const override{return {false,false,{"Beauty"}};}
    void onLoad(const RenderContext&)override{}
    void onUnload(const RenderContext&)override{}
    void onSwapchainRecreate(const RenderContext&)override{}
    void updateFrame(const SceneFrameData&)override{}
    void recordScene(const RenderContext&)override{++p_->recordings;if(p_->fail)throw std::runtime_error("Injected recording failure");}
    void onFrameSubmitted(std::uint64_t serial)noexcept override{++p_->commits;p_->serial=serial;}
    void onFrameAbandoned()noexcept override{++p_->cancels;}
private:std::shared_ptr<Probe> p_;
};
void checkSubmission(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Expected shader directory");
    rhi::NullRhi rhi;rhi::NullCommandRecorder commands;RenderContext base;
    base.rhi=&rhi;base.allocator=&rhi.allocator();base.device=reinterpret_cast<VkDevice>(1);base.commands=&commands;
    base.sceneColorFormat=VK_FORMAT_R16G16B16A16_SFLOAT;base.depthFormat=VK_FORMAT_D32_SFLOAT;
    base.normalFormat=VK_FORMAT_R8G8B8A8_UNORM;base.shadowFormat=VK_FORMAT_D32_SFLOAT;base.shaderDirectory=argv[1];
    std::vector<std::shared_ptr<Probe>> probes;
    RenderViewService service(base,[&](const std::string&){auto p=std::make_shared<Probe>();probes.push_back(p);return std::make_unique<ProbeRenderer>(p);},[]{});
    RenderViewDescriptor descriptor;descriptor.rendererId="probe";
    const auto a=service.create(descriptor),b=service.create(descriptor);
    SceneFrameData frame;std::string error;service.request(a);service.request(b);
    RenderGraph first;service.schedule(first,frame,base,1);
    checkSubmission(!service.submitted(1)&&probes[0]->commits==0,"Scheduling cannot publish history");
    checkSubmission(first.compile(error),"Submission graph compiles");first.execute(&commands);
    checkSubmission(probes[0]->commits==0,"Recording cannot publish history");
    bool otherThreadResult=true;std::thread otherThread([&]{otherThreadResult=service.submitted(1);});otherThread.join();
    checkSubmission(!otherThreadResult&&probes[0]->commits==0,"Commit notifications require the owner thread");
    checkSubmission(service.submitted(1)&&probes[0]->serial==1&&probes[1]->serial==1,"Successful submit publishes both rendered views");
    checkSubmission(!service.submitted(1),"Duplicate submit is rejected");service.complete(1);
    checkSubmission(service.describe(a).at("lastRenderSubmission")==1&&service.describe(a).at("submittedRenderCount")==1,
        "Diagnostics report accepted submissions separately from scheduling");
    service.request(b);RenderGraph skipped;service.schedule(skipped,frame,base,9);
    checkSubmission(skipped.compile(error),"Skipped view graph compiles");skipped.execute(&commands);service.submitted(9);service.complete(9);
    checkSubmission(probes[0]->commits==1&&probes[1]->commits==2&&probes[1]->serial==9,"Static views keep their own previous submission");
    service.request(a);probes[0]->fail=true;RenderGraph failed;service.schedule(failed,frame,base,10);checkSubmission(failed.compile(error),"Failure graph compiles");
    bool threw=false;try{failed.execute(&commands);}catch(const std::runtime_error&){threw=true;}
    checkSubmission(threw&&!service.submitted(10)&&service.cancel(10),"Failed recording can be canceled without committing history");
    checkSubmission(probes[0]->commits==1&&probes[0]->cancels==1,"Cancellation notifies only participating views");
    probes[0]->fail=false;RenderGraph retry;service.schedule(retry,frame,base,11);checkSubmission(retry.compile(error),"Retry graph compiles");retry.execute(&commands);
    service.release(a);checkSubmission(service.submitted(11)&&probes[0]->serial==11,"Retired owners receive the queued frame's commit");service.complete(11);
    service.request(b);RenderGraph discarded;service.schedule(discarded,frame,base,12);
    checkSubmission(service.cancel(12)&&!service.submitted(12),"Discarding an unexecuted graph leaves history untouched");
    RenderGraph recovered;service.schedule(recovered,frame,base,13);checkSubmission(recovered.compile(error),"Recovery graph compiles");recovered.execute(&commands);service.submitted(13);service.complete(13);
    checkSubmission(probes[1]->commits==3&&probes[1]->cancels==1,"Canceled request is restored for retry");
    // Discarded graphs retain renderer leases until their callbacks are gone.
    failed=RenderGraph{};discarded=RenderGraph{};
    service.release(b);service.complete(13);checkSubmission(rhi.allocator().statistics().liveImages==0,"Submission leases retire without resource leaks");
    std::cout<<"Render view submission contract passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
