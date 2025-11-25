#include "nes.h"
#include "gui.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <SDL3/SDL.h>

static const double NES_FRAME_TIME = 655171.0 / 39375000.0;

int main(int argc, char **argv) {
    if (argc <= 1) {
        printf("Usage: %s <file.nes>\n", argv[0]);
        return 1;
    }

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        printf("Failed to init SDL audio: %s\n", SDL_GetError());
    }

    _gui gui;
    if (gui_init(&gui, argv[1])) {
        gui_deinit(&gui);
        return 1;
    }

    _nes nes;
    if (nes_init(&nes, argv[1], &gui)) {
        gui_deinit(&gui);
        nes_deinit(&nes);
        return 1;
    }

    uint64_t perf_freq = SDL_GetPerformanceFrequency();
    uint64_t last_counter = SDL_GetPerformanceCounter();
    double accumulator = 0.0;

    while (!nes.cpu.halt) {
        uint64_t now = SDL_GetPerformanceCounter();
        double dt = (double)(now - last_counter) / (double)perf_freq;
        last_counter = now;

        if (dt > 0.25) dt = 0.25;
        accumulator += dt;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.repeat) {
                continue;
            }

            switch (event.type) {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                nes.cpu.halt = 1;
                break;

            case SDL_EVENT_KEY_DOWN:
                switch (event.key.scancode) {
                case SDL_SCANCODE_BACKSPACE:
                    cpu_reset(&nes.cpu);
                    break;
                default:
                    break;
                }
                break;

            default:
                break;
            }
        }

        const bool *keys = SDL_GetKeyboardState(NULL);
        uint8_t c0 = 0;

        if (keys[SDL_SCANCODE_X])     c0 |= 0x80;
        if (keys[SDL_SCANCODE_Z])     c0 |= 0x40;
        if (keys[SDL_SCANCODE_A])     c0 |= 0x20;
        if (keys[SDL_SCANCODE_S])     c0 |= 0x10;
        if (keys[SDL_SCANCODE_UP])    c0 |= 0x08;
        if (keys[SDL_SCANCODE_DOWN])  c0 |= 0x04;
        if (keys[SDL_SCANCODE_LEFT])  c0 |= 0x02;
        if (keys[SDL_SCANCODE_RIGHT]) c0 |= 0x01;

        nes.input.controller[0] = c0;

        if (accumulator >= NES_FRAME_TIME) {
            accumulator -= NES_FRAME_TIME;
            if (accumulator > NES_FRAME_TIME) {
                accumulator = NES_FRAME_TIME;
            }

            nes_clock(&nes);
            gui_draw(&gui);
        }

        SDL_Delay(0);
    }

    nes_deinit(&nes);
    gui_deinit(&gui);

    return 0;
}
