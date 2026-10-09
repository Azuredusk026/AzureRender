#pragma once
#include "editor/EditorContext.hpp"
#include "runtime/AnimationStateMachine.hpp"
#include "assets/GltfLoader.hpp"
#include "editor/viewport/ViewportOverlayRegistry.hpp"
namespace azurerender {
class AnimationPreviewService {
public:
    void seek(EditorContext&,const std::string& node,const std::string& state,double time,const std::string& previous={},double crossfade=0);
    void play(EditorContext&,const std::string& node);
    void pause(const std::string& node);
    void clear(const std::string& node={});
    void advance(EditorContext&,double delta);
    void configure(EditorContext&,const std::string& node,double speed,bool loop);
    std::vector<ViewportOverlayLine> skeletonLines(EditorContext&);
    nlohmann::json describe() const;
    std::vector<NodeAnimationFrame> frames() const;
private:
    struct Track {AnimationStateMachine machine;std::array<float,2> morph;bool playing=false;double crossfade=.18;std::string previous;double speed=1;bool loop=true;std::shared_ptr<LoadedAsset> skeleton;};
    std::map<std::string,Track> tracks_;
    std::uint64_t revision_=0,assetVersion_=0;
    std::string fingerprint_;
    void synchronize(EditorContext&);
};
}
