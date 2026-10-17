#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "render/EnvironmentAsset.hpp"
#include <OpenEXR/ImfOutputFile.h>
#include <OpenEXR/ImfHeader.h>
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfFrameBuffer.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void writeFixture(const std::filesystem::path& path) {
    const Imath::Box2i window({-3, 7}, {-2, 7});
    Imf::Header header(2, 1);
    header.dataWindow() = window;
    for (const char* name : {"R", "G", "B"}) header.channels().insert(name, Imf::Channel(Imf::FLOAT));
    std::array<float, 2> red{4.0F, 0.25F}, green{0.5F, 8.0F}, blue{2.0F, 1.0F};
    Imf::FrameBuffer frame;
    frame.insert("R", Imf::Slice::Make(Imf::FLOAT, red.data(), window));
    frame.insert("G", Imf::Slice::Make(Imf::FLOAT, green.data(), window));
    frame.insert("B", Imf::Slice::Make(Imf::FLOAT, blue.data(), window));
    Imf::OutputFile file(path.string().c_str(), header);
    file.setFrameBuffer(frame);
    file.writePixels(1);
}
}

int main() {
    const auto directory = std::filesystem::current_path() / "environment-test-fixtures";
    try {
        std::filesystem::create_directories(directory);
        const auto path = directory / "linear.EXR";
        writeFixture(path);
        const auto image = azurerender::loadEnvironmentImage({path.string()});
        require(image.width == 2 && image.height == 1, "EXR data window dimensions");
        const std::vector<std::uint16_t> expected{0x4400, 0x3800, 0x4000, 0x3c00,
                                                0x3400, 0x4800, 0x3c00, 0x3c00};
        require(image.rgba16f == expected, "EXR preserves linear HDR, RGB order, offset and default alpha");
        const auto authored = directory / "prefiltered.azenv";
        // Two independently authored levels. The last level is HDR blue,
        // deliberately different from any average of the red base pixels.
        const auto writeAuthored = [&](unsigned count, bool truncate) {
            std::ofstream out(authored, std::ios::binary);
            out.write("AZENV001", 8);
            for (unsigned value : {2U, 1U, count})
                for (unsigned shift : {0U,8U,16U,24U}) out.put(char(value >> shift));
            const std::array<unsigned short,12> values{0x4000,0,0,0x3c00,0x4000,0,0,0x3c00,0,0,0x4800,0x3c00};
            for (unsigned i=0;i<(truncate?11U:12U);++i) {
                out.put(char(values[i])); out.put(char(values[i]>>8));
            }
        };
        writeAuthored(2, false);
        const auto prefiltered=azurerender::loadEnvironmentImage({authored.string()});
        require(prefiltered.rgba16f.size()==12 && prefiltered.rgba16f[10]==0x4800,
            "Authored HDR mip must retain its independent radiance");
        for (const bool truncated : {false,true}) {
            writeAuthored(truncated?2:3, truncated);
            bool failed=false;
            try { (void)azurerender::loadEnvironmentImage({authored.string()}); }
            catch (const std::exception&) { failed=true; }
            require(failed,"Invalid authored mip count or truncated payload must fail");
        }
        const auto broken = directory / "broken.exr";
        { std::ofstream out(broken, std::ios::binary); out << "invalid EXR"; }
        bool rejected = false;
        try { static_cast<void>(azurerender::loadEnvironmentImage({broken.string()})); }
        catch (const std::runtime_error& error) {
            const std::string message(error.what());
            rejected = message.find("EXR") != std::string::npos && message.find("broken.exr") != std::string::npos;
        }
        require(rejected, "Malformed EXR identifies format and file");
        std::filesystem::remove_all(directory);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        std::filesystem::remove_all(directory);
        return 1;
    }
}
