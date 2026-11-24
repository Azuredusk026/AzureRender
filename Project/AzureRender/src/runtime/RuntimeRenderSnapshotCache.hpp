#pragma once
#include "runtime/SceneDocument.hpp"
#include "scene/SceneDescription.hpp"
#include <functional>
#include <memory>

namespace azurerender {
class RuntimeRenderSnapshotCache final {
public:
    using Prepare=std::function<scene::SceneDescription(const SceneDocument&)>;
    std::shared_ptr<const scene::SceneDescription> prepare(const SceneDocument& document,
        std::shared_ptr<const void> source,const Prepare& prepareStatic);
    void clear() noexcept;
    std::size_t pooledFrameCount() const noexcept {return frames_.size();}
private:
    bool sameLayout(const SceneDocument& document) const;
    std::shared_ptr<const void> source_;
    scene::SceneDescription base_;
    std::vector<std::size_t> parents_,lightNodes_;
    std::vector<internal::Matrix4> matrices_;
    std::vector<std::uint8_t> states_;
    std::vector<std::shared_ptr<scene::SceneDescription>> frames_;
};
}
