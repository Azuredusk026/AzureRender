// Receiver range layout v2: four consecutive uvec2 records per cluster.
const uint AZURE_CLUSTER_RECEIVER_RANGE_COUNT=4U;
const uint AZURE_CLUSTER_SURFACE_RANGE=0U;
const uint AZURE_CLUSTER_CHARACTER_RANGE=1U;
const uint AZURE_CLUSTER_SURFACE_EMISSION_RANGE=2U;
const uint AZURE_CLUSTER_CHARACTER_BLEND_RANGE=3U;
uvec2 azureClusterLightRange(uint cluster,uint receiverRange){
    return clusterHeaderData.clusters[cluster*AZURE_CLUSTER_RECEIVER_RANGE_COUNT+receiverRange];
}
