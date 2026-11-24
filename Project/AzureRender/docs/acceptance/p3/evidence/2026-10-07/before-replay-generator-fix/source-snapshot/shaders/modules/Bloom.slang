import SharedTypes;
import BloomAlgorithm;
import strategies.BlockStrategies;

[[vk::push_constant]] ConstantBuffer<BloomParameters> parameters;
[[vk::binding(0, 0)]] Sampler2D sourceImage;
[[vk::binding(1, 0)]] [format("rgba16f")] RWTexture2D<float4> destinationImage;

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID) {
    uint width, height;
    destinationImage.GetDimensions(width, height);
    uint2 pixel = dispatchId.xy;
    if (pixel.x >= width || pixel.y >= height) return;
    uint sourceWidth, sourceHeight;
    sourceImage.GetDimensions(sourceWidth, sourceHeight);
    DirectSamples direct;
    direct.image = sourceImage;
    direct.origin = int2(pixel) * 2;
    direct.limit = int2(sourceWidth, sourceHeight) - 1;
#if AZURE_BLOOM_CACHE
    CachedSamples source;
    for (uint index = 0; index < 4; ++index) source.values[index] = direct.read(index);
#else
    DirectSamples source = direct;
#endif
    destinationImage[pixel] = float4(filterBlock(source, parameters), 1.0);
}
