#pragma once
#include <filesystem>
#include <vector>
namespace azurerender {
struct FileDropEvent {std::vector<std::filesystem::path> paths;double x=0,y=0;};
}
