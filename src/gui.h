#pragma once
#include <SDL3/SDL.h>
#include <imgui/dcimgui.h>
#include <imgui/backends/dcimgui_impl_sdl3.h>
#include <imgui/backends/dcimgui_impl_sdlrenderer3.h>

typedef struct _gui {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    uint32_t* pixels;
    ImGuiContext* imgui;
} _gui;

int gui_init(_gui* gui, char* file);
void set_pixel(_gui* gui, uint16_t x, uint16_t y, uint32_t color);
void gui_draw(_gui* gui);
void gui_deinit(_gui *gui);
