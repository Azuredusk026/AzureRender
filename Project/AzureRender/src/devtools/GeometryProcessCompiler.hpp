#pragma once
#include "assets/generators/ProceduralContracts.hpp"
#include <filesystem>
namespace azurerender {
class GeometryProcessCompiler final:public IGeometryCompiler {
public:
    GeometryProcessCompiler(std::filesystem::path executable,std::filesystem::path scratchRoot);
    std::string compile(const std::string&,const GenerationRequest&) override;
private:
    std::filesystem::path executable_,scratch_;
};
}
