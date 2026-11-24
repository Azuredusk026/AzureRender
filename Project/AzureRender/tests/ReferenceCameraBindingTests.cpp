#include "render/CameraProjection.hpp"
#include "editor/EditorCameraController.hpp"
#include "editor/viewport/EditorCameraService.hpp"
#include "render/RenderContext.hpp"
#include <iostream>
using namespace azurerender;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::invalid_argument&){caught=true;}check(caught,"Expected invalid reference calibration rejection");}
}
int main()try{
 CameraViewDescriptor camera;camera.position={0,0,0};camera.target={0,0,-1};
 ReferenceCameraView source;source.extent={2560,1440};source.projection.parameters.columns={ReferenceClipProjectionParameters::Row{1.6F,0,0,0},{0,2.5F,0,0},{0,0,0,-1},{0,0,.1F,0}};
 const auto anchor=referenceCameraPose(camera.position,camera.target,camera.up);
 camera.referenceCalibration=ReferenceCameraCalibration{source,anchor,1};const auto reference=resolveReferenceCamera(camera);check(bool(reference),"Reference binding absent");
 double maximumError=0;unsigned checked=0;
 for(int x=-8;x<=8;++x)for(int y=-5;y<=5;++y){const std::array<float,3> point{x*.1F,y*.1F,-3};const auto pixel=reference->project(point);const auto ray=cameraPickRay(camera,float(pixel[0]/2560),float(pixel[1]/1440),2560.F/1440);
  const double t=(double(point[2])-ray.origin[2])/ray.direction[2];for(unsigned a=0;a<3;++a)maximumError=std::max(maximumError,std::abs(double(ray.origin[a])+t*ray.direction[a]-point[a]));++checked;}
 check(maximumError<1.e-5,"Picking ray does not match reference raster coordinates");
 const std::array<float,3> from{-.2F,.1F,-3},to{.2F,.1F,-3};const auto segment=projectCameraSegment(camera,2560.F/1440,from,to);check(bool(segment),"Visible reference helper segment clipped");const auto pixel=reference->project(from);check(std::abs(segment->from[0]-pixel[0]/2560)<1.e-6&&std::abs(segment->from[1]-pixel[1]/1440)<1.e-6,"Helper projection differs from reference raster");
 auto moved=camera;moved.position[0]+=2;moved.target[0]+=2;const auto movedView=resolveReferenceCamera(moved);const auto movedPixel=movedView->project({from[0]+2,from[1],from[2]});check(std::abs(movedPixel[0]-pixel[0])<.001,"Reference calibration did not follow camera motion");
 auto zoom=camera;zoom.referenceCalibration->screenScale=2;const auto zoomPixel=resolveReferenceCamera(zoom)->project(from);check(std::abs((zoomPixel[0]-1280)-2*(pixel[0]-1280))<.001,"Reference screen scale ignored");
 auto disabled=camera;disabled.referenceCalibration.reset();check(!resolveReferenceCamera(disabled),"Ordinary camera acquired a reference binding");const auto ray=cameraPickRay(disabled,.5F,.5F,1);check(ray.origin==disabled.position&&ray.direction==internal::Vector3{0,0,-1},"Ordinary camera center ray changed");
 auto bad=camera;bad.referenceCalibration->screenScale=0;rejects([&]{(void)cameraPickRay(bad,.5F,.5F,1);});bad=camera;bad.referenceCalibration->anchor.right={0,0,0};rejects([&]{validateCameraView(bad,1);});
 EditorViewDescriptor editor;static_cast<CameraViewDescriptor&>(editor)=camera;editor.projection=CameraProjectionKind::Orthographic;EditorViewportInput input;input.zoomSteps=1;check(EditorCameraController::apply(input,editor)&&editor.referenceCalibration->screenScale>1,"Orthographic navigation ignored reference zoom");
 SelectionBoundsResult bounds;bounds.valid=true;bounds.bounds.minimum={-1,-1,-1};bounds.bounds.maximum={1,1,1};const auto before=editor.referenceCalibration->screenScale;check(!EditorCameraService::frameSelection(editor,bounds,1).passed&&editor.referenceCalibration->screenScale==before,"Unsupported reference framing changed calibration");
 EditorCameraService::projection(editor,CameraProjectionKind::Perspective);check(!editor.referenceCalibration,"Explicit standard projection retained reference calibration");
 SceneRendererCapabilities capabilities;rejects([&]{validateSceneRendererCamera(capabilities,camera);});capabilities.supportsReferenceCamera=true;validateSceneRendererCamera(capabilities,camera);
 std::cout<<checked<<" shared reference pick rays; maximum world error="<<maximumError<<"; raster helpers, motion, zoom, disabled and invalid bindings passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
