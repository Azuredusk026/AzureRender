#pragma once
#include "render/GameUiRenderer.hpp"
#include <array>
#include <optional>
#include <vector>
namespace azurerender {
struct Rect2D {float x=0,y=0,width=0,height=0;};
struct Renderer2DStatistics {std::size_t primitives=0,batches=0,vertices=0,indices=0;};
// The owner begins the shared backend frame. Texture handles are borrowed.
// Stable layer order preserves translucent composition; each batch uses one sampler.
class Renderer2D {
public:
    explicit Renderer2D(GameUiRenderer& backend):backend_(backend){}
    void sprite(Rect2D rectangle,std::array<float,4> color,Rml::TextureHandle texture=0,int layer=0);
    void line(std::array<float,2> from,std::array<float,2> to,float width,std::array<float,4> color,int layer=0);
    Renderer2DStatistics flush();
    static std::optional<std::array<float,2>> projectLabel(std::array<float,4> clip,VkExtent2D viewport);
private:
    struct Primitive {std::array<Rml::Vertex,4> vertices;Rml::TextureHandle texture;int layer;};
    void quad(std::array<std::array<float,2>,4> points,std::array<float,4> color,Rml::TextureHandle texture,int layer);
    GameUiRenderer& backend_;
    std::vector<Primitive> pending_;
};
}
