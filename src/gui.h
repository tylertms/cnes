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

    GLuint nes_texture;
    GLuint program;
    GLuint vao;
    GLuint vbo;

    char glsl_version[32];

    uint32_t* pixels;

    _Bool show_settings;
    _Bool request_quit;

    ImGuiContext* im_ctx;
} _gui;

int gui_init(_gui* gui, char* file);
void set_pixel(_gui* gui, uint16_t x, uint16_t y, uint32_t color);
uint64_t gui_draw(_gui* gui);
void gui_deinit(_gui* gui);
