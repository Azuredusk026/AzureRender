#include "MotionPoseData.hpp"
#include "LinearMath.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <stdexcept>
#include <tuple>
namespace azurerender {
namespace {
template<class T>bool finiteMotion(const T& values){return std::all_of(values.begin(),values.end(),[](float value){return std::isfinite(value);});}
struct alignas(16) PoseHeader {
    std::array<std::uint32_t,4> layout{};
    std::array<float,4> camera{};
    MotionMatrix gizmo{};
    std::array<std::uint32_t,4> data{};
};
struct alignas(16) PoseRecord {
    MotionMatrix model{},modelViewProjection{};
    std::array<std::uint32_t,4> data{};
    std::array<float,2> legacy{};
    std::uint32_t jointCount=0,weightCount=0;
};
static_assert(sizeof(PoseHeader)==PackedMotionPoseFrame::kHeaderWords*4);
static_assert(sizeof(PoseRecord)==PackedMotionPoseFrame::kRecordWords*4);
bool compatiblePose(const MotionPoseState& a,const MotionPoseState& b){
    return a.vertexCount==b.vertexCount&&a.joints.size()==b.joints.size()
        &&a.morphWeights.size()==b.morphWeights.size()&&a.morphTableBase==b.morphTableBase;
}
}
bool MotionObjectIdentity::operator<(const MotionObjectIdentity& other) const noexcept {
    return std::tie(node,geometry,replica)<std::tie(other.node,other.geometry,other.replica);
}
struct MotionPoseHistory::Snapshot {
    std::vector<MotionPoseState> poses;
    std::map<MotionObjectIdentity,std::size_t> index;
    MotionPoseView view;
};
void MotionPoseHistory::stage(std::vector<MotionPoseState> poses,const MotionPoseView& view){
    if(!finiteMotion(view.cameraPosition)||!finiteMotion(view.gizmo)||!std::isfinite(view.outlinePixels)||view.outlinePixels<0||view.outlinePixels>8)
        throw std::invalid_argument("Motion view requires finite camera/gizmo and outline width within [0,8]");
    if(poses.size()>std::numeric_limits<std::uint32_t>::max())throw std::length_error("Motion instance count exceeds word addressing");
    auto snapshot=std::make_shared<Snapshot>();snapshot->poses=std::move(poses);snapshot->view=view;
    for(std::size_t i=0;i<snapshot->poses.size();++i){
        const auto& pose=snapshot->poses[i];
        if(pose.identity.node.empty()||!pose.vertexCount||pose.joints.empty()
            ||!finiteMotion(pose.model)||!finiteMotion(pose.legacyMorph)||!finiteMotion(pose.morphWeights)
            ||(pose.morphTableBase==0)!=pose.morphWeights.empty())
            throw std::invalid_argument("Motion pose requires stable identity, finite transforms and compatible morph inputs");
        for(const auto& joint:pose.joints)if(!finiteMotion(joint))throw std::invalid_argument("Motion joints must be finite");
        if(!snapshot->index.emplace(pose.identity,i).second)throw std::invalid_argument("Duplicate motion object identity");
    }
    pending_=std::move(snapshot);
}
PackedMotionPoseFrame MotionPoseHistory::pack(const MotionMatrix& previousViewProjection,
    std::uint64_t previousCameraSubmission,std::uint64_t byteLimit) const {
    if(!pending_)throw std::logic_error("Motion packing requires a staged frame");
    if(!finiteMotion(previousViewProjection))throw std::invalid_argument("Previous motion projection must be finite");
    const std::uint64_t wordLimit=std::min<std::uint64_t>(byteLimit/4,std::numeric_limits<std::uint32_t>::max());
    std::uint64_t size=PackedMotionPoseFrame::kHeaderWords;
    const auto add=[&](std::uint64_t count){if(size>wordLimit||count>wordLimit-size)throw std::length_error("Motion pose buffer exceeds storage capacity");size+=count;};
    if(pending_->poses.size()>wordLimit/PackedMotionPoseFrame::kRecordWords)throw std::length_error("Motion records exceed storage capacity");
    add(pending_->poses.size()*PackedMotionPoseFrame::kRecordWords);
    const auto jointsBegin=size;
    std::vector<const MotionPoseState*> matches(pending_->poses.size(),nullptr);
    if(committed_&&previousCameraSubmission&&previousCameraSubmission==lastSubmission_){
        for(std::size_t i=0;i<matches.size();++i){
            const auto& current=pending_->poses[i];const auto found=committed_->index.find(current.identity);
            if(found!=committed_->index.end()){
                const auto& old=committed_->poses[found->second];
                if(compatiblePose(current,old)){
                    if(old.joints.size()>wordLimit/16)throw std::length_error("Motion joints exceed storage capacity");
                    add(old.joints.size()*16);matches[i]=&old;
                }
            }
        }
    }
    const auto weightsBegin=size;
    for(const auto* match:matches)if(match)add(match->morphWeights.size());
    PackedMotionPoseFrame result;result.words.resize(static_cast<std::size_t>(size),0);
    PoseHeader header;header.layout={PackedMotionPoseFrame::kVersion,static_cast<std::uint32_t>(matches.size()),
        PackedMotionPoseFrame::kHeaderWords,PackedMotionPoseFrame::kRecordWords};
    header.data={static_cast<std::uint32_t>(size),static_cast<std::uint32_t>(jointsBegin),static_cast<std::uint32_t>(weightsBegin),UINT32_MAX};
    if(committed_&&previousCameraSubmission&&previousCameraSubmission==lastSubmission_){
        const auto& view=committed_->view;header.camera={view.cameraPosition[0],view.cameraPosition[1],view.cameraPosition[2],view.outlinePixels};
        header.gizmo=view.gizmo;header.data[3]=view.selectedPrimitive;
    }else header.gizmo=motionIdentityMatrix();
    std::memcpy(result.words.data(),&header,sizeof(header));
    auto jointCursor=static_cast<std::uint32_t>(jointsBegin),weightCursor=static_cast<std::uint32_t>(weightsBegin);
    for(std::size_t i=0;i<matches.size();++i){
        PoseRecord record;
        if(const auto* previous=matches[i]){
            record.model=previous->model;record.modelViewProjection=internal::multiply(previousViewProjection,previous->model);
            if(!finiteMotion(record.modelViewProjection))throw std::invalid_argument("Previous motion projection multiplication overflowed");
            record.data={jointCursor,weightCursor,previous->vertexCount,1};
            record.legacy=previous->legacyMorph;
            record.jointCount=static_cast<std::uint32_t>(previous->joints.size());
            record.weightCount=static_cast<std::uint32_t>(previous->morphWeights.size());
            const auto jointBytes=previous->joints.size()*sizeof(MotionMatrix),weightBytes=previous->morphWeights.size()*sizeof(float);
            std::memcpy(result.words.data()+jointCursor,previous->joints.data(),jointBytes);
            if(weightBytes)std::memcpy(result.words.data()+weightCursor,previous->morphWeights.data(),weightBytes);
            jointCursor+=static_cast<std::uint32_t>(previous->joints.size()*16);
            weightCursor+=static_cast<std::uint32_t>(previous->morphWeights.size());++result.validObjects;
        }
        std::memcpy(result.words.data()+PackedMotionPoseFrame::kHeaderWords+i*PackedMotionPoseFrame::kRecordWords,&record,sizeof(record));
    }
    return result;
}
bool MotionPoseHistory::commit(std::uint64_t submission) noexcept {
    if(!pending_||!submission||submission<=lastSubmission_)return false;
    committed_=std::move(pending_);lastSubmission_=submission;return true;
}
}
