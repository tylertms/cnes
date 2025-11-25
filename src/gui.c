#include "gui.h"
#include "SDL3/SDL_render.h"
#include "ppu.h"

int gui_init(_gui* gui, char* file) {
    const int width = 256 * 3;
    const int height = 240 * 3;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

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

    SDL_SetRenderVSync(gui->renderer, 1);

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
    SDL_UpdateTexture(
        gui->texture,
        NULL,
        gui->pixels,
        NES_W * 4
    );

    SDL_RenderClear(gui->renderer);

    SDL_FRect src = { 0.0f, 0.0f, (float)NES_W, (float)NES_H };
    int win_w, win_h;
    SDL_GetRenderOutputSize(gui->renderer, &win_w, &win_h);
    SDL_FRect dst = { 0.0f, 0.0f, (float)win_w, (float)win_h };

    SDL_RenderTexture(gui->renderer, gui->texture, &src, &dst);
    SDL_RenderPresent(gui->renderer);
}

void gui_deinit(_gui *gui) {
    if (!gui) return;

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
