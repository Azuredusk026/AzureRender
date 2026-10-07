#pragma once
#include "extensions/ISceneRenderer.hpp"
#include "render/RenderSettings.hpp"
#include <array>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <thread>
namespace azurerender {
using RenderViewHandle=std::uint64_t;
enum class ViewColorTransfer { Linear, SrgbEncoded };
struct RenderViewDescriptor {
    std::string rendererId;
    VkExtent2D extent{128,128};
    std::array<float,3> cameraPosition{2,1,3},cameraTarget{0,0,0};
    scene::SceneDescription scene;
    RenderSettings settings;
    bool sceneWorldCoordinates=false;
    ViewColorTransfer transfer=ViewColorTransfer::Linear;
};
class RenderViewService final {
public:
    using Factory=std::function<std::unique_ptr<ISceneRenderer>(const std::string&)>;
    RenderViewService(RenderContext,Factory,std::function<void()> waitIdle,std::size_t capacity=3);
    ~RenderViewService();
    RenderViewHandle create(const RenderViewDescriptor&);
    void request(RenderViewHandle);
    void resize(RenderViewHandle,VkExtent2D);
    void setCamera(RenderViewHandle,const std::array<float,3>&,const std::array<float,3>&);
    void schedule(RenderGraph&,const SceneFrameData&,const RenderContext&,std::uint64_t submission);
    void complete(std::uint64_t submission);
    // Every UI/capture read advances lifetime, including cached static output.
    // Pending resize returns no image until previous GPU uses complete.
    VkImageView sample(RenderViewHandle,std::uint64_t submission);
    void release(RenderViewHandle);
    nlohmann::json describe(RenderViewHandle) const;
    std::vector<unsigned char> readPixels(RenderViewHandle);
    void clear();
private:
    struct State;
    std::unique_ptr<State> state_;
};
}
