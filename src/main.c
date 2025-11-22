#include "cart.h"
#include "nes.h"
#include "gui.h"
#include <stdio.h>

int main(int argc, char** argv) {
    const double NES_FRAME_TIME = 655171.0 / 39375000.0;

    if (argc <= 1) {
        printf("Usage: %s <file.nes>\n", argv[0]);
        return 1;
    }

    _gui gui;
    if (init_gui(&gui)) {
        fprintf(stderr, "ERROR: Failed to initialize GUI!\n");
        deinit_gui(&gui);
        return 1;
    }

    _nes nes;
    nes_init(&nes, &gui);

    uint8_t res = cart_load(&nes.cart, argv[1]);
    if (res) {
        deinit_gui(&gui);
        return res;
    }

    nes_reset(&nes);

    SDL_Event event;

    uint64_t perf_freq = SDL_GetPerformanceFrequency();
    uint64_t last_counter = SDL_GetPerformanceCounter();
    double accumulator  = 0.0;

    while (!nes.cpu.halt) {
        uint64_t now = SDL_GetPerformanceCounter();
        double dt = (double)(now - last_counter) / (double)perf_freq;
        last_counter = now;
        if (dt > 0.25) dt = 0.25;
        accumulator += dt;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.repeat)
                continue;

            switch (event.type) {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                nes.cpu.halt = 1;
                break;

            case SDL_EVENT_KEY_DOWN:
                switch (event.key.scancode) {
                    case SDL_SCANCODE_X:     nes.input.controller[0] |= 0x80; break;
                    case SDL_SCANCODE_Z:     nes.input.controller[0] |= 0x40; break;
                    case SDL_SCANCODE_A:     nes.input.controller[0] |= 0x20; break;
                    case SDL_SCANCODE_S:     nes.input.controller[0] |= 0x10; break;
                    case SDL_SCANCODE_UP:    nes.input.controller[0] |= 0x08; break;
                    case SDL_SCANCODE_DOWN:  nes.input.controller[0] |= 0x04; break;
                    case SDL_SCANCODE_LEFT:  nes.input.controller[0] |= 0x02; break;
                    case SDL_SCANCODE_RIGHT: nes.input.controller[0] |= 0x01; break;
                    default: break;
                }
                break;

            case SDL_EVENT_KEY_UP:
                switch (event.key.scancode) {
                    case SDL_SCANCODE_X:     nes.input.controller[0] &= ~0x80; break;
                    case SDL_SCANCODE_Z:     nes.input.controller[0] &= ~0x40; break;
                    case SDL_SCANCODE_A:     nes.input.controller[0] &= ~0x20; break;
                    case SDL_SCANCODE_S:     nes.input.controller[0] &= ~0x10; break;
                    case SDL_SCANCODE_UP:    nes.input.controller[0] &= ~0x08; break;
                    case SDL_SCANCODE_DOWN:  nes.input.controller[0] &= ~0x04; break;
                    case SDL_SCANCODE_LEFT:  nes.input.controller[0] &= ~0x02; break;
                    case SDL_SCANCODE_RIGHT: nes.input.controller[0] &= ~0x01; break;
                    default: break;
                }
                break;

            default:
                break;
            }
        }

        int did_step = 0;
        while (accumulator >= NES_FRAME_TIME) {
            nes_clock(&nes);
            accumulator -= NES_FRAME_TIME;
            did_step = 1;
        }

        draw_gui(&gui);
    }

    deinit_gui(&gui);
    return 0;
}
