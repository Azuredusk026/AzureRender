#include "render/ViewFogGpu.hpp"
#include "render/FogVolumeGpu.hpp"
#include "render/CameraProjection.hpp"
#include <cstring>
#include <stdexcept>
#include <vector>
namespace azurerender {
struct ViewFogGpu::State {
    rhi::IRhi& backend;
    ViewFogBinding binding;
    std::unique_ptr<FogVolumeGpu> volume;
    std::vector<rhi::GpuBuffer> frames;
    explicit State(rhi::IRhi& rhi,const ViewFogBinding& source):backend(rhi),binding(source){}
    ~State(){for(auto& buffer:frames)backend.allocator().destroyBuffer(buffer);}
};
ViewFogGpu::ViewFogGpu(rhi::IRhi& backend,const ViewFogBinding& binding,const FogVolumeData& data,unsigned frames){
    binding.validate();data.validate();
    if(!binding.enabled||frames==0||frames>8)throw std::invalid_argument("View fog requires an enabled binding and one to eight frame buffers");
    auto state=std::make_unique<State>(backend,binding);
    state->volume=std::make_unique<FogVolumeGpu>(backend,data);
    state->frames.resize(frames);
    for(auto& buffer:state->frames){
        buffer=backend.allocator().createBuffer(sizeof(PackedViewFogFrame),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
        PackedViewFogFrame initial{};
        const auto projection=binding.projection.pack();
        for(unsigned row=0;row<3;++row)initial.rows[row]=projection.rows[row];
        std::memcpy(buffer.mapped,&initial,sizeof(initial));backend.allocator().flush(buffer,0,sizeof(initial));
    }
    state_=std::move(state);
}
ViewFogGpu::~ViewFogGpu()=default;
FogViewMatch ViewFogGpu::updateFrame(unsigned frame,const CameraViewDescriptor& camera,std::array<std::uint32_t,2> viewport){
    auto& buffer=state_->frames.at(frame);
    if(viewport[0]==0||viewport[1]==0||viewport[0]>16384||viewport[1]>16384)
        throw std::invalid_argument("View fog viewport is outside its domain");
    const auto match=state_->binding.match(camera,float(viewport[0])/viewport[1]);
    PackedViewFogFrame packed{};const auto projection=state_->binding.projection.pack();
    for(unsigned row=0;row<3;++row)packed.rows[row]=projection.rows[row];
    packed.rows[3]={float(viewport[0]),float(viewport[1]),match==FogViewMatch::Matched?1.F:0.F,0};
    if(state_->binding.analytic){
        const auto& analytic=*state_->binding.analytic;const float aspect=float(viewport[0])/viewport[1];validateCameraView(camera,aspect);
        const auto forward=internal::normalize(internal::subtract(camera.target,camera.position));
        const auto right=internal::normalize(internal::cross(forward,camera.up)),up=internal::cross(right,forward);
        const bool ortho=camera.projection==CameraProjectionKind::Orthographic;
        const float halfHeight=ortho?camera.orthoHeight*.5F:std::tan(camera.fov*.5F);
        packed.rows[3][3]=1;packed.rows[4]={camera.position[0],camera.position[1],camera.position[2],ortho?1.F:0.F};
        for(unsigned i=0;i<3;++i){packed.rows[5][i]=forward[i];packed.rows[6][i]=right[i]*halfHeight*aspect;packed.rows[7][i]=up[i]*halfHeight;}
        packed.rows[8]={camera.projectionOffsetX,camera.projectionOffsetY,0,0};
        if(analytic.height){
            const auto& height=*analytic.height;packed.rows[8][2]=height.startViewDepth;packed.rows[8][3]=static_cast<float>(height.layers.size());
            for(unsigned i=0;i<height.layers.size();++i){const auto& layer=height.layers[i];packed.rows[9+2*i]={layer.density,layer.heightFalloff,layer.referenceHeight,0};packed.rows[10+2*i]={layer.scatteringColor[0],layer.scatteringColor[1],layer.scatteringColor[2],0};}
            packed.rows[13][0]=height.minimumTransmittance;packed.rows[14]=height.transmittanceBias;
        }
        if(analytic.aerial){packed.rows[13][1]=1;const auto p=analytic.aerial->pack();for(unsigned i=0;i<6;++i)packed.rows[15+i]=p.rows[i];}
    }
    std::memcpy(buffer.mapped,&packed,sizeof(packed));state_->backend.allocator().flush(buffer,0,sizeof(packed));
    return match;
}
const rhi::GpuBuffer& ViewFogGpu::parameters(unsigned frame) const{return state_->frames.at(frame);}
void ViewFogGpu::writeDescriptors(VkDescriptorSet set,unsigned frame,unsigned parameterBinding,unsigned textureBinding) const{
    const auto& buffer=parameters(frame);
    state_->backend.writeDescriptorBuffer({set,parameterBinding,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,buffer.buffer,buffer.size});
    state_->volume->writeTexture(set,textureBinding);
}
}
