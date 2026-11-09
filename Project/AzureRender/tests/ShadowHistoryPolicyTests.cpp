#include "render/ShadowHistoryPolicy.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <set>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main()try{
    ShadowHistoryPrimitiveRanges ranges;std::set<std::uint32_t> identities;
    for(auto count:{6U,18U,300U,9U}){const auto base=ranges.reserve(count);require(base!=UINT32_MAX,"Valid primitive range must be allocated");
        for(unsigned triangle=0;triangle<count/3;++triangle){const auto key=ShadowHistoryPrimitiveRanges::key(base,count,0,triangle);require(key!=UINT32_MAX&&identities.insert(key).second,"Primitive identities cannot collide");}}
    require(ranges.reserve(0)==UINT32_MAX&&ranges.reserve(4)==UINT32_MAX,"Empty and incomplete triangles are rejected");
    require(ShadowHistoryPrimitiveRanges::key(0,6,0,2)==UINT32_MAX,"Out-of-range triangle is rejected");
    require(ShadowHistoryPrimitiveRanges::key(UINT32_MAX-3,3,0,0)==UINT32_MAX-3,"Last complete range fits before the sentinel");
    require(ShadowHistoryPrimitiveRanges::key(UINT32_MAX-2,3,0,0)==UINT32_MAX,"Range overflow is rejected");
    require(ShadowHistoryPrimitiveRanges::key(0,6,1,0)==UINT32_MAX,"Triangle origins require index alignment");
    ShadowHistoryPrimitiveRanges fullRange;require(fullRange.reserve(UINT32_MAX)==0&&fullRange.reserve(3)==UINT32_MAX,"Range allocator rejects exhaustion without wrapping");
    ShadowHistoryViewIdentity view;const std::vector<float> values{0,1,2};
    require(!view.begin(values,{128,96},1)&&!view.begin(values,{128,96},1),"An unsubmitted frame cannot establish history");
    view.invalidate();require(!view.begin(values,{128,96},1),"First view cannot reuse history");view.commit();
    require(view.begin(values,{128,96},1),"Only an identical submitted view can reuse history");view.commit();
    auto changed=values;changed[0]=-0.F;require(!view.begin(changed,{128,96},1),"Signed zero changes shader input bits");
    require(!view.begin(changed,{128,96},2)&&!view.begin(changed,{256,96},2),"Generation and extent changes invalidate");
    changed[0]=std::numeric_limits<float>::quiet_NaN();require(!view.begin(changed,{256,96},2)&&!view.begin(values,{256,96},2),"Invalid inputs break the history chain");
    require(!view.begin({}, {256,96},2)&&!view.begin(values,{0,96},2)&&!view.begin(values,{256,96},0),"Incomplete identity cannot qualify");
    view.commit();require(!view.begin(values,{128,96},1),"Invalid view cannot be committed as history");
    view.invalidate();require(!view.begin(values,{128,96},1),"Explicit invalidation drops prior identity");
    const internal::Vector3 low{-.2F,-.3F,-3.F},high{.4F,.5F,-1.5F};
    const internal::Matrix4 ortho{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};auto perspective=ortho;perspective[11]=-1;perspective[15]=0;
    std::mt19937 random(319);unsigned checked=0;
    for(const auto& matrix:{ortho,perspective}){
        const auto rect=shadowHistoryProjectBounds(low,high,matrix,{2048,2048});require(!rect.empty(),"Valid box projects into the cascade");
        for(unsigned sample=0;sample<2048;++sample){internal::Vector3 point{};for(unsigned axis=0;axis<3;++axis)point[axis]=std::uniform_real_distribution<float>(low[axis],high[axis])(random);
            float clip[4]{};for(unsigned row=0;row<4;++row){clip[row]=matrix[12+row];for(unsigned axis=0;axis<3;++axis)clip[row]+=matrix[axis*4+row]*point[axis];}
            const float x=(clip[0]/clip[3]*.5F+.5F)*2048,y=(clip[1]/clip[3]*.5F+.5F)*2048;
            require(x>=rect.left&&x<rect.right&&y>=rect.top&&y<rect.bottom,"Dirty rectangle must contain actual float-projected interior points");++checked;
        }
    }
    auto invalid=ortho;invalid[15]=0;const auto full=shadowHistoryProjectBounds(low,high,invalid,{2048,2048});require(full.left==0&&full.top==0&&full.right==2048&&full.bottom==2048,"Invalid homogeneous bounds dirty the full cascade");
    invalid=ortho;invalid[0]=std::numeric_limits<float>::infinity();require(shadowHistoryProjectBounds(low,high,invalid,{2048,2048}).right==2048,"Nonfinite projection dirties full cascade");
    invalid=ortho;invalid[0]=std::numeric_limits<float>::max();require(shadowHistoryProjectBounds({2,0,0},{3,1,1},invalid,{2048,2048}).right==2048,"GPU float overflow dirties full cascade");
    invalid=ortho;invalid[15]=1e-30F;require(shadowHistoryProjectBounds(low,high,invalid,{2048,2048}).right==2048,"Near-zero positive w remains conservative");
    require(shadowHistoryProjectBounds(high,low,ortho,{2048,2048}).right==2048,"Reversed bounds dirty full cascade");
    require(shadowHistoryProjectBounds({-10,-10,-3},{-9,-9,-2},ortho,{2048,2048}).empty(),"Off-atlas bounds have an empty footprint");
    ShadowHistoryDirtyRect dirty{100,100,120,120};require(dirty.intersects(85,110,16)&&!dirty.intersects(50,110,16),"Maximum filter reach covers moving-caster neighbours");
    require(dirty.intersects(std::numeric_limits<float>::quiet_NaN(),110,16)&&dirty.intersects(50,110,-1),"Unknown filter footprint uses full shading");
    require(!ShadowHistoryDirtyRect{}.intersects(0,0,16),"An empty valid rectangle admits reuse");
    dirty.include({80,150,90,160});require(dirty.left==80&&dirty.top==100&&dirty.right==120&&dirty.bottom==160,"Previous and current bounds are conservatively united");
    std::cout<<"Shadow history policy: identity ranges, bitwise views, invalidation, "<<checked<<" projected points and filter reach passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
