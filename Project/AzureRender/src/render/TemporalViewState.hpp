#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>

namespace azurerender {
struct TemporalFrameInfo {
    std::uint32_t width=0,height=0;
    std::uint64_t sceneGeneration=0,cameraCut=0;
    std::array<float,16> projection{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    std::array<float,16> viewProjection=projection;
    float exposureScale=1;
};
enum class TemporalReset { None,First,Extent,Scene,CameraCut,Projection };
struct TemporalPreparedFrame {
    TemporalFrameInfo current;
    std::optional<TemporalFrameInfo> previous;
    TemporalReset reset=TemporalReset::First;
    std::uint64_t previousSubmission=0;
    std::uint32_t sampleIndex=0;
};
inline constexpr char kTemporalSamplingPattern[]="zero-mean-halton8-v1";
// Zero-mean Halton(2,3), eight phases. The phase follows this view's successful
// submissions, independently of frame-ring slots and simulation updates.
inline std::array<float,2> temporalJitterPixels(std::uint32_t phase) {
    constexpr std::array<std::array<float,2>,8> samples{{
        {{0,-1.F/6}},{{-.25F,1.F/6}},{{.25F,-7.F/18}},{{-.375F,-1.F/18}},
        {{.125F,5.F/18}},{{-.125F,-5.F/18}},{{.375F,1.F/18}},{{-.4375F,7.F/18}}}};
    auto value=samples[phase%8];value[0]+=7.F/128.F;return value;
}
// Owner-thread CPU state. Preparing never publishes GPU history. Resource
// owners commit only after a successful queue submission; clear on recreation.
class TemporalViewState final {
public:
    const TemporalPreparedFrame& prepare(const TemporalFrameInfo& info) {
        if(!info.width||!info.height||!info.sceneGeneration||!std::isfinite(info.exposureScale)||info.exposureScale<=0)
            throw std::invalid_argument("Temporal frame requires extent, generation and positive finite exposure");
        for(const auto& matrix:{info.projection,info.viewProjection})for(float value:matrix)
            if(!std::isfinite(value))throw std::invalid_argument("Temporal camera matrices must be finite");
        TemporalPreparedFrame next;next.current=info;
        if(committed_) {
            const auto& previous=committed_->current;
            if(previous.width!=info.width||previous.height!=info.height)next.reset=TemporalReset::Extent;
            else if(previous.sceneGeneration!=info.sceneGeneration)next.reset=TemporalReset::Scene;
            else if(previous.cameraCut!=info.cameraCut)next.reset=TemporalReset::CameraCut;
            else if(previous.projection!=info.projection)next.reset=TemporalReset::Projection;
            else {
                next.reset=TemporalReset::None;next.previous=previous;
                next.previousSubmission=lastSubmission_;
                next.sampleIndex=(committed_->sampleIndex+1)%8;
            }
        }
        pending_=std::move(next);return *pending_;
    }
    // A false result preserves both states. This hook must be safe after the
    // queue has accepted work, including duplicate or absent notifications.
    bool commit(std::uint64_t submission) noexcept {
        if(!pending_||!submission||submission<=lastSubmission_)return false;
        committed_=std::move(pending_);pending_.reset();lastSubmission_=submission;return true;
    }
    void cancel() noexcept {pending_.reset();}
    void clear() noexcept {pending_.reset();committed_.reset();}
    const std::optional<TemporalPreparedFrame>& pending() const noexcept {return pending_;}
    const std::optional<TemporalPreparedFrame>& committed() const noexcept {return committed_;}
    std::uint64_t lastSubmission() const noexcept {return lastSubmission_;}
private:
    std::optional<TemporalPreparedFrame> pending_,committed_;
    std::uint64_t lastSubmission_=0;
};
}
