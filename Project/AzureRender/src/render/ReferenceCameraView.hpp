#pragma once
#include "render/ReferencePositionFrame.hpp"
#include "render/ReferenceClipProjection.hpp"
#include "render/ReferenceDepthConvention.hpp"
#include <algorithm>
#include <limits>
#include <optional>
#include <vector>
namespace azurerender {
inline constexpr unsigned kReferenceObjectVisibilityRecordCapacity=8;
struct SceneReferenceObjectVisibilityRecordCalibration {
    unsigned atlasPage=0;
    std::array<float,16> anchorWorldToUnit{},referenceToUnit{};
    std::array<float,3> lightDirection{0,1,0};
};
struct SceneReferenceObjectVisibilityCalibration {
    std::uint32_t schemaVersion=1;
    std::array<float,3> positionOffset{};
    std::array<float,16> clipToWorld{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    std::vector<SceneReferenceObjectVisibilityRecordCalibration> objectRecords;
};
struct SceneReferenceObjectVisibilitySettings {
    std::array<float,2> pixelOffset{.5F,.5F};
    std::optional<SceneReferenceObjectVisibilityCalibration> calibration;
};
struct ReferenceOutlineDepthProjection {
    std::uint32_t schemaVersion=0;
    float clipWSlope=0;
    float clipZIntercept=0;
};
static_assert(sizeof(ReferenceOutlineDepthProjection)==3*sizeof(std::uint32_t));
struct ReferenceCameraView {
    std::uint32_t schemaVersion=1;
    ReferenceOutlineDepthProjection outlineDepthProjection;
    ReferencePositionFrame frame;
    ReferenceClipProjectionProfile projection;
    std::array<unsigned,2> extent{1,1};
    ReferenceDepthConvention depthConvention=ReferenceDepthConvention::Reversed;
    bool flipNdcY=false;
    bool produceSceneDepth=false;
    bool produceSceneVisibilityInputs=false;
    std::optional<SceneReferenceObjectVisibilitySettings> objectVisibility;
};
struct ReferenceCameraRay {std::array<float,3> origin,direction;};
struct ReferenceClipBoundsDescription {
    using Matrix=std::array<std::array<double,4>,4>;
    Matrix linear{},magnitude{};
};
class PreparedReferenceCameraView final {
    using Matrix=std::array<std::array<double,4>,4>;
    ReferenceCameraView view_;Matrix matrix_{},inverse_{},magnitude_{};
    std::array<std::array<double,4>,6> planes_{};
    static void require(bool value,const char* message){if(!value)throw std::invalid_argument(message);}
    static double norm(const Matrix& m){double result=0;for(const auto& row:m){double sum=0;for(double v:row)sum+=std::abs(v);result=std::max(result,sum);}return result;}
    static Matrix invert(const Matrix& matrix){
        std::array<std::array<double,8>,4> augmented{};
        for(unsigned row=0;row<4;++row){for(unsigned c=0;c<4;++c)augmented[row][c]=matrix[row][c];augmented[row][row+4]=1;}
        for(unsigned c=0;c<4;++c){unsigned pivot=c;for(unsigned row=c+1;row<4;++row)if(std::abs(augmented[row][c])>std::abs(augmented[pivot][c]))pivot=row;
            require(augmented[pivot][c]!=0,"Reference camera matrix is singular");std::swap(augmented[c],augmented[pivot]);const double divisor=augmented[c][c];
            for(auto& value:augmented[c])value/=divisor;
            for(unsigned row=0;row<4;++row)if(row!=c){const double factor=augmented[row][c];for(unsigned k=0;k<8;++k)augmented[row][k]-=factor*augmented[c][k];}}
        Matrix inverse{};
        for(unsigned row=0;row<4;++row)for(unsigned c=0;c<4;++c){inverse[row][c]=augmented[row][c+4];require(std::isfinite(inverse[row][c]),"Reference inverse matrix is nonfinite");}
        const double condition=norm(matrix)*norm(inverse);
        require(std::isfinite(condition)&&condition<=1.e12,"Reference camera matrix condition exceeds1e12");
        return inverse;
    }
    static void validateObjectProjection(const std::array<float,16>& projection){
        require(projection[3]==0&&projection[7]==0&&projection[11]==0&&projection[15]==1,
            "Scene object calibration projection must be affine");
        Matrix matrix{};
        for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col){
            const float value=projection[col*4+row];
            require(std::isfinite(value),"Scene object calibration projection must be finite");
            matrix[row][col]=value;
        }
        (void)invert(matrix);
    }
    static void validateVisibilityCalibration(const SceneReferenceObjectVisibilityCalibration& calibration){
        require(calibration.schemaVersion==1,"Unsupported scene object visibility calibration version");
        for(const float value:calibration.positionOffset)require(std::isfinite(value),"Scene object visibility origin must be finite");
        Matrix matrix{};
        for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col){
            const float value=calibration.clipToWorld[col*4+row];
            require(std::isfinite(value),"Scene object visibility inverse matrix must be finite");
            matrix[row][col]=value;
        }
        (void)invert(matrix);
        std::array<bool,kReferenceObjectVisibilityRecordCapacity> usedPages{};
        for(const auto& record:calibration.objectRecords){
            require(record.atlasPage<usedPages.size()&&!usedPages[record.atlasPage],
                "Scene object calibration atlas pages must be unique and in range");
            usedPages[record.atlasPage]=true;
            validateObjectProjection(record.anchorWorldToUnit);
            validateObjectProjection(record.referenceToUnit);
            float squared=0;
            for(float component:record.lightDirection)squared+=component*component;
            require(squared>=.99F&&squared<=1.01F,"Scene object calibration light direction must have unit length");
        }
    }
    std::array<double,3> unprojectDouble(double u,double v,double depth) const {
        require(std::isfinite(u)&&std::isfinite(v)&&std::isfinite(depth)&&depth>=0&&depth<=1,"Invalid reference viewport coordinates or depth");
        const std::array<double,4> clip{2*u-1,2*v-1,depth,1};std::array<double,4> world{};
        for(unsigned row=0;row<4;++row){for(unsigned c=0;c<4;++c)world[row]+=inverse_[row][c]*clip[c];require(std::isfinite(world[row]),"Reference inverse projection overflows");}
        require(world[3]!=0,"Reference inverse projection is at infinity");std::array<double,3> result;
        for(unsigned i=0;i<3;++i){result[i]=world[i]/world[3];require(std::isfinite(result[i]),"Reference world point overflows");}return result;
    }
public:
    explicit PreparedReferenceCameraView(const ReferenceCameraView& view):view_(view){
        require(view.schemaVersion==1,"Unsupported reference camera view version");
        require(view.extent[0]&&view.extent[1]&&view.extent[0]<=8192&&view.extent[1]<=8192,"Reference view extent must be1..8192");
        require(view.depthConvention==ReferenceDepthConvention::Forward||view.depthConvention==ReferenceDepthConvention::Reversed,"Unknown reference camera depth convention");
        require(!view.produceSceneDepth||view.depthConvention==ReferenceDepthConvention::Reversed,
            "Scene reference depth requires reversed depth");
        require(!view.produceSceneVisibilityInputs||view.produceSceneDepth,
            "Scene visibility inputs require scene reference depth");
        const auto& outline=view.outlineDepthProjection;
        require(outline.schemaVersion<=1,"Unsupported reference outline depth projection version");
        if(outline.schemaVersion==1){
            require(view.produceSceneDepth,"Reference outline projection requires scene reference depth");
            require(std::isfinite(outline.clipWSlope)&&std::isfinite(outline.clipZIntercept)&&outline.clipZIntercept>0,
                "Reference outline depth projection requires finite coefficients and a positive clip Z intercept");
        }else{
            require(outline.clipWSlope==0&&outline.clipZIntercept==0,"Disabled reference outline depth projection coefficients must be zero");
        }
        if(view.objectVisibility){
            require(view.produceSceneVisibilityInputs,"Scene object visibility requires scene visibility inputs");
            for(const float offset:view.objectVisibility->pixelOffset)
                require(std::isfinite(offset)&&std::abs(offset)<=1.F,"Scene object visibility pixel offset must be in [-1,1]");
        }
        validateReferencePositionFrame(view.frame);validateReferenceClipProjection(view.projection);const auto& p=view.projection.parameters;
        for(unsigned row=0;row<4;++row){
            matrix_[row][3]=p.columns[3][row];magnitude_[row][3]=std::abs(double(p.columns[3][row]));
            for(unsigned axis=0;axis<3;++axis){const int choice=view.frame.parameters.axes[axis];const unsigned source=static_cast<unsigned>((choice<0?-choice:choice)-1);const double coefficient=p.columns[axis][row],offset=view.frame.parameters.offset[axis];
                matrix_[row][source]+=(choice<0?-coefficient:coefficient);matrix_[row][3]+=coefficient*offset;
                magnitude_[row][source]+=std::abs(coefficient);magnitude_[row][3]+=std::abs(coefficient)*std::abs(offset);}
        }
        for(unsigned c=0;c<4;++c){matrix_[0][c]-=2*double(p.jitter[0])*matrix_[3][c];matrix_[1][c]+=2*double(p.jitter[1])*matrix_[3][c];
            magnitude_[0][c]+=2*std::abs(double(p.jitter[0]))*magnitude_[3][c];magnitude_[1][c]+=2*std::abs(double(p.jitter[1]))*magnitude_[3][c];if(view.flipNdcY)matrix_[1][c]=-matrix_[1][c];}
        inverse_=invert(matrix_);
        if(view.objectVisibility){
            if(view.objectVisibility->calibration)validateVisibilityCalibration(*view.objectVisibility->calibration);
            else (void)clipToReference();
        }
        for(unsigned c=0;c<4;++c){planes_[0][c]=matrix_[3][c]+matrix_[0][c];planes_[1][c]=matrix_[3][c]-matrix_[0][c];planes_[2][c]=matrix_[3][c]+matrix_[1][c];planes_[3][c]=matrix_[3][c]-matrix_[1][c];planes_[4][c]=matrix_[2][c];planes_[5][c]=matrix_[3][c]-matrix_[2][c];}
    }
    const ReferenceCameraView& descriptor() const noexcept{return view_;}
    ReferenceClipBoundsDescription conservativeClipDescription() const noexcept{return {matrix_,magnitude_};}
    std::array<float,16> clipToReference() const {
        std::array<float,16> result{};
        for(unsigned row=0;row<3;++row){
            const int axis=view_.frame.parameters.axes[row];
            const auto source=static_cast<unsigned>(std::abs(axis)-1);
            const double sign=axis<0?-1:1;
            for(unsigned col=0;col<4;++col){
                const double value=sign*inverse_[source][col]+double(view_.frame.parameters.offset[row])*inverse_[3][col];
                result[col*4+row]=static_cast<float>(value);
            }
        }
        for(unsigned col=0;col<4;++col)result[col*4+3]=static_cast<float>(inverse_[3][col]);
        for(const float value:result)require(std::isfinite(value),"Reference clip inverse exceeds binary32");
        return result;
    }
    std::array<float,4> clip(const std::array<float,3>& world) const {
        auto result=projectReferenceClip(view_.projection,referenceRelativePosition(view_.frame,world));if(view_.flipNdcY)result[1]=-result[1];return result;
    }
    std::array<double,3> project(const std::array<float,3>& world) const {
        const auto p=clip(world);require(p[3]!=0,"Reference pixel projection has zero W");const double w=p[3];
        return {(double(p[0])/w+1)*.5*view_.extent[0],(double(p[1])/w+1)*.5*view_.extent[1],double(p[2])/w};
    }
    std::array<float,3> unproject(double u,double v,double depth) const {
        const auto point=unprojectDouble(u,v,depth);std::array<float,3> result;for(unsigned i=0;i<3;++i){result[i]=float(point[i]);require(std::isfinite(result[i]),"Reference world point exceeds binary32");}return result;
    }
    ReferenceCameraRay pickRay(double u,double v) const {
        const auto nearPoint=unprojectDouble(u,v,view_.depthConvention==ReferenceDepthConvention::Reversed?1:0),middle=unprojectDouble(u,v,.5);std::array<double,3> direction;double squared=0;
        for(unsigned i=0;i<3;++i){direction[i]=middle[i]-nearPoint[i];squared+=direction[i]*direction[i];}require(squared>0&&std::isfinite(squared),"Reference pick ray is degenerate");
        ReferenceCameraRay result;for(unsigned i=0;i<3;++i){result.origin[i]=float(nearPoint[i]);result.direction[i]=float(direction[i]/std::sqrt(squared));require(std::isfinite(result.origin[i])&&std::isfinite(result.direction[i]),"Reference pick ray exceeds binary32");}return result;
    }
    bool intersectsBounds(const std::array<float,3>& minimum,const std::array<float,3>& maximum) const {
        std::array<double,3> extent;
        for(unsigned i=0;i<3;++i){require(std::isfinite(minimum[i])&&std::isfinite(maximum[i])&&minimum[i]<=maximum[i],"Invalid reference camera bounds");extent[i]=std::max(std::abs(double(minimum[i])),std::abs(double(maximum[i])));}
        std::array<double,4> errors{};
        // Projection devices may flush subnormal intermediate values to zero.
        for(unsigned row=0;row<4;++row){double magnitude=magnitude_[row][3];for(unsigned i=0;i<3;++i)magnitude+=magnitude_[row][i]*extent[i];errors[row]=64*std::numeric_limits<float>::epsilon()*magnitude+64*double(std::numeric_limits<float>::min());}
        const std::array<double,6> margins{errors[3]+errors[0],errors[3]+errors[0],errors[3]+errors[1],errors[3]+errors[1],errors[2],errors[3]+errors[2]};
        for(unsigned plane=0;plane<6;++plane){double maximumValue=planes_[plane][3];for(unsigned i=0;i<3;++i)maximumValue+=planes_[plane][i]*(planes_[plane][i]>=0?maximum[i]:minimum[i]);if(maximumValue < -margins[plane])return false;}return true;
    }
};
}
