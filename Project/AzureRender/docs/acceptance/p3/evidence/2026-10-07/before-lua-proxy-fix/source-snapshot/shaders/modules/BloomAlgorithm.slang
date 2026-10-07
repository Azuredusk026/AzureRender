import SharedTypes;
import interfaces.BlockSamples;

public float3 filterBlock<T : IBlockSamples>(T source, BloomParameters parameters) {
    float3 color = float3(0.0);
    for (uint index = 0; index < 4; ++index) color += source.read(index);
    color *= 0.25;
    if (parameters.extractBright != 0)
        color = max(color - max(parameters.threshold, 0.0), 0.0);
    return color;
}
