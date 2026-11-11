#pragma once
#include "render/ReferenceCameraView.hpp"
#include <map>
#include <string>
namespace azurerender {
using ReferenceCameraViews=std::map<std::string,ReferenceCameraView>;
struct ReferenceCameraPose {
    std::array<float,3> position{},right{1,0,0},up{0,1,0},back{0,0,1};
};
struct ReferenceCameraCalibration {
    ReferenceCameraView view;
    ReferenceCameraPose anchor;
    float screenScale=1;
};
inline void validateReferenceCameraPose(const ReferenceCameraPose& p){
    for(const auto& v:{p.position,p.right,p.up,p.back})for(float f:v)if(!std::isfinite(f))throw std::invalid_argument("Reference camera pose must be finite");
    const auto dot=[](const auto& a,const auto& b){double value=0;for(unsigned i=0;i<3;++i)value+=double(a[i])*b[i];return value;};
    for(const auto& v:{p.right,p.up,p.back})if(std::abs(dot(v,v)-1)>1.e-4)throw std::invalid_argument("Reference camera axes must be unit vectors");
    if(std::abs(dot(p.right,p.up))>1.e-4||std::abs(dot(p.right,p.back))>1.e-4||std::abs(dot(p.up,p.back))>1.e-4)throw std::invalid_argument("Reference camera axes must be orthogonal");
    const std::array<double,3> cross{double(p.right[1])*p.up[2]-double(p.right[2])*p.up[1],double(p.right[2])*p.up[0]-double(p.right[0])*p.up[2],double(p.right[0])*p.up[1]-double(p.right[1])*p.up[0]};
    if(dot(cross,p.back)<.9999)throw std::invalid_argument("Reference camera axes must be right handed");
}
inline ReferenceCameraPose referenceCameraPose(const std::array<float,3>& position,const std::array<float,3>& target,const std::array<float,3>& inputUp){
    const auto normalize=[](std::array<double,3> v){double squared=0;for(double f:v){if(!std::isfinite(f))throw std::invalid_argument("Camera pose must be finite");squared+=f*f;}if(squared<1.e-12||!std::isfinite(squared))throw std::invalid_argument("Camera pose is degenerate");for(auto& f:v)f/=std::sqrt(squared);return v;};
    const auto cross=[](const auto& a,const auto& b){return std::array<double,3>{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};};
    const auto back=normalize({double(position[0])-target[0],double(position[1])-target[1],double(position[2])-target[2]});const auto right=normalize(cross(inputUp,back)),up=cross(back,right);
    ReferenceCameraPose p;p.position=position;for(unsigned i=0;i<3;++i){p.right[i]=float(right[i]);p.up[i]=float(up[i]);p.back[i]=float(back[i]);}validateReferenceCameraPose(p);return p;
}
inline ReferenceCameraView relocateReferenceCamera(const ReferenceCameraView& source,const ReferenceCameraPose& anchor,const ReferenceCameraPose& current){
    (void)PreparedReferenceCameraView(source);validateReferenceCameraPose(anchor);validateReferenceCameraPose(current);
    if(anchor.position==current.position&&anchor.right==current.right&&anchor.up==current.up&&anchor.back==current.back)return source;
    const std::array<std::array<float,3>,3> a{anchor.right,anchor.up,anchor.back},b{current.right,current.up,current.back};
    std::array<std::array<double,3>,3> rotation{};for(unsigned row=0;row<3;++row)for(unsigned col=0;col<3;++col)for(unsigned axis=0;axis<3;++axis)rotation[row][col]+=double(a[axis][row])*b[axis][col];
    auto result=source;result.frame={};for(unsigned i=0;i<3;++i)result.frame.parameters.offset[i]=-current.position[i];
    for(unsigned row=0;row<4;++row){
        for(unsigned col=0;col<3;++col){double value=0;for(unsigned axis=0;axis<3;++axis){const int choice=source.frame.parameters.axes[axis];value+=double(source.projection.parameters.columns[axis][row])*(choice<0?-1:1)*rotation[std::abs(choice)-1][col];}result.projection.parameters.columns[col][row]=float(value);}
        double translation=source.projection.parameters.columns[3][row];for(unsigned axis=0;axis<3;++axis){const int choice=source.frame.parameters.axes[axis];translation+=double(source.projection.parameters.columns[axis][row])*((choice<0?-1:1)*double(anchor.position[std::abs(choice)-1])+source.frame.parameters.offset[axis]);}result.projection.parameters.columns[3][row]=float(translation);
    }
    if(source.objectVisibility&&source.objectVisibility->calibration){
        const auto& authored=*source.objectVisibility->calibration;
        auto& relocated=*result.objectVisibility->calibration;
        std::array<unsigned,3> worldAxes{};
        std::array<double,3> signs{},worldOrigin{};
        for(unsigned axis=0;axis<3;++axis){
            const int choice=source.frame.parameters.axes[axis];
            worldAxes[axis]=static_cast<unsigned>(std::abs(choice)-1);
            signs[axis]=choice<0?-1:1;
            worldOrigin[worldAxes[axis]]=signs[axis]*authored.positionOffset[axis];
        }
        for(unsigned row=0;row<3;++row){
            relocated.positionOffset[row]=static_cast<float>(worldOrigin[row]);
            double translation=double(current.position[row])+worldOrigin[row];
            for(unsigned axis=0;axis<3;++axis)translation-=rotation[axis][row]*(double(anchor.position[axis])+worldOrigin[axis]);
            for(unsigned col=0;col<4;++col){
                double value=translation*authored.clipToWorld[col*4+3];
                for(unsigned axis=0;axis<3;++axis)value+=rotation[worldAxes[axis]][row]*signs[axis]*authored.clipToWorld[col*4+axis];
                relocated.clipToWorld[col*4+row]=static_cast<float>(value);
            }
        }
    }
    (void)PreparedReferenceCameraView(result);return result;
}
}
