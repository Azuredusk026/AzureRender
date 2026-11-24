#pragma once
#include "assets/MaterialData.hpp"
namespace azurerender {
// Fixed typed connections compile registered material fields, not shader code.
class MaterialGraph {
public:
    static MaterialGraph fromMaterial(const MaterialData& material);
    static MaterialGraph parse(const nlohmann::json& document);
    nlohmann::json encode() const;
    MaterialData compile() const;
    void setField(const std::string& field, const nlohmann::json& value);
private:
    nlohmann::json document_;
};
}
