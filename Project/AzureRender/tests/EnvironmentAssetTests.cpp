#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "render/EnvironmentAsset.hpp"
#include "render/EnvironmentCubeTexture.hpp"
#include "rhi/NullRhi.hpp"
#include <OpenEXR/ImfOutputFile.h>
#include <OpenEXR/ImfHeader.h>
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfFrameBuffer.h>
#include <array>
#include <cstring>
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
        const auto cubePath = directory / "six-faces.azenv";
        {
            std::ofstream out(cubePath, std::ios::binary);
            out.write("AZENV002", 8);
            for (unsigned value : {1U, 1U, 1U})
                for (unsigned shift : {0U,8U,16U,24U}) out.put(char(value >> shift));
            for (unsigned face=0;face<6;++face)
                for (unsigned value : {0x3c00U+face*0x400U,0x3800U,0U,0x3c00U}) {
                    out.put(char(value));out.put(char(value>>8));
                }
        }
        const auto cubeImage=azurerender::loadEnvironmentImage({cubePath.string()});
        require(cubeImage.width==1 && cubeImage.height==1 && cubeImage.rgba16f.size()==24,
            "Authored cube retains all six 1x1 faces");
        for (unsigned face=0;face<6;++face)
            require(cubeImage.rgba16f[face*4]==0x3c00U+face*0x400U,
                "Authored cube retains independent final-level radiance and face order");
        require(azurerender::environmentAsCubeFaces(cubeImage).rgba16f==cubeImage.rgba16f,
            "Native cube normalization preserves authored half bits");
        azurerender::rhi::NullRhi backend;backend.collectImageUploads=true;
        {
            azurerender::EnvironmentCubeTexture texture(backend,cubeImage);
            require(texture.image().arrayLayers==6 && texture.image().mipLevels==1,
                "Cube upload retains six physical layers");
            require(backend.imageUploads.size()==1 && backend.imageUploads[0].pixels.size()==48,
                "Cube upload retains all six source faces");
            require(std::memcmp(backend.imageUploads[0].pixels.data(),cubeImage.rgba16f.data(),48)==0,
                "Cube upload preserves authored half bits without filtering");
            bool cubeView=false;
            for(const auto& call:backend.calls)
                cubeView|=call.name=="createImageView" && call.detail.find("type=3 layers=6")!=std::string::npos;
            require(cubeView,"Cube upload creates a six-layer cube view");
        }
        require(backend.allocator().statistics().liveImages==0 && backend.allocator().statistics().liveBuffers==0,
            "Cube texture owner releases image and staging allocation");
        backend.unavailableSampledFormats.insert(VK_FORMAT_R16G16B16A16_SFLOAT);
        bool unsupported=false;
        try{azurerender::EnvironmentCubeTexture texture(backend,cubeImage);}
        catch(const std::runtime_error&){unsupported=true;}
        require(unsupported && backend.allocator().statistics().liveImages==0 && backend.allocator().statistics().liveBuffers==0,
            "Unsupported HDR sampling fails before allocation");
        azurerender::EnvironmentImage legacy;
        legacy.width=8;legacy.height=4;legacy.authoredMipLevels=2;
        legacy.rgba16f.resize(8*4*4,0);
        for(unsigned y=0;y<2;++y)for(unsigned x=0;x<4;++x) {
            legacy.rgba16f.push_back(azurerender::environmentFloatToHalf(float(x)));
            legacy.rgba16f.push_back(azurerender::environmentFloatToHalf(y?20.F:10.F));
            legacy.rgba16f.push_back(0);legacy.rgba16f.push_back(0x3c00);
        }
        const auto normalized=azurerender::environmentAsCubeFaces(legacy);
        require(normalized.projection==azurerender::EnvironmentProjection::CubeFaces
            && normalized.width==2 && normalized.height==2 && normalized.authoredMipLevels==2,
            "Legacy environment preserves authored mip count in a legal cube shape");
        const std::size_t coarseOffset=2*2*6*4;
        const std::array<float,6> expectedRed{1.5F,1.5F,1.5F,1.5F,2.5F,.5F};
        const std::array<float,6> expectedGreen{15,15,10,20,15,15};
        for(unsigned face=0;face<6;++face) {
            require(normalized.rgba16f[coarseOffset+face*4]==azurerender::environmentFloatToHalf(expectedRed[face]),
                "Legacy cube uses DDS face axes and wraps longitude at the seam");
            require(normalized.rgba16f[coarseOffset+face*4+1]==azurerender::environmentFloatToHalf(expectedGreen[face]),
                "Legacy cube clamps latitude at the poles");
        }
        const auto rejectsEnvironment=[&](const auto& source) {
            try{(void)azurerender::loadEnvironmentImage(source);return false;}
            catch(const std::runtime_error&){return true;}
        };
        require(rejectsEnvironment(azurerender::SceneEnvironmentSource{cubePath.string(),azurerender::EnvironmentProjection::Equirectangular}),
            "Explicit projection mismatch must fail");
        // Corruption must be rejected before GPU allocation.
        for(const auto replacement:std::array<std::pair<unsigned,unsigned>,6>{{{8,0},{12,2},{16,2},{0,'X'},{20,0},{21,0x7c}}}) {
            auto corruptPath=directory/"corrupt-cube.azenv";
            std::ifstream in(cubePath,std::ios::binary);
            std::vector<char> bytes((std::istreambuf_iterator<char>(in)),{});
            if(replacement.first==20)bytes.pop_back();
            else bytes[replacement.first]=char(replacement.second);
            std::ofstream out(corruptPath,std::ios::binary);out.write(bytes.data(),bytes.size());out.close();
            require(rejectsEnvironment(azurerender::SceneEnvironmentSource{corruptPath.string()}),
                "Cube rejects empty or rectangular faces, excessive mips, wrong version, truncation and infinity");
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
