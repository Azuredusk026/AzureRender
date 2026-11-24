#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace azurerender {
struct ExtensionDescriptor {
    std::string id;
    std::uint32_t apiVersion = 1;
    std::vector<std::string> capabilities;
    std::vector<std::string> dependencies;
};
}
