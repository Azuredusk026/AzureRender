#pragma once
#include "rhi/Rhi.hpp"
#include <RmlUi/Core/RenderInterface.h>
#include <filesystem>
#include <memory>
namespace azurerender {
class GameUiRenderer final: public Rml::RenderInterface {
public:
    GameUiRenderer(rhi::IRhi& rhi,VkRenderPass renderPass,const std::filesystem::path& shaders);
    ~GameUiRenderer();
    void beginFrame(std::uint64_t serial,VkExtent2D extent);
    void record(rhi::ICommandRecorder& commands);
    std::uint64_t drawCalls() const;
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices,Rml::Span<const int> indices) override;
    void RenderGeometry(Rml::CompiledGeometryHandle geometry,Rml::Vector2f translation,Rml::TextureHandle texture) override;
    void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;
    Rml::TextureHandle LoadTexture(Rml::Vector2i& dimensions,const Rml::String& source) override;
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source,Rml::Vector2i dimensions) override;
    void ReleaseTexture(Rml::TextureHandle texture) override;
    void EnableScissorRegion(bool enable) override;
    void SetScissorRegion(Rml::Rectanglei region) override;
    void SetTransform(const Rml::Matrix4f* transform) override;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
