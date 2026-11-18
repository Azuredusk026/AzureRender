#include <d3d11.h>
#include <wrl/client.h>
#include <zlib.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;
constexpr UINT vertexSize = 8 * sizeof(float);
enum class Pass { Sky, Cloud, Fog, FogVolume };

void require(HRESULT status, const char* operation) {
    if (FAILED(status)) {
        std::cerr << operation << " returned 0x" << std::hex << static_cast<unsigned>(status) << '\n';
        throw std::runtime_error(operation);
    }
}

std::vector<std::byte> read(const fs::path& file) {
    std::ifstream stream(file, std::ios::binary | std::ios::ate);
    const auto bytes = stream.tellg();
    if (bytes <= 0 || bytes > 512LL * 1024 * 1024) throw std::runtime_error("Invalid source payload size");
    std::vector<std::byte> data(static_cast<std::size_t>(bytes));
    stream.seekg(0);
    stream.read(reinterpret_cast<char*>(data.data()), bytes);
    if (!stream) throw std::runtime_error("Incomplete source payload");
    return data;
}

ComPtr<ID3D11Buffer> buffer(ID3D11Device& device, const fs::path& file, UINT binding, UINT expected = 0) {
    const auto data = read(file);
    if (expected && data.size() != expected) throw std::runtime_error("Source constant buffer size differs");
    D3D11_BUFFER_DESC layout{};
    layout.ByteWidth = static_cast<UINT>(data.size());
    layout.Usage = D3D11_USAGE_IMMUTABLE;
    layout.BindFlags = binding;
    D3D11_SUBRESOURCE_DATA input{data.data(), 0, 0};
    ComPtr<ID3D11Buffer> result;
    require(device.CreateBuffer(&layout, &input, &result), "CreateBuffer");
    return result;
}

std::vector<std::byte> texturePixels(fs::path raw, std::size_t expected) {
    auto compact = raw;
    compact += L".zlib";
    if (!fs::exists(compact)) return read(raw);
    const auto input = read(compact);
    std::vector<std::byte> pixels(expected);
    uLong inputBytes = static_cast<uLong>(input.size());
    uLongf outputBytes = static_cast<uLongf>(pixels.size());
    if (uncompress2(reinterpret_cast<Bytef*>(pixels.data()), &outputBytes,
                    reinterpret_cast<const Bytef*>(input.data()), &inputBytes) != Z_OK ||
        outputBytes != pixels.size() || inputBytes != input.size())
        throw std::runtime_error("Corrupt compressed source texture");
    return pixels;
}

ComPtr<ID3D11ShaderResourceView> image(ID3D11Device& device, const fs::path& root, UINT slot, bool depth) {
    const auto key = std::to_wstring(slot);
    std::ifstream source(root / (L"texture-" + key + L".txt"));
    UINT width = 0, height = 0, srgb = 0;
    if (!(source >> width >> height >> srgb) || width == 0 || width > 8192 || height == 0 || height > 8192 || srgb > 1)
        throw std::runtime_error("Invalid source texture format");
    const auto expected = static_cast<std::size_t>(width) * height * 4;
    const auto data = texturePixels(root / (L"texture-" + key + L".rgba"), expected);
    if (data.size() != expected)
        throw std::runtime_error("Source texture payload does not match its dimensions");
    D3D11_TEXTURE2D_DESC description{};
    description.Width = width;
    description.Height = height;
    description.MipLevels = 1;
    description.ArraySize = 1;
    description.Format = depth ? DXGI_FORMAT_R32_FLOAT :
        srgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
    description.SampleDesc.Count = 1;
    description.Usage = D3D11_USAGE_IMMUTABLE;
    description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA pixels{data.data(), width * 4, 0};
    ComPtr<ID3D11Texture2D> texture;
    require(device.CreateTexture2D(&description, &pixels, &texture), "CreateTexture2D source");
    ComPtr<ID3D11ShaderResourceView> view;
    require(device.CreateShaderResourceView(texture.Get(), nullptr, &view), "CreateShaderResourceView source");
    return view;
}

ComPtr<ID3D11ShaderResourceView> depthSlice(ID3D11Device& device, const std::byte* data) {
    D3D11_TEXTURE2D_DESC description{};
    description.Width = description.Height = description.MipLevels = description.ArraySize = 1;
    description.Format = DXGI_FORMAT_R32_FLOAT;
    description.SampleDesc.Count = 1;
    description.Usage = D3D11_USAGE_IMMUTABLE;
    description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    const D3D11_SUBRESOURCE_DATA input{data, sizeof(float), 0};
    ComPtr<ID3D11Texture2D> texture;
    require(device.CreateTexture2D(&description, &input, &texture), "CreateTexture2D depth slice");
    ComPtr<ID3D11ShaderResourceView> view;
    require(device.CreateShaderResourceView(texture.Get(), nullptr, &view), "CreateShaderResourceView depth slice");
    return view;
}

void run(const fs::path& source, const fs::path& output, Pass pass) {
    if (fs::exists(output)) throw std::runtime_error("Capture target already exists");
    const bool volume = pass == Pass::FogVolume;
    const bool fog = pass == Pass::Fog || volume;
    const UINT faceSize = pass == Pass::Sky ? 256 : 128;
    const UINT height = volume ? 72 : faceSize;
    const UINT rowSize = faceSize * 4 * sizeof(float);
    const UINT globalBytes = pass == Pass::Cloud ? 2416 : 2448;
    const UINT textureCount = pass == Pass::Sky ? 6 : pass == Pass::Cloud ? 5 : 3;
    const std::string name = pass == Pass::Sky ? "sky" : pass == Pass::Cloud ? "cozy-cloud" : "fog";
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    require(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
                              &device, nullptr, &context), "D3D11CreateDevice");
    const auto vertexProgram = read(source / (name + "-vertex.dxbc"));
    const auto fragmentProgram = read(source / (name + "-fragment.dxbc"));
    ComPtr<ID3D11VertexShader> vertex;
    ComPtr<ID3D11PixelShader> fragment;
    require(device->CreateVertexShader(vertexProgram.data(), vertexProgram.size(), nullptr, &vertex), "CreateVertexShader");
    require(device->CreatePixelShader(fragmentProgram.data(), fragmentProgram.size(), nullptr, &fragment), "CreatePixelShader");
    constexpr D3D11_INPUT_ELEMENT_DESC attributes[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 3 * sizeof(float), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 6 * sizeof(float), D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    ComPtr<ID3D11InputLayout> layout;
    const UINT attributeCount = fog ? 2 : static_cast<UINT>(std::size(attributes));
    require(device->CreateInputLayout(attributes, attributeCount, vertexProgram.data(),
                                      vertexProgram.size(), &layout), "CreateInputLayout");
    const auto vertices = buffer(*device.Get(), source / "vertices.bin", D3D11_BIND_VERTEX_BUFFER);
    const auto indices = buffer(*device.Get(), source / "indices.bin", D3D11_BIND_INDEX_BUFFER);
    D3D11_BUFFER_DESC indexed{};
    indices->GetDesc(&indexed);
    if (indexed.ByteWidth % (3 * sizeof(std::uint32_t))) throw std::runtime_error("Invalid original triangle list");
    const auto model = buffer(*device.Get(), source / "object.bin", D3D11_BIND_CONSTANT_BUFFER, 688);
    std::array<ComPtr<ID3D11ShaderResourceView>, 6> texture;
    std::array<ID3D11ShaderResourceView*, 6> views{};
    for (UINT slot = 0; slot < textureCount; ++slot) {
        if (volume && slot == 2) continue;
        texture[slot] = image(*device.Get(), source, slot, fog && slot == 2);
        views[slot] = texture[slot].Get();
    }
    D3D11_SAMPLER_DESC sampling{};
    sampling.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampling.AddressU = sampling.AddressV = sampling.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampling.MaxLOD = D3D11_FLOAT32_MAX;
    ComPtr<ID3D11SamplerState> sampler;
    require(device->CreateSamplerState(&sampling, &sampler), "CreateSamplerState");
    std::array<ID3D11SamplerState*, 6> samplers{};
    samplers.fill(sampler.Get());
    ComPtr<ID3D11SamplerState> depthSampler;
    if (fog) {
        sampling.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        sampling.AddressU = sampling.AddressV = sampling.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        require(device->CreateSamplerState(&sampling, &depthSampler), "CreateSamplerState depth");
        samplers[0] = depthSampler.Get();
    }

    D3D11_TEXTURE2D_DESC image{};
    image.Width = faceSize;
    image.Height = height;
    image.MipLevels = image.ArraySize = image.SampleDesc.Count = 1;
    image.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    image.Usage = D3D11_USAGE_DEFAULT;
    image.BindFlags = D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> color;
    require(device->CreateTexture2D(&image, nullptr, &color), "CreateTexture2D render target");
    ComPtr<ID3D11RenderTargetView> target;
    require(device->CreateRenderTargetView(color.Get(), nullptr, &target), "CreateRenderTargetView");
    image.Usage = D3D11_USAGE_STAGING;
    image.BindFlags = 0;
    image.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> readback;
    require(device->CreateTexture2D(&image, nullptr, &readback), "CreateTexture2D readback");
    D3D11_RASTERIZER_DESC raster{};
    raster.FillMode = D3D11_FILL_SOLID;
    raster.CullMode = D3D11_CULL_FRONT;
    raster.DepthClipEnable = TRUE;
    ComPtr<ID3D11RasterizerState> rasterizer;
    require(device->CreateRasterizerState(&raster, &rasterizer), "CreateRasterizerState");
    D3D11_VIEWPORT viewport{0, 0, float(faceSize), float(height), 0, 1};
    context->RSSetState(rasterizer.Get());
    context->RSSetViewports(1, &viewport);
    context->IASetInputLayout(layout.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11Buffer* positions = vertices.Get();
    const UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &positions, &vertexSize, &offset);
    context->IASetIndexBuffer(indices.Get(), DXGI_FORMAT_R32_UINT, 0);
    context->VSSetShader(vertex.Get(), nullptr, 0);
    context->PSSetShader(fragment.Get(), nullptr, 0);
    context->PSSetShaderResources(0, textureCount, views.data());
    context->PSSetSamplers(0, textureCount, samplers.data());
    ID3D11RenderTargetView* selected = target.Get();
    context->OMSetRenderTargets(1, &selected, nullptr);

    std::ofstream stream(output, std::ios::binary);
    if (!stream) throw std::runtime_error("Could not create the original shader capture");
    const auto depths = volume ? read(source / "depth-slices.bin") : std::vector<std::byte>{};
    if (volume && (depths.size() % sizeof(float) || depths.size() > 256 * sizeof(float)))
        throw std::runtime_error("Invalid fog depth slice count");
    const auto samples = volume ? depths.size() / sizeof(float) : 6;
    for (std::size_t sample = 0; sample < samples; ++sample) {
        const auto suffix = std::to_string(volume ? 0 : sample) + ".bin";
        const auto camera = buffer(*device.Get(), source / ("view-" + suffix), D3D11_BIND_CONSTANT_BUFFER, globalBytes);
        const auto pixels = buffer(*device.Get(), source / (fog ? "world-" + suffix : "world.bin"),
                                   D3D11_BIND_CONSTANT_BUFFER, globalBytes);
        ID3D11Buffer* vertexConstants[] = {camera.Get(), model.Get()};
        ID3D11Buffer* fragmentConstants[] = {pixels.Get(), model.Get()};
        context->VSSetConstantBuffers(0, 2, vertexConstants);
        context->PSSetConstantBuffers(0, 2, fragmentConstants);
        if (volume) {
            texture[2] = depthSlice(*device.Get(), depths.data() + sample * sizeof(float));
            views[2] = texture[2].Get();
            context->PSSetShaderResources(2, 1, &views[2]);
        }
        constexpr float clear[]{0, 0, 0, 0};
        context->ClearRenderTargetView(target.Get(), clear);
        context->DrawIndexed(indexed.ByteWidth / sizeof(std::uint32_t), 0, 0);
        context->CopyResource(readback.Get(), color.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        require(context->Map(readback.Get(), 0, D3D11_MAP_READ, 0, &mapped), "Map original frame");
        const auto* data = static_cast<const char*>(mapped.pData);
        for (UINT row = 0; row < height; ++row) stream.write(data + row * mapped.RowPitch, rowSize);
        context->Unmap(readback.Get(), 0);
        if (!stream) throw std::runtime_error("Incomplete original shader capture");
    }
    std::cout << "Original Direct3D 11 " << name << " program rendered " << samples << " " << faceSize << "x" << height
              << " RGBA32F " << (volume ? "depth slices" : "directions") << '\n';
}
}

int wmain(int argc, wchar_t** argv) try {
    if (argc < 3 || argc > 4) throw std::invalid_argument("Expected source data, unused output, and optional pass name");
    Pass pass = Pass::Sky;
    if (argc == 4) {
        const std::wstring mode = argv[3];
        if (mode == L"cozy-cloud") pass = Pass::Cloud;
        else if (mode == L"fog") pass = Pass::Fog;
        else if (mode == L"fog-volume") pass = Pass::FogVolume;
        else throw std::invalid_argument("Expected cozy-cloud, fog or fog-volume");
    }
    run(argv[1], argv[2], pass);
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
