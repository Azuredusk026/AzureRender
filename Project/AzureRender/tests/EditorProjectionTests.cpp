#include "editor/EditorSession.hpp"
#include <iostream>
#include <cmath>
#include "render/CameraProjection.hpp"
#include "editor/viewport/EditorCameraService.hpp"
#include "editor/EditorCameraController.hpp"
using namespace azurerender;
int main(){try{
    EditorSession session(std::make_shared<EditorContext>(SceneDocument{},std::filesystem::temp_directory_path()/"azure-projection-fixture.azscene"));const auto before=session.context().documentContent();
    const auto require=[](bool result,const char* message){if(!result)throw std::runtime_error(message);};
    for(auto kind:{CameraProjectionKind::Perspective,CameraProjectionKind::Orthographic}) {
        auto shifted=session.viewportView();shifted.position={0,0,5};shifted.target={0,0,0};shifted.up={0,1,0};shifted.projection=kind;
        for(auto offset:{std::array<float,2>{0,0},std::array<float,2>{.5F,-.25F},std::array<float,2>{-1.5F,1.25F}}) {
            shifted.projectionOffsetX=offset[0];shifted.projectionOffsetY=offset[1];
            auto matrix=internal::multiply(cameraProjectionMatrix(shifted,1.5F),cameraViewMatrix(shifted));
            const float w=matrix[15];require(std::abs(matrix[12]/w-offset[0])<1e-5F&&std::abs(matrix[13]/w-offset[1])<1e-5F,
                "A projection offset moves the optical-axis point in normalized viewport coordinates");
            for(auto uv:{std::array<float,2>{.2F,.7F},std::array<float,2>{.5F,.5F}}) {
                const auto ray=cameraPickRay(shifted,uv[0],uv[1],1.5F);const auto point=internal::addVectors(ray.origin,internal::scaleVector(ray.direction,3));
                const auto screen=projectCameraSegment(shifted,1.5F,point,point);
                require(screen&&std::abs(screen->from[0]-uv[0])<1e-5F&&std::abs(screen->from[1]-uv[1])<1e-5F,"Offset picking rays must reproject into their source viewport pixel");
            }
            SelectionBoundsResult bounds{true,{{-1,-2,-.5F},{1,2,.5F}},{}};
            const auto framed=EditorCameraService::frameSelection(shifted,bounds,1.5F);require(framed.passed,"Offset camera framing must remain available");
            matrix=internal::multiply(cameraProjectionMatrix(shifted,1.5F),cameraViewMatrix(shifted));
            for(unsigned corner=0;corner<8;++corner){internal::Vector3 point;for(unsigned a=0;a<3;++a)point[a]=(corner&(1U<<a))?bounds.bounds.maximum[a]:bounds.bounds.minimum[a];
                float clip[4]{};for(unsigned row=0;row<4;++row){clip[row]=matrix[12+row];for(unsigned a=0;a<3;++a)clip[row]+=matrix[a*4+row]*point[a];}
                require(std::abs(clip[0]/clip[3])<=.951F&&std::abs(clip[1]/clip[3])<=.951F,"Offset framing must retain every selection corner inside the viewport");
            }
            shifted.position={0,0,5};shifted.target={0,0,0};
        }
    }
    for(const auto* axis:{"+X","-X","+Y","-Y","+Z","-Z"}){
        const auto oriented=session.edit("viewport.orientation",{{"axis",axis}});require(static_cast<bool>(oriented),"Six-axis orientations must use a registered production operation");
        const auto view=session.edit("viewport.describe");require(static_cast<bool>(view),"The shared view descriptor must be observable");
        require(view.value.at("projection")=="orthographic","Axis views must provide an orthographic projection");
        for(const auto& value:view.value.at("position"))require(std::isfinite(value.get<float>()),"Axis poses must remain finite");
        auto descriptor=session.viewportView();const auto forward=internal::normalize(internal::subtract(descriptor.target,descriptor.position));
        const auto right=internal::normalize(internal::cross(forward,descriptor.up));require(internal::vectorLength(right)>.99F,"Top and bottom views must retain a valid camera basis");
        for(float aspect:{.6F,1.F,2.F})for(auto kind:{CameraProjectionKind::Perspective,CameraProjectionKind::Orthographic}){
            descriptor.projection=kind;const auto combined=internal::multiply(cameraProjectionMatrix(descriptor,aspect),cameraViewMatrix(descriptor));
            for(auto uv:{std::array<float,2>{.5F,.5F},std::array<float,2>{.2F,.7F},std::array<float,2>{.9F,.1F}}){
                const auto ray=cameraPickRay(descriptor,uv[0],uv[1],aspect);const auto point=internal::addVectors(ray.origin,internal::scaleVector(ray.direction,2));
                const float w=combined[3]*point[0]+combined[7]*point[1]+combined[11]*point[2]+combined[15];
                const float x=(combined[0]*point[0]+combined[4]*point[1]+combined[8]*point[2]+combined[12])/w;
                const float y=(combined[1]*point[0]+combined[5]*point[1]+combined[9]*point[2]+combined[13])/w;
                require(std::abs((x+1)*.5F-uv[0])<1e-5F&&std::abs((y+1)*.5F-uv[1])<1e-5F,"Picking rays must reproject to the same viewport pixels in every axis and projection");
            }
            SelectionBoundsResult bounds{true,{{-1,-2,-.5F},{1,2,.5F}},{}};
            const auto framed=EditorCameraService::frameSelection(descriptor,bounds,aspect);require(framed.passed,"Both projection kinds must frame real selection bounds");
            const auto framedMatrix=internal::multiply(cameraProjectionMatrix(descriptor,aspect),cameraViewMatrix(descriptor));
            for(unsigned corner=0;corner<8;++corner){internal::Vector3 p;for(unsigned a=0;a<3;++a)p[a]=corner&(1U<<a)?bounds.bounds.maximum[a]:bounds.bounds.minimum[a];
                const float w=framedMatrix[3]*p[0]+framedMatrix[7]*p[1]+framedMatrix[11]*p[2]+framedMatrix[15];
                const float x=(framedMatrix[0]*p[0]+framedMatrix[4]*p[1]+framedMatrix[8]*p[2]+framedMatrix[12])/w,y=(framedMatrix[1]*p[0]+framedMatrix[5]*p[1]+framedMatrix[9]*p[2]+framedMatrix[13])/w;
                require(std::abs(x)<=.901F&&std::abs(y)<=.901F,"Frame selection must fit every corner within the shared projection margins");
            }
            const auto eye=descriptor.position;EditorViewportInput input;input.lookDeltaX=20;input.lookDeltaY=-20;
            require(EditorCameraController::apply(input,descriptor)&&descriptor.position==eye,"Held RMB look must preserve camera position even in top views");
        }
    }
    require(!session.edit("viewport.orientation",{{"axis","invalid"}}),"Unknown orientation must be rejected");
    const auto restored=session.edit("viewport.projection",{{"value","perspective"}});require(static_cast<bool>(restored),"Perspective must be restorable through the same view service");
    require(session.context().documentContent()==before&&!session.context().dirty(),"View configuration must preserve the scene document and its history");
    const auto speed=session.edit("viewport.fly-speed",{{"steps",2}});require(static_cast<bool>(speed),"Flight wheel speed must use a validated public operation");
    require(speed.value.get<float>()>5,"Positive flight wheel steps must increase flight speed");
    require(!session.edit("viewport.fly-speed",{{"steps",1e30}}),"Nonfinite speed calculations must be rejected");
    const auto overlays=session.edit("viewport.overlays");require(static_cast<bool>(overlays)&&overlays.value.size()>=3,"Grid, camera and light helper providers must be discoverable");
    require(static_cast<bool>(session.edit("viewport.overlay-policy",{{"category","light"},{"policy","selected"}})),"Helpers must share validated workspace visibility policies");
    require(!session.edit("viewport.overlay-policy",{{"category","light"},{"policy","invalid"}}),"Invalid helper policies must be rejected");
    CameraViewDescriptor clipView;clipView.position={0,0,0};clipView.target={0,0,-1};
    for(auto kind:{CameraProjectionKind::Perspective,CameraProjectionKind::Orthographic}){
        clipView.projection=kind;
        require(projectCameraSegment(clipView,1,{-1,0,-2},{1,0,1}).has_value(),"Helpers crossing the near plane must retain their visible segment");
        require(!projectCameraSegment(clipView,1,{0,0,1},{1,0,2}),"Helpers behind the camera must be clipped in either projection");
        require(!projectCameraSegment(clipView,1,{0,0,-101},{1,0,-102}),"Helpers beyond the far plane must be clipped");
    }
    std::cout<<"View operations, six orientations and independent workspace state passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
