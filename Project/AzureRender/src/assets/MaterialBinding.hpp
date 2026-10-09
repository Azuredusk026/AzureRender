#pragma once
#include <map>
#include <string>
namespace azurerender {
struct MaterialBinding {
    std::string path,fingerprint;
    std::map<std::string,std::string> textures;
};
}
