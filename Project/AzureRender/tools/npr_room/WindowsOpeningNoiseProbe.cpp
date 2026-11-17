#include <d3d11.h>
#include <wrl/client.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;
constexpr UINT width = 320, height = 180;
constexpr UINT timelineWidth = 64;
using Vector = std::array<float, 4>;

void require(HRESULT status, const char* operation) {
    if (FAILED(status)) {
        std::cerr << operation << " returned 0x" << std::hex << static_cast<unsigned>(status) << '\n';
        throw std::runtime_error(operation);
    }
}

std::vector<std::byte> read(const fs::path& file) {
    std::ifstream stream(file, std::ios::binary | std::ios::ate);
    const auto length = stream.tellg();
    if (length <= 0 || length > 1024 * 1024) throw std::runtime_error("Invalid source payload size");
    std::vector<std::byte> result(static_cast<size_t>(length));
    stream.seekg(0);
    stream.read(reinterpret_cast<char*>(result.data()), length);
    if (!stream) throw std::runtime_error("Incomplete source payload");
    return result;
}

template<size_t N>
ComPtr<ID3D11Buffer> constants(ID3D11Device& device, const std::array<Vector, N>& values) {
    D3D11_BUFFER_DESC layout{};
    layout.ByteWidth = static_cast<UINT>(sizeof(values));
    layout.Usage = D3D11_USAGE_IMMUTABLE;
    layout.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    D3D11_SUBRESOURCE_DATA source{values.data(), 0, 0};
    ComPtr<ID3D11Buffer> result;
    require(device.CreateBuffer(&layout, &source, &result), "CreateBuffer");
    return result;
}

void run(const fs::path& source, const fs::path& output) {
    if (fs::exists(output)) throw std::invalid_argument("Original capture output already exists");
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    require(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
                              &device, nullptr, &context), "D3D11CreateDevice");
    const auto vs = read(source / "noise-vertex.dxbc");
    const auto ps = read(source / "noise-fragment.dxbc");
    ComPtr<ID3D11VertexShader> vertex;
    ComPtr<ID3D11PixelShader> fragment;
    require(device->CreateVertexShader(vs.data(), vs.size(), nullptr, &vertex), "CreateVertexShader");
    require(device->CreatePixelShader(ps.data(), ps.size(), nullptr, &fragment), "CreatePixelShader");

    const auto timeline = read(source / "noise-timeline.rgba");
    if (timeline.size() != timelineWidth * 4) throw std::invalid_argument("Expected the original 64-by-1 noise timeline");
    D3D11_TEXTURE2D_DESC texture{};
    texture.Width = timelineWidth;
    texture.Height = texture.MipLevels = texture.ArraySize = texture.SampleDesc.Count = 1;
    texture.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    texture.Usage = D3D11_USAGE_IMMUTABLE;
    texture.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA row{timeline.data(), timelineWidth * 4, 0};
    ComPtr<ID3D11Texture2D> noiseTexture;
    require(device->CreateTexture2D(&texture, &row, &noiseTexture), "CreateTexture2D timeline");
    ComPtr<ID3D11ShaderResourceView> timelineView;
    require(device->CreateShaderResourceView(noiseTexture.Get(), nullptr, &timelineView), "CreateShaderResourceView");
    ID3D11ShaderResourceView* sampled = timelineView.Get();
    context->PSSetShaderResources(0, 1, &sampled);
    D3D11_SAMPLER_DESC sampling{};
    sampling.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    sampling.AddressU = sampling.AddressV = sampling.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampling.MaxLOD = D3D11_FLOAT32_MAX;
    ComPtr<ID3D11SamplerState> sampler;
    require(device->CreateSamplerState(&sampling, &sampler), "CreateSamplerState");
    ID3D11SamplerState* boundSampler = sampler.Get();
    context->PSSetSamplers(0, 1, &boundSampler);

    texture.Width = width;
    texture.Height = height;
    texture.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    texture.Usage = D3D11_USAGE_DEFAULT;
    texture.BindFlags = D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> screen;
    require(device->CreateTexture2D(&texture, nullptr, &screen), "CreateTexture2D screen");
    ComPtr<ID3D11RenderTargetView> target;
    require(device->CreateRenderTargetView(screen.Get(), nullptr, &target), "CreateRenderTargetView");
    texture.Usage = D3D11_USAGE_STAGING;
    texture.BindFlags = 0;
    texture.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> readback;
    require(device->CreateTexture2D(&texture, nullptr, &readback), "CreateTexture2D readback");
    ID3D11RenderTargetView* selected = target.Get();

    D3D11_BLEND_DESC blending{};
    blending.RenderTarget[0] = {TRUE, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD,
                                D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD,
                                D3D11_COLOR_WRITE_ENABLE_ALL};
    ComPtr<ID3D11BlendState> blend;
    require(device->CreateBlendState(&blending, &blend), "CreateBlendState");
    context->OMSetBlendState(blend.Get(), nullptr, 0xffffffff);
    context->OMSetRenderTargets(1, &selected, nullptr);
    D3D11_RASTERIZER_DESC raster{};
    raster.FillMode = D3D11_FILL_SOLID;
    raster.CullMode = D3D11_CULL_NONE;
    raster.DepthClipEnable = TRUE;
    ComPtr<ID3D11RasterizerState> rasterizer;
    require(device->CreateRasterizerState(&raster, &rasterizer), "CreateRasterizerState");
    D3D11_VIEWPORT viewport{0, 0, float(width), float(height), 0, 1};
    context->RSSetState(rasterizer.Get());
    context->RSSetViewports(1, &viewport);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vertex.Get(), nullptr, 0);
    context->PSSetShader(fragment.Get(), nullptr, 0);

    std::array<Vector, 135> vertexGlobals{};
    for (UINT i = 0; i < 4; ++i) vertexGlobals[86 + i][i] = 1;
    const auto vertexBuffer = constants(*device.Get(), vertexGlobals);
    ID3D11Buffer* vertexConstant = vertexBuffer.Get();
    context->VSSetConstantBuffers(0, 1, &vertexConstant);
    constexpr std::array<std::array<float, 2>, 3> samples{{{9.302F, 0}, {9.302F, .05F}, {9.802F, .05F}}};
    constexpr Vector background{.25F, .5F, .75F, 1};
    std::ofstream stream(output, std::ios::binary);
    if (!stream) throw std::runtime_error("Could not create original noise capture");
    for (const auto& sample : samples) {
        std::array<Vector, 26> globals{};
        globals[25] = {sample[0], std::sin(sample[0]), std::cos(sample[0]), 0};
        const std::array<Vector, 2> material{{{}, {sample[1], 0, 0, 0}}};
        const auto worldBuffer = constants(*device.Get(), globals);
        const auto materialBuffer = constants(*device.Get(), material);
        ID3D11Buffer* buffers[]{worldBuffer.Get(), materialBuffer.Get()};
        context->PSSetConstantBuffers(0, 2, buffers);
        context->ClearRenderTargetView(target.Get(), background.data());
        context->Draw(3, 0);
        context->CopyResource(readback.Get(), screen.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        require(context->Map(readback.Get(), 0, D3D11_MAP_READ, 0, &mapped), "Map original noise");
        const auto* bytes = static_cast<const char*>(mapped.pData);
        for (UINT y = 0; y < height; ++y) stream.write(bytes + y * mapped.RowPitch, width * sizeof(Vector));
        context->Unmap(readback.Get(), 0);
        if (!stream) throw std::runtime_error("Incomplete original noise capture");
    }
    std::cout << "Original Direct3D 11 noise captured three 320x180 RGBA32F frames\n";
}
}

int wmain(int argc, wchar_t** argv) try {
    if (argc != 3) throw std::invalid_argument("Expected original effect directory and unused capture path");
    run(argv[1], argv[2]);
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
