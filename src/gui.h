#pragma once

#include "nes.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <dcimgui.h>
#include <backends/dcimgui_impl_sdl3.h>
#include <backends/dcimgui_impl_sdlgpu3.h>

#ifdef __APPLE__
    #define MOD_KEY "Cmd+"
#else
    #define MOD_KEY "Ctrl+"
#endif

static const double NES_FRAME_TIME = 655171.0 / 39375000.0;

typedef struct _present_mode {
    SDL_GPUPresentMode mode;
    const char* name;
    uint8_t supported;
} _present_mode;

static _present_mode PRESENT_MODES[] = {
    { SDL_GPU_PRESENTMODE_MAILBOX,   "Adaptive Sync",   0 },
    { SDL_GPU_PRESENTMODE_VSYNC,     "VSync",           0 },
    { SDL_GPU_PRESENTMODE_IMMEDIATE, "No VSync",        0 },
};
static const int PRESENT_MODE_COUNT = sizeof(PRESENT_MODES) / sizeof(_present_mode);

typedef struct _gui {
    SDL_Window* window;
    SDL_GPUDevice* gpu_device;

    SDL_GPUPresentMode present_mode;
    SDL_GPUPresentMode pending_mode;
    uint8_t needs_mode_update;

    SDL_GPUTexture* nes_texture;
    SDL_GPUTransferBuffer* nes_transfer;
    SDL_GPUSampler* nes_sampler;
    SDL_GPUShader* nes_vs;
    SDL_GPUShader* nes_fs;
    SDL_GPUGraphicsPipeline* nes_pipeline;

    float menu_height;

    uint8_t quit;

    ImGuiContext* im_ctx;
    ImFont* nes_font;
} _gui;

CNES_RESULT gui_init(_gui* gui);
void gui_draw(_gui* gui, _nes* nes);
void gui_deinit(_gui* gui);
void gui_open_file_dialog(_gui* gui, _nes* nes);
void gui_set_title(_gui* gui, _cart* cart);
