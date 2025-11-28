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
    uint64_t counter0 = SDL_GetPerformanceCounter();
    double t0 = (double)counter0 / (double)perf_freq;
    double spin_threshold = 0.001;
    double max_lag = 0.1;
    uint64_t last_frame_end = 0;
    uint64_t frame_index = 0;

    while (!nes.cpu.halt) {
        double target_time = t0 + (double)frame_index * NES_FRAME_TIME;

        for (;;) {
            uint64_t now_counter = SDL_GetPerformanceCounter();
            double now = (double)now_counter / (double)perf_freq;
            double remaining = target_time - now;
            if (remaining <= 0.0) break;
            if (remaining > spin_threshold) {
                uint32_t ms = (uint32_t)((remaining - spin_threshold) * 1000.0);
                if (ms > 0) {
                    SDL_Delay(ms);
                    continue;
                }
            }
            for (;;) {
                uint64_t c2 = SDL_GetPerformanceCounter();
                if ((double)c2 / (double)perf_freq >= target_time) break;
            }
            break;
        }

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            cImGui_ImplSDL3_ProcessEvent(&event);

            switch (event.type) {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                nes.cpu.halt = 1;
                break;

            case SDL_EVENT_KEY_DOWN:
                if ((event.key.scancode == SDL_SCANCODE_BACKSPACE) ||
                    (event.key.scancode == SDL_SCANCODE_DELETE)) {
                    nes_reset(&nes);
                } else {
                    SDL_HideCursor();
                }
                break;

            case SDL_EVENT_MOUSE_MOTION:
                SDL_ShowCursor();
                break;

            default: break;
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

        uint64_t frame_start = SDL_GetPerformanceCounter();

        nes_clock(&nes);
        gui_draw(&gui);

        uint64_t frame_end = SDL_GetPerformanceCounter();

        double frame_time = (double)(frame_end - frame_start) / (double)perf_freq;
        double interval = 0.0;
        if (last_frame_end != 0) {
            interval = (double)(frame_end - last_frame_end) / (double)perf_freq;
        }

        double percentage = (frame_time / NES_FRAME_TIME) * 100.0;
        double max_fps = 1.0 / frame_time;

        printf("%6.3fms | %6.3fms | %4.1f%% | %4.0f FPS\n",
               frame_time * 1000.0,
               interval * 1000.0,
               percentage,
               max_fps);

        last_frame_end = frame_end;
        frame_index++;

        double now = (double)frame_end / (double)perf_freq;
        double current_target = t0 + (double)frame_index * NES_FRAME_TIME;
        if (now - current_target > max_lag) {
            t0 = now;
            frame_index = 0;
        }
    }

    nes_deinit(&nes);
    gui_deinit(&gui);

    return 0;
}
