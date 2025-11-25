#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "nes.h"
#include "gui.h"
#include <stdio.h>
#include <stdbool.h>

static const double NES_FRAME_TIME = 655171.0 / 39375000.0;

typedef struct AppState {
    _nes nes;
    _gui gui;

    uint64_t perf_freq;
    uint64_t last_counter;
    double accumulator;
} AppState;

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv) {
    if (argc <= 1) {
        printf("Usage: %s <file.nes>\n", argv[0]);
        return SDL_APP_FAILURE;
    }

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        printf("Failed to init SDL audio: %s\n", SDL_GetError());
    }

    AppState *state = (AppState *)SDL_calloc(1, sizeof(AppState));
    if (!state) {
        printf("Failed to allocate AppState\n");
        return SDL_APP_FAILURE;
    }

    if (gui_init(&state->gui, argv[1])) {
        gui_deinit(&state->gui);
        SDL_free(state);
        return SDL_APP_FAILURE;
    }

    if (nes_init(&state->nes, argv[1], &state->gui)) {
        gui_deinit(&state->gui);
        nes_deinit(&state->nes);
        SDL_free(state);
        return SDL_APP_FAILURE;
    }

    state->perf_freq = SDL_GetPerformanceFrequency();
    state->last_counter = SDL_GetPerformanceCounter();
    state->accumulator = 0.0;

    *appstate = state;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    AppState *state = (AppState *)appstate;
    if (!state) return SDL_APP_FAILURE;

    _nes *nes = &state->nes;

    if (event->type == SDL_EVENT_KEY_DOWN && event->key.repeat) {
        return SDL_APP_CONTINUE;
    }

    switch (event->type) {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        nes->cpu.halt = 1;
        return SDL_APP_SUCCESS;

    case SDL_EVENT_KEY_DOWN:
        switch (event->key.scancode) {
            case SDL_SCANCODE_BACKSPACE:
                cpu_reset(&nes->cpu);
                break;

            case SDL_SCANCODE_X:     nes->input.controller[0] |= 0x80; break;
            case SDL_SCANCODE_Z:     nes->input.controller[0] |= 0x40; break;
            case SDL_SCANCODE_A:     nes->input.controller[0] |= 0x20; break;
            case SDL_SCANCODE_S:     nes->input.controller[0] |= 0x10; break;
            case SDL_SCANCODE_UP:    nes->input.controller[0] |= 0x08; break;
            case SDL_SCANCODE_DOWN:  nes->input.controller[0] |= 0x04; break;
            case SDL_SCANCODE_LEFT:  nes->input.controller[0] |= 0x02; break;
            case SDL_SCANCODE_RIGHT: nes->input.controller[0] |= 0x01; break;
            default: break;
        }
        break;

    case SDL_EVENT_KEY_UP:
        switch (event->key.scancode) {
            case SDL_SCANCODE_X:     nes->input.controller[0] &= ~0x80; break;
            case SDL_SCANCODE_Z:     nes->input.controller[0] &= ~0x40; break;
            case SDL_SCANCODE_A:     nes->input.controller[0] &= ~0x20; break;
            case SDL_SCANCODE_S:     nes->input.controller[0] &= ~0x10; break;
            case SDL_SCANCODE_UP:    nes->input.controller[0] &= ~0x08; break;
            case SDL_SCANCODE_DOWN:  nes->input.controller[0] &= ~0x04; break;
            case SDL_SCANCODE_LEFT:  nes->input.controller[0] &= ~0x02; break;
            case SDL_SCANCODE_RIGHT: nes->input.controller[0] &= ~0x01; break;
            default: break;
        }
        break;

    default:
        break;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    AppState *state = (AppState *)appstate;
    if (!state) return SDL_APP_FAILURE;

    _nes *nes = &state->nes;
    _gui *gui = &state->gui;

    if (nes->cpu.halt) {
        return SDL_APP_SUCCESS;
    }

    uint64_t now = SDL_GetPerformanceCounter();
    double dt = (double)(now - state->last_counter) / (double)state->perf_freq;
    state->last_counter = now;

    if (dt > 0.25) dt = 0.25;
    state->accumulator += dt;

    int did_step = 0;
    while (state->accumulator >= NES_FRAME_TIME) {
        nes_clock(nes);
        state->accumulator -= NES_FRAME_TIME;
        did_step = 1;
    }

    if (did_step) {
        gui_draw(gui);
    }

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    AppState *state = (AppState *)appstate;
    if (!state) return;

    nes_deinit(&state->nes);
    gui_deinit(&state->gui);

    SDL_free(state);
}
