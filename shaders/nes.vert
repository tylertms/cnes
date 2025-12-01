#version 460 core

layout(location = 0) out vec2 v_uv;

void main() {
    v_uv = vec2((gl_VertexIndex & 1), (gl_VertexIndex >> 1));
    gl_Position = vec4(v_uv * 2.0 - 1.0, 0.0, 1.0);
}
