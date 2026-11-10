#pragma once
#include "render/ReferenceCameraView.hpp"
#include "render/GpuCulling.hpp"
#include <vector>
namespace azurerender {
class PreparedReferenceRasterFrustum;
inline PreparedReferenceRasterFrustum prepareReferenceRasterFrustum(const PreparedReferenceCameraView&,const PreparedReferenceCameraView&,const std::array<float,3>&,const std::array<float,3>&,bool);
class PreparedReferenceRasterFrustum final {
    PreparedReferenceRasterFrustum()=default;
    scene::FrustumPlanes planes_;
    std::array<float,3> minimum_{},maximum_{};
    bool bounded_=true,depth_=false;
    friend PreparedReferenceRasterFrustum prepareReferenceRasterFrustum(const PreparedReferenceCameraView&,const PreparedReferenceCameraView&,const std::array<float,3>&,const std::array<float,3>&,bool);
    bool contains(const std::array<float,3>& lo,const std::array<float,3>& hi) const {
        for(unsigned i=0;i<3;++i)if(!std::isfinite(lo[i])||!std::isfinite(hi[i])||lo[i]>hi[i])throw std::invalid_argument("Raster frustum bounds must be finite and ordered");
        for(unsigned i=0;i<3;++i)if(lo[i]<minimum_[i]||hi[i]>maximum_[i])return false;return true;
    }
public:
    bool bounded() const noexcept{return bounded_;}
    bool usesDeviceDepth() const noexcept{return depth_;}
    const scene::FrustumPlanes& planes() const noexcept{return planes_;}
    bool intersects(const std::array<float,3>& lo,const std::array<float,3>& hi) const {
        return !contains(lo,hi)||scene::boundsInsideFrustum(planes_,lo,hi);
    }
    GpuCullParameters parameters(const std::vector<GpuCullBounds>& bounds) const {
        for(const auto& b:bounds)if(!contains({b.minimum[0],b.minimum[1],b.minimum[2]},{b.maximum[0],b.maximum[1],b.maximum[2]}))throw std::invalid_argument("GPU cull bounds leave prepared raster domain");
        GpuCullParameters p;p.planes=planes_.planes;p.conservativeMargin=planes_.conservativeMargin;p.referenceWPlane=planes_.referenceWPlane;validateGpuCullParameters(p);return p;
    }
};
inline PreparedReferenceRasterFrustum prepareReferenceRasterFrustum(const PreparedReferenceCameraView& reference,const PreparedReferenceCameraView& device,const std::array<float,3>& minimum,const std::array<float,3>& maximum,bool useDeviceDepth=true){
    PreparedReferenceRasterFrustum result;result.minimum_=minimum;result.maximum_=maximum;std::array<double,3> extent;
    for(unsigned i=0;i<3;++i){if(!std::isfinite(minimum[i])||!std::isfinite(maximum[i])||minimum[i]>maximum[i])throw std::invalid_argument("Invalid raster frustum domain");extent[i]=std::max(std::abs(double(minimum[i])),std::abs(double(maximum[i])));}
    const auto keepAll=[&](){result.bounded_=false;result.depth_=false;result.planes_={};result.planes_.conservativeMargin=0;result.planes_.referenceWPlane={0,0,0,0};return result;};
    const auto source=reference.conservativeClipDescription(),physical=device.conservativeClipDescription();
    const double finiteLimit=double(std::numeric_limits<float>::max())/16;
    const auto rawError=[&](const std::array<double,4>& magnitude){double sum=magnitude[3];for(unsigned i=0;i<3;++i)sum+=magnitude[i]*extent[i];return 64*double(std::numeric_limits<float>::epsilon())*sum+64*double(std::numeric_limits<float>::min());};
    for(const auto* view:{&reference,&device}){const auto& frame=view->descriptor().frame.parameters;for(unsigned axis=0;axis<3;++axis)if(extent[std::abs(frame.axes[axis])-1]+std::abs(double(frame.offset[axis]))>finiteLimit)return keepAll();const auto d=view->conservativeClipDescription();for(const auto& row:d.magnitude){double bound=row[3];for(unsigned i=0;i<3;++i)bound+=row[i]*extent[i];if(!std::isfinite(bound)||bound>finiteLimit)return keepAll();}}
    using Row=std::array<double,4>;
    struct Plane {Row linear;std::array<float,4> stored;double error;};bool valid=true;
    const auto plane=[&](const Row& coefficients,const Row& magnitude){
        double squared=0;for(unsigned i=0;i<3;++i)squared+=coefficients[i]*coefficients[i];const double length=squared>0?std::sqrt(squared):1;Plane p{};double convertedError=0;
        for(unsigned i=0;i<4;++i){p.linear[i]=coefficients[i]/length;p.stored[i]=float(p.linear[i]);if(!std::isfinite(p.stored[i]))valid=false;convertedError+=std::abs(double(p.stored[i])-p.linear[i])*(i==3?1:extent[i]);}
        p.error=rawError(magnitude)/length+convertedError;if(!std::isfinite(p.error))valid=false;return p;
    };
    const auto combine=[](const Row& a,const Row& b,double sign){Row v{};for(unsigned i=0;i<4;++i)v[i]=a[i]+sign*b[i];return v;};
    std::array<Plane,6> planes{};
    for(unsigned i=0;i<4;++i){const unsigned axis=i/2;planes[i]=plane(combine(source.linear[3],source.linear[axis],i%2?-1:1),combine(source.magnitude[3],source.magnitude[axis],1));}
    const auto refW=plane(source.linear[3],source.magnitude[3]),devW=plane(physical.linear[3],physical.magnitude[3]);
    std::array<Row,8> corners{};std::array<double,8> dw{},rw{};double minimumDeviceW=std::numeric_limits<double>::infinity(),envelope=0;
    for(unsigned c=0;c<8;++c){corners[c][3]=1;for(unsigned i=0;i<3;++i)corners[c][i]=(c&(1U<<i))?maximum[i]:minimum[i];for(unsigned i=0;i<4;++i){dw[c]+=devW.linear[i]*corners[c][i];rw[c]+=refW.linear[i]*corners[c][i];}minimumDeviceW=std::min(minimumDeviceW,dw[c]);if(std::abs(dw[c])<=devW.error)envelope=std::max(envelope,std::abs(rw[c]));}
    // Every vertex of a slab/box intersection is a box corner or an edge
    // intersection. Its reference-W envelope keeps both zero-W fallbacks.
    for(unsigned c=0;c<8;++c)for(unsigned axis=0;axis<3;++axis)if(!(c&(1U<<axis))){const unsigned other=c|(1U<<axis);const double difference=dw[other]-dw[c];if(difference!=0)for(double side:{-devW.error,devW.error}){const double t=(side-dw[c])/difference;if(t>=0&&t<=1)envelope=std::max(envelope,std::abs(rw[c]+t*(rw[other]-rw[c])));}}
    result.depth_=useDeviceDepth&&minimumDeviceW>devW.error;
    if(result.depth_){planes[4]=plane(physical.linear[2],physical.magnitude[2]);planes[5]=plane(combine(physical.linear[3],physical.linear[2],-1),combine(physical.magnitude[3],physical.magnitude[2],1));}
    else{planes[4]=plane({0,0,0,1},{0,0,0,1});planes[5]=planes[4];}
    double margin=refW.error+envelope;for(unsigned i=0;i<6;++i){result.planes_.planes[i]=planes[i].stored;margin=std::max(margin,planes[i].error);}
    result.planes_.referenceWPlane=refW.stored;result.planes_.conservativeMargin=float(margin);
    if(!valid||!std::isfinite(result.planes_.conservativeMargin))return keepAll();
    // The conversion is an upper bound even when a positive double rounds down.
    if(double(result.planes_.conservativeMargin)<margin)result.planes_.conservativeMargin=std::nextafter(result.planes_.conservativeMargin,std::numeric_limits<float>::infinity());
    if(!std::isfinite(result.planes_.conservativeMargin))return keepAll();return result;
}
}
