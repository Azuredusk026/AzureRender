#pragma once
#include <filesystem>
#include <array>
#include <functional>
#include <memory>
#include <string>
namespace Rml {class RenderInterface;}
namespace azurerender {
class GameUi {
public:
    GameUi(const std::filesystem::path& document, const std::filesystem::path& font, Rml::RenderInterface& renderer);
    ~GameUi();
    void resize(int width, int height, float density);
    void setText(const std::string& id, const std::string& text);
    std::string text(const std::string& id) const;
    std::array<float,4> bounds(const std::string& id) const;
    void setActionHandler(std::function<void(std::string)> handler);
    bool pointer(int x, int y, bool down);
    bool wantsPointer(int x,int y) const;
    void update(double delta);
    void render();
    bool wantsKeyboard() const;
    void key(int key, bool down);
    void character(char32_t value);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
