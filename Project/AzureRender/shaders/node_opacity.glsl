#ifndef AZURE_NODE_OPACITY_GLSL
#define AZURE_NODE_OPACITY_GLSL

// A fixed mesh-space hash keeps the coverage attached to the same surface in
// color, depth, outline and shadow views, including animated instances.
bool azureNodeOpacityCovered(vec3 coordinate, float opacity) {
    if (opacity >= 1.0) return true;
    if (opacity <= 0.0) return false;
    uvec3 cell=uvec3(ivec3(floor(coordinate*256.0)));
    uint hash=cell.x*1597334677u ^ cell.y*3812015801u ^ cell.z*2798796415u;
    hash ^= hash >> 16u;
    hash *= 2246822519u;
    hash ^= hash >> 13u;
    return float(hash >> 8u)*(1.0/16777216.0)<opacity;
}
#endif
