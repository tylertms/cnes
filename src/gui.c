#include "gui.h"
#include "dcimgui.h"
#include "ppu.h"
#include <stdio.h>

int gui_init(_gui* gui, char* file) {
    memset(gui, 0, sizeof(_gui));

    const int width = 256 * 3;
    const int height = 240 * 3;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_SetHint(SDL_HINT_TIMER_RESOLUTION, "1");

    char title[128];
    snprintf(title, 128, "cnes - %s", file);
    gui->window = SDL_CreateWindow(title, width, height, SDL_WINDOW_RESIZABLE);
    if (!gui->window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return 1;
    }

    gui->renderer = SDL_CreateRenderer(gui->window, NULL);
    if (!gui->renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        return 1;
    }

    if (!SDL_SetRenderVSync(gui->renderer, SDL_RENDERER_VSYNC_ADAPTIVE)) {
        SDL_SetRenderVSync(gui->renderer, 1);
    }

    gui->texture = SDL_CreateTexture(
        gui->renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        NES_W,
        NES_H
    );
    if (!gui->texture) {
        SDL_Log("SDL_CreateTexture failed: %s", SDL_GetError());
        return 1;
    }

    SDL_SetTextureScaleMode(gui->texture, SDL_SCALEMODE_NEAREST);

    gui->pixels = SDL_calloc(NES_PIXELS, sizeof(uint32_t));
    if (!gui->pixels) {
        SDL_Log("Failed to allocate pixel buffer");
        return 1;
    }

    ImGuiContext *ctx = ImGui_CreateContext(NULL);
    ImGui_SetCurrentContext(ctx);
    cImGui_ImplSDL3_InitForSDLRenderer(gui->window, gui->renderer);
    cImGui_ImplSDLRenderer3_Init(gui->renderer);

    ImGuiIO* io = ImGui_GetIO();
    io->ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io->ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    return 0;
}

void set_pixel(_gui* gui, uint16_t x, uint16_t y, uint32_t color) {
    if (!gui->pixels) {
        return;
    }

    if (x < NES_W && y < NES_H) {
        gui->pixels[y * NES_W + x] = color;
    }
}

void gui_draw(_gui* gui) {
    if (!gui || !gui->renderer) return;

    cImGui_ImplSDL3_NewFrame();
    cImGui_ImplSDLRenderer3_NewFrame();
    ImGui_NewFrame();

    ImGui_Begin("cnes Debug", NULL, 0);
    ImGui_Text("Hello from ImGui!");
    ImGui_Text("FPS: %.1f", ImGui_GetIO()->Framerate);
    ImGui_End();

    ImGui_Render();

    SDL_UpdateTexture(
        gui->texture, NULL,
        gui->pixels, NES_W * 4
    );

    SDL_RenderClear(gui->renderer);

    SDL_FRect src = { 0.0f, 0.0f, (float)NES_W, (float)NES_H };
    int win_w, win_h;
    SDL_GetRenderOutputSize(gui->renderer, &win_w, &win_h);
    SDL_FRect dst = { 0.0f, 0.0f, (float)win_w, (float)win_h };

    SDL_RenderTexture(gui->renderer, gui->texture, &src, &dst);

    cImGui_ImplSDLRenderer3_RenderDrawData(ImGui_GetDrawData(), gui->renderer);

    SDL_RenderPresent(gui->renderer);
}

void gui_deinit(_gui *gui) {
    if (!gui) return;

    if (gui->imgui) {
        ImGui_SetCurrentContext(gui->imgui);
        cImGui_ImplSDLRenderer3_Shutdown();
        cImGui_ImplSDL3_Shutdown();
        ImGui_DestroyContext(gui->imgui);
        gui->imgui = NULL;
    }

    if (gui->pixels) {
        SDL_free(gui->pixels);
        gui->pixels = NULL;
    }

    if (gui->texture) {
        SDL_DestroyTexture(gui->texture);
        gui->texture = NULL;
    }

    if (gui->renderer) {
        SDL_DestroyRenderer(gui->renderer);
        gui->renderer = NULL;
    }

    if (gui->window) {
        SDL_DestroyWindow(gui->window);
        gui->window = NULL;
    }

    SDL_Quit();
}
