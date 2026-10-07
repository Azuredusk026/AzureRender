#pragma once
#include "render/IShaderHotReloader.hpp"
#include <memory>
namespace azurerender {
ShaderReloadOptions loadShaderReloadOptions(const std::filesystem::path&);
class ShaderHotReloader final : public IShaderHotReloader {
public:
    explicit ShaderHotReloader(ShaderReloadOptions);
    ~ShaderHotReloader() override;
    void requestRebuild() override;
    void poll() override;
    ShaderReloadStatus status() const override;
    std::optional<ShaderReloadCandidate> candidate() const override;
    void finishCandidate(bool,std::string) override;
    void cancel() override;
private:
    struct State;
    std::unique_ptr<State> state_;
};
}
