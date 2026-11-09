#pragma once
#include "ecs/LightInfluenceBox.hpp"
#include <array>
#include <cmath>
#include <limits>
#include <optional>

namespace azurerender {
// CPU-only box planes in view coordinates with positive forward depth.
struct ClusterLightInfluenceBox {
    std::array<std::array<double,4>,3> rows{};
    std::array<double,3> roundoffScale{};
    bool operator==(const ClusterLightInfluenceBox& other) const {return rows==other.rows && roundoffScale==other.roundoffScale;}
};

// Missing/ill-conditioned affine transforms leave radius-based assignment active.
inline std::optional<ClusterLightInfluenceBox> clusterInfluenceBox(
    const LightInfluenceBox& box, const std::array<float,16>& view) {
    if(!box.enabled)return std::nullopt;
    for(float v:view)if(!std::isfinite(v))return std::nullopt;
    if(view[3]!=0 || view[7]!=0 || view[11]!=0 || view[15]!=1)return std::nullopt;
    double inverse[3][6]{};double norm=0;
    for(unsigned r=0;r<3;++r){
        double rowNorm=0;
        for(unsigned c=0;c<3;++c){inverse[r][c]=view[c*4+r];rowNorm+=std::abs(inverse[r][c]);}
        inverse[r][r+3]=1;norm=std::max(norm,rowNorm);
    }
    for(unsigned c=0;c<3;++c){
        unsigned pivot=c;
        for(unsigned r=c+1;r<3;++r)if(std::abs(inverse[r][c])>std::abs(inverse[pivot][c]))pivot=r;
        if(std::abs(inverse[pivot][c])<1e-12)return std::nullopt;
        for(unsigned i=0;i<6;++i)std::swap(inverse[c][i],inverse[pivot][i]);
        const double divisor=inverse[c][c];for(double& v:inverse[c])v/=divisor;
        for(unsigned r=0;r<3;++r)if(r!=c){const double factor=inverse[r][c];for(unsigned i=0;i<6;++i)inverse[r][i]-=factor*inverse[c][i];}
    }
    double inverseNorm=0;std::array<double,3> translation{};
    for(unsigned r=0;r<3;++r){
        double rowNorm=0;for(unsigned c=0;c<3;++c){rowNorm+=std::abs(inverse[r][c+3]);translation[r]-=inverse[r][c+3]*view[12+c];}
        inverseNorm=std::max(inverseNorm,rowNorm);
    }
    if(!std::isfinite(norm*inverseNorm)||norm*inverseNorm>1e6)return std::nullopt;
    ClusterLightInfluenceBox result;
    for(unsigned r=0;r<3;++r){
        const auto& source=box.worldToBox[r];
        for(float v:source)if(!std::isfinite(v))return std::nullopt;
        auto& target=result.rows[r];target[3]=source[3];result.roundoffScale[r]=1+std::abs(double(source[3]));
        for(unsigned k=0;k<3;++k){
            target[3]+=source[k]*translation[k];
            result.roundoffScale[r]+=std::abs(source[k]*translation[k]);
            for(unsigned c=0;c<3;++c)target[c]+=source[k]*inverse[k][c+3]*(c==2?-1:1);
        }
        for(double v:target)if(!std::isfinite(v))return std::nullopt;
    }
    return result;
}

// Axis intervals enclose the complete cluster frustum, with outward padding.
// A separating box plane is sufficient; ambiguous intersections retain the light.
inline bool clusterIntersectsInfluenceBox(const ClusterLightInfluenceBox& box,
    const std::array<double,2>& x, const std::array<double,2>& y,
    const std::array<double,2>& depth, bool orthographic) {
    for(unsigned i=0;i<3;++i){
        const auto& p=box.rows[i];
        double low=orthographic?0:p[2],high=low;
        for(unsigned a=0;a<2;++a){const auto& range=a==0?x:y;
            const double v0=p[a]*range[0],v1=p[a]*range[1];low+=std::min(v0,v1);high+=std::max(v0,v1);}
        if(orthographic){low+=std::min(p[2]*depth[0],p[2]*depth[1]);high+=std::max(p[2]*depth[0],p[2]*depth[1]);}
        else{low=std::min(low*depth[0],low*depth[1]);high=std::max(high*depth[0],high*depth[1]);}
        low+=p[3];high+=p[3];
        const double zMagnitude=std::max(std::abs(depth[0]),std::abs(depth[1]));
        const double xyMagnitude=std::abs(p[0])*std::max(std::abs(x[0]),std::abs(x[1]))
            +std::abs(p[1])*std::max(std::abs(y[0]),std::abs(y[1]));
        const double tolerance=64*std::numeric_limits<float>::epsilon()*(box.roundoffScale[i]
            +xyMagnitude*(orthographic?1:zMagnitude)+std::abs(p[2])*zMagnitude+std::abs(p[3]));
        if(std::isfinite(low)&&std::isfinite(high)&&(low>1+tolerance||high< -1-tolerance))return false;
    }
    return true;
}
} // namespace azurerender
