#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace azurerender {
using MotionMatrix=std::array<float,16>;
inline MotionMatrix motionIdentityMatrix(){return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
struct MotionObjectIdentity {
    std::string node;
    std::uint32_t geometry=0,replica=0;
    bool operator<(const MotionObjectIdentity&) const noexcept;
};
// Geometry tokens are stable within a resource generation. Resource replacement
// clears the history. Replica identifies deliberate repeated occurrences.
struct MotionPoseState {
    MotionObjectIdentity identity;
    MotionMatrix model=motionIdentityMatrix();
    std::vector<MotionMatrix> joints;
    std::vector<float> morphWeights;
    std::array<float,2> legacyMorph{};
    std::uint32_t vertexCount=0,morphTableBase=0;
};
struct MotionPoseView {
    std::array<float,3> cameraPosition{};
    float outlinePixels=1;
    MotionMatrix gizmo=motionIdentityMatrix();
    std::uint32_t selectedPrimitive=UINT32_MAX;
};
struct PackedMotionPoseFrame {
    static constexpr std::uint32_t kVersion=1,kHeaderWords=28,kRecordWords=40;
    std::vector<std::uint32_t> words;
    std::uint32_t validObjects=0;
};
// Owner-thread CPU snapshots; GPU buffers belong to the consuming motion pass.
// stage/pack may fail without publishing history. commit never allocates.
class MotionPoseHistory final {
public:
    void stage(std::vector<MotionPoseState>,const MotionPoseView&);
    PackedMotionPoseFrame pack(const MotionMatrix& previousViewProjection,
        std::uint64_t previousCameraSubmission,std::uint64_t byteLimit) const;
    bool commit(std::uint64_t submission) noexcept;
    void cancel() noexcept {pending_.reset();}
    void clear() noexcept {pending_.reset();committed_.reset();}
    std::uint64_t lastSubmission() const noexcept {return lastSubmission_;}
private:
    struct Snapshot;
    std::shared_ptr<const Snapshot> pending_,committed_;
    std::uint64_t lastSubmission_=0;
};
}
