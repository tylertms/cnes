#version 460 core

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 out_color;

layout(set = 2, binding = 0) uniform sampler2D u_tex;

void main() {
    vec2 uv = vec2(v_uv.x, 1.0 - v_uv.y);
    out_color = texture(u_tex, uv).abgr;
}
