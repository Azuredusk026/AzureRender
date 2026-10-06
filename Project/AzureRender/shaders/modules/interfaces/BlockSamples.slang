// Sampling strategy contract. The filter owns reduction and threshold policy.
public interface IBlockSamples {
    float3 read(uint index);
}
