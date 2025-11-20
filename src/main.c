#include "cart.h"
#include "nes.h"
#include "gui.h"
#include <stdio.h>

int main(int argc, char** argv) {
    const double target_fps = 60.0988;
    const double target_seconds = 1.0 / target_fps;
    const uint64_t target_frame_ns = (uint64_t)(target_seconds * 1e9);

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

    while (!nes.cpu.halt) {
        uint64_t frame_start = SDL_GetTicksNS();

        nes_clock(&nes);

        draw_gui(&gui);

        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                nes.cpu.halt = 1; break;
            default: break;
            }
        }

        uint64_t frame_end = SDL_GetTicksNS();
        uint64_t elapsed_ns = frame_end - frame_start;
        if (elapsed_ns < target_frame_ns) {
            SDL_DelayNS(target_frame_ns - elapsed_ns);
        }
    }

    return 0;
}
