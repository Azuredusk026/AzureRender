#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>
namespace azurerender {
class Scene2D {
public:
    static Scene2D parse(const nlohmann::json& document);
    static Scene2D load(const std::filesystem::path& path);
    nlohmann::json encode()const{return document_;}
    std::string toRml()const;
private:
    nlohmann::json document_;
};
}
