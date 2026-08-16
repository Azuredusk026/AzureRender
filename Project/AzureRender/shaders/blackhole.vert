#version 450

layout(location = 0) out vec2 screenUv;

void main() {
    // Fullscreen triangle without a vertex buffer:
    // vertex 0 -> (-1,-1), vertex 1 -> (3,-1), vertex 2 -> (-1,3).
    vec2 position = vec2(
        float((gl_VertexIndex << 1) & 2),
        float(gl_VertexIndex & 2));
    screenUv = position * 0.5;
    gl_Position = vec4(position * 2.0 - 1.0, 0.0, 1.0);
}
