#pragma once

#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace azurerender {

// Reads a whole file as bytes. Throws std::runtime_error when unreadable.
[[nodiscard]] inline std::vector<char> readBinaryFile(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file) {
        throw std::runtime_error("Unable to open file: " + path);
    }
    const auto fileSize = static_cast<std::size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    return buffer;
}

}  // namespace azurerender
