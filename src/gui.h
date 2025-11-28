#pragma once
#include <SDL3/SDL.h>
#include <glad/gl.h>
#include <dcimgui.h>
#include <dcimgui_internal.h>
#include <backends/dcimgui_impl_sdl3.h>
#include <backends/dcimgui_impl_opengl3.h>
#include <IconsFontAwesome7.h>

typedef struct _gui {
    SDL_Window* window;
    SDL_GLContext gl_ctx;

    uint32_t nes_texture;
    uint32_t program;
    uint32_t vao;
    uint32_t vbo;

    char glsl_version[32];

    uint32_t* pixels;

    _Bool show_settings;
    _Bool quit;

    ImGuiContext* im_ctx;
} _gui;

uint8_t gui_init(_gui* gui, char* file);
void set_pixel(_gui* gui, uint16_t x, uint16_t y, uint32_t color);
uint64_t gui_draw(_gui* gui);
void gui_deinit(_gui* gui);

static const char *vertex_source =
"   in vec2 pos;                                           \n"
"   in vec2 uv;                                            \n"
"   out vec2 v_uv;                                         \n"
"                                                          \n"
"   void main() {                                          \n"
"       v_uv = uv;                                         \n"
"       gl_Position = vec4(pos, 0.0, 1.0);                 \n"
"   }                                                      \n";

static const char *frag_source =
"   uniform sampler2D u_tex;                               \n"
"   in vec2 v_uv;                                          \n"
"   out vec4 frag_color;                                   \n"
"                                                          \n"
"   void main() {                                          \n"
"       vec2 coord = vec2(v_uv.x, 1.0 - v_uv.y);           \n"
"       frag_color = texture(u_tex, coord).abgr;           \n"
"   }                                                      \n";
