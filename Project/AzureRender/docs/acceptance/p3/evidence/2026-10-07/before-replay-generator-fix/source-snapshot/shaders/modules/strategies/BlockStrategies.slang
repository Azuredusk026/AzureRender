import interfaces.BlockSamples;

// Fetch-on-demand strategy with no local sample cache.
public struct DirectSamples : IBlockSamples {
    public Sampler2D image;
    public int2 origin;
    public int2 limit;
    public float3 read(uint index) {
        return image.Load(int3(min(origin + int2(index % 2, index / 2), limit), 0)).rgb;
    }
}

// A local immutable block supports algorithms that reuse fetched samples.
public struct CachedSamples : IBlockSamples {
    public float3 values[4];
    public float3 read(uint index) { return values[index]; }
}
