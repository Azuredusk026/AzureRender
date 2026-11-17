#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;
constexpr UINT width = 320, height = 180;
using Vector = std::array<float, 4>;
constexpr Vector background{.25F, .5F, .75F, 1};

struct Sample { float time, ratio, blind; };
enum class Effect { Noise, Blackout, Chromatic, ScreenMove, Glitch };

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

ComPtr<ID3D11ShaderResourceView> textureView(ID3D11Device& device, UINT w, UINT h,
        DXGI_FORMAT format, const void* pixels, UINT rowPitch) {
    D3D11_TEXTURE2D_DESC texture{};
    texture.Width = w;
    texture.Height = h;
    texture.MipLevels = texture.ArraySize = texture.SampleDesc.Count = 1;
    texture.Format = format;
    texture.Usage = D3D11_USAGE_IMMUTABLE;
    texture.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA row{pixels, rowPitch, 0};
    ComPtr<ID3D11Texture2D> image;
    require(device.CreateTexture2D(&texture, &row, &image), "CreateTexture2D source");
    ComPtr<ID3D11ShaderResourceView> view;
    require(device.CreateShaderResourceView(image.Get(), nullptr, &view), "CreateShaderResourceView");
    return view;
}

ComPtr<ID3D11ShaderResourceView> rgbaView(ID3D11Device& device, const fs::path& path, UINT w, UINT h, bool srgb = true) {
    const auto pixels = read(path);
    if (pixels.size() != w * h * 4) throw std::invalid_argument("Unexpected source RGBA dimensions");
    const auto format = srgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
    return textureView(device, w, h, format, pixels.data(), w * 4);
}

ComPtr<ID3D11ShaderResourceView> sourceScreen(ID3D11Device& device, const fs::path& path) {
    const auto input = read(path);
    if (input.size() != width * height * sizeof(Vector)) throw std::invalid_argument("Unexpected screen source dimensions");
    return textureView(device, width, height, DXGI_FORMAT_R32G32B32A32_FLOAT, input.data(), width * sizeof(Vector));
}

ComPtr<ID3D11SamplerState> sampler(ID3D11Device& device, D3D11_FILTER filter, D3D11_TEXTURE_ADDRESS_MODE wrap) {
    D3D11_SAMPLER_DESC sampling{};
    sampling.Filter = filter;
    sampling.AddressU = sampling.AddressV = sampling.AddressW = wrap;
    sampling.MaxLOD = D3D11_FLOAT32_MAX;
    ComPtr<ID3D11SamplerState> result;
    require(device.CreateSamplerState(&sampling, &result), "CreateSamplerState");
    return result;
}

void bindSources(ID3D11Device& device, ID3D11DeviceContext& context, const fs::path& source,
                 Effect effect, const std::string& prefix) {
    const auto linear = sampler(device, D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT, D3D11_TEXTURE_ADDRESS_WRAP);
    if (effect == Effect::ScreenMove) {
        const auto point = sampler(device, D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_WRAP);
        const auto clamp = sampler(device, D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);
        const auto bands = rgbaView(device, source / "screen-move-bands.rgba", 1, 32, false);
        const auto instability = rgbaView(device, source / "screen-move-instability.rgba", 64, 1);
        const auto flow = rgbaView(device, source / "screen-move-flow.rgba", 32, 18);
        const auto movement = rgbaView(device, source / "screen-move-timeline.rgba", 32, 1);
        const auto screen = sourceScreen(device, source / "screen-move-source.rgba32f");
        ID3D11ShaderResourceView* views[]{bands.Get(), instability.Get(), flow.Get(), movement.Get(), screen.Get()};
        ID3D11SamplerState* samplers[]{point.Get(), linear.Get(), clamp.Get(), point.Get()};
        context.PSSetShaderResources(0, 5, views);
        context.PSSetSamplers(0, 4, samplers);
        return;
    }
    const auto timeline = rgbaView(device, source / (prefix + "-timeline.rgba"), 64, 1);
    if (effect == Effect::Noise) {
        ID3D11ShaderResourceView* view = timeline.Get();
        ID3D11SamplerState* sampling = linear.Get();
        context.PSSetShaderResources(0, 1, &view);
        context.PSSetSamplers(0, 1, &sampling);
        return;
    }
    if (effect == Effect::Chromatic) {
        const auto noise = rgbaView(device, source / "chromatic-noise.rgba", 256, 256);
        const auto screen = sourceScreen(device, source / "chromatic-source.rgba32f");
        ID3D11ShaderResourceView* views[]{noise.Get(), timeline.Get(), screen.Get()};
        ID3D11SamplerState* samplers[]{linear.Get(), linear.Get()};
        context.PSSetShaderResources(0, 3, views);
        context.PSSetSamplers(0, 2, samplers);
        return;
    }
    if (effect == Effect::Glitch) {
        const auto pointClamp = sampler(device, D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);
        const auto camera = rgbaView(device, source / "glitch-source.rgba", 512, 288, false);
        ID3D11ShaderResourceView* views[]{timeline.Get(), camera.Get()};
        ID3D11SamplerState* samplers[]{linear.Get(), pointClamp.Get()};
        context.PSSetShaderResources(0, 2, views);
        context.PSSetSamplers(0, 2, samplers);
        return;
    }
    const auto point = sampler(device, D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_WRAP);
    const auto clamp = sampler(device, D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);
    const auto ramp = rgbaView(device, source / "blackout-ramp.rgba", 32, 32);
    constexpr Vector opaqueColor{.7F, .45F, .2F, 1};
    const auto opaque = textureView(device, 1, 1, DXGI_FORMAT_R32G32B32A32_FLOAT, opaqueColor.data(), sizeof(Vector));
    ID3D11ShaderResourceView* views[]{opaque.Get(), ramp.Get(), ramp.Get(), timeline.Get()};
    ID3D11SamplerState* samplers[]{clamp.Get(), point.Get(), point.Get(), linear.Get()};
    context.PSSetShaderResources(0, 4, views);
    context.PSSetSamplers(0, 4, samplers);
}

void run(const fs::path& source, const fs::path& output, Effect effect, std::optional<UINT> adapterIndex) {
    if (fs::exists(output)) throw std::invalid_argument("Original capture output already exists");
    ComPtr<IDXGIAdapter1> requestedAdapter;
    if (adapterIndex) {
        ComPtr<IDXGIFactory1> factory;
        require(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "CreateDXGIFactory1");
        require(factory->EnumAdapters1(*adapterIndex, &requestedAdapter), "EnumAdapters1");
    }
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    require(D3D11CreateDevice(requestedAdapter.Get(), requestedAdapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
                              nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
                              &device, nullptr, &context), "D3D11CreateDevice");
    ComPtr<IDXGIDevice> dxgi;
    ComPtr<IDXGIAdapter> adapter;
    DXGI_ADAPTER_DESC properties{};
    require(device.As(&dxgi), "Query DXGI device");
    require(dxgi->GetAdapter(&adapter), "Query DXGI adapter");
    require(adapter->GetDesc(&properties), "Read DXGI adapter");
    std::wcout << L"Direct3D adapter " << properties.Description << '\n';
    std::string prefix;
    switch (effect) {
    case Effect::Noise: prefix = "noise"; break;
    case Effect::Blackout: prefix = "blackout"; break;
    case Effect::Chromatic: prefix = "chromatic"; break;
    case Effect::ScreenMove: prefix = "screen-move"; break;
    case Effect::Glitch: prefix = "glitch"; break;
    }
    const auto vs = read(source / (prefix + "-vertex.dxbc"));
    const auto ps = read(source / (prefix + "-fragment.dxbc"));
    ComPtr<ID3D11VertexShader> vertex;
    ComPtr<ID3D11PixelShader> fragment;
    require(device->CreateVertexShader(vs.data(), vs.size(), nullptr, &vertex), "CreateVertexShader");
    require(device->CreatePixelShader(ps.data(), ps.size(), nullptr, &fragment), "CreatePixelShader");

    bindSources(*device.Get(), *context.Get(), source, effect, prefix);
    D3D11_TEXTURE2D_DESC texture{};
    texture.Width = width;
    texture.Height = height;
    texture.MipLevels = texture.ArraySize = texture.SampleDesc.Count = 1;
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
    const bool alphaBlend = effect != Effect::Chromatic && effect != Effect::ScreenMove;
    blending.RenderTarget[0] = {alphaBlend, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD,
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
    std::vector<Sample> samples;
    switch (effect) {
    case Effect::Noise: samples = {{9.302F, 0, 0}, {9.302F, .05F, 0}, {9.802F, .05F, 0}}; break;
    case Effect::Blackout:
        samples = {{9.302F, 0, 0}, {9.302F, .5F, 0}, {9.802F, .5F, 0}, {9.302F, 1, 0}, {9.302F, .5F, 1}};
        break;
    case Effect::Chromatic: samples = {{9.302F, 0, 0}, {9.302F, .2F, 0}, {9.802F, .2F, 0}, {9.302F, .5F, 0}}; break;
    case Effect::ScreenMove: samples = {{9.302F, 0, 0}, {9.302F, .5F, 0}, {9.802F, .5F, 0}, {9.302F, 1, 0}}; break;
    case Effect::Glitch: samples = {{9.302F, 0, 0}, {9.302F, .5F, 0}, {9.802F, .5F, 0}, {9.302F, .3F, 0}}; break;
    }
    std::array<Vector, 6> initialMaterial{};
    if (effect != Effect::Noise) {
        const auto data = read(source / (prefix + "-material.bin"));
        const auto vectors = effect == Effect::Blackout ? 5 : effect == Effect::Chromatic ? 3 : effect == Effect::Glitch ? 4 : 6;
        const auto expectedBytes = vectors * sizeof(Vector);
        if (data.size() != expectedBytes) throw std::invalid_argument("Unexpected original material constant buffer");
        std::memcpy(initialMaterial.data(), data.data(), data.size());
    }
    std::ofstream stream(output, std::ios::binary);
    if (!stream) throw std::runtime_error("Could not create original capture");
    for (const auto& sample : samples) {
        std::array<Vector, 135> globals{};
        globals[25] = {sample.time, std::sin(sample.time), std::cos(sample.time), 0};
        globals[28] = {float(width), float(height), 1 + 1.F / width, 1 + 1.F / height};
        auto material = initialMaterial;
        if (effect == Effect::Blackout) {
            material[3][1] = sample.ratio;
            material[4][3] = sample.blind;
        } else if (effect == Effect::Chromatic || effect == Effect::ScreenMove) {
            globals[126] = {float(width), float(height), 1.F / width, 1.F / height};
            material[2][0] = sample.ratio;
        } else if (effect == Effect::Glitch) {
            material[1][1] = sample.ratio;
        } else {
            material[1][0] = sample.ratio;
        }
        const auto worldBuffer = constants(*device.Get(), globals);
        const auto materialBuffer = constants(*device.Get(), material);
        ID3D11Buffer* buffers[]{worldBuffer.Get(), materialBuffer.Get()};
        context->PSSetConstantBuffers(0, 2, buffers);
        context->ClearRenderTargetView(target.Get(), background.data());
        context->Draw(3, 0);
        context->CopyResource(readback.Get(), screen.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        require(context->Map(readback.Get(), 0, D3D11_MAP_READ, 0, &mapped), "Map original effect");
        const auto* bytes = static_cast<const char*>(mapped.pData);
        for (UINT y = 0; y < height; ++y) stream.write(bytes + y * mapped.RowPitch, width * sizeof(Vector));
        context->Unmap(readback.Get(), 0);
        if (!stream) throw std::runtime_error("Incomplete original effect capture");
    }
    std::cout << "Original Direct3D 11 effect captured " << samples.size() << " 320x180 RGBA32F frames\n";
}
}

int wmain(int argc, wchar_t** argv) try {
    if (argc < 3 || argc > 5) throw std::invalid_argument("Expected input directory and unused capture path");
    Effect effect = Effect::Noise;
    if (argc >= 4) {
        const std::wstring_view name(argv[3]);
        if (name == L"blackout") effect = Effect::Blackout;
        else if (name == L"chromatic") effect = Effect::Chromatic;
        else if (name == L"screen-move") effect = Effect::ScreenMove;
        else if (name == L"glitch") effect = Effect::Glitch;
        else throw std::invalid_argument("Expected blackout, chromatic, screen-move or glitch and an optional adapter index");
    }
    std::optional<UINT> index;
    if (argc == 5) {
        const std::wstring_view text(argv[4]);
        if (text.empty() || text.find_first_not_of(L"0123456789") != std::wstring_view::npos)
            throw std::invalid_argument("Expected a numeric DXGI adapter index");
        index = std::stoul(std::wstring(text));
    }
    run(argv[1], argv[2], effect, index);
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
