#ifndef AZURE_MOTION_VECTOR_PUSH
#define AZURE_MOTION_VECTOR_PUSH
layout(push_constant) uniform MotionPush {
    float alphaCutoff;uint flags;uint material;uint primitive;
    mat4 gizmo;vec4 camera;vec4 detail;vec4 outline;
} motion;
#endif
