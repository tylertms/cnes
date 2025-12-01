#include "nes.h"
#include "gui.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <SDL3/SDL.h>

#define NES_REFRESH_RATE 60.0988138974405
#define NES_FRAME_TIME_SEC (1.0 / NES_REFRESH_RATE)

#define AUDIO_TARGET_QUEUED_BYTES (2048)
#define AUDIO_SYNC_THRESHOLD (1024)
#define MAX_ADJUSTMENT (0.0005)

static inline void print_build_info(void) {
    printf("cnes build info:\n");
#ifdef NDEBUG
    printf("  NDEBUG = 1\n");
#else
    printf("  NDEBUG = 0\n");
#endif
#ifdef __OPTIMIZE__
    printf("  __OPTIMIZE__ = 1\n");
#else
    printf("  __OPTIMIZE__ = 0\n");
#endif
    printf("  SDL3 Revision: %s\n", SDL_GetRevision());
}

static uint8_t get_button_mask(SDL_Scancode code) {
    switch (code) {
        case SDL_SCANCODE_X:     return 0x80;
        case SDL_SCANCODE_Z:     return 0x40;
        case SDL_SCANCODE_A:     return 0x20;
        case SDL_SCANCODE_S:     return 0x10;
        case SDL_SCANCODE_UP:    return 0x08;
        case SDL_SCANCODE_DOWN:  return 0x04;
        case SDL_SCANCODE_LEFT:  return 0x02;
        case SDL_SCANCODE_RIGHT: return 0x01;
        default: return 0;
    }
}

int main(int argc, char **argv) {
    print_build_info();

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
    double perf_freq_dbl = (double)perf_freq;
    double target_seconds_per_frame = NES_FRAME_TIME_SEC;

    uint64_t next_frame_target = SDL_GetPerformanceCounter();
    uint64_t last_stats_time = next_frame_target;

    uint64_t frame_count_stats = 0;
    double max_jitter = 0.0;
    double max_frame_time = 0.0;
    double min_frame_time = 1.0;

    uint8_t input_live = 0;
    uint8_t input_latch = 0;

    printf("Starting emulation loop. Target: %.4f Hz (%.6f ms/frame)\n",
           NES_REFRESH_RATE, target_seconds_per_frame * 1000.0);

    while (!nes.cpu.halt && !gui.quit) {
        uint64_t now = SDL_GetPerformanceCounter();
        int64_t remaining_ticks = (int64_t)next_frame_target - (int64_t)now;

        double remaining_seconds = (double)remaining_ticks / perf_freq_dbl;

        if (remaining_seconds > 0.002) {
            uint32_t delay_ms = (uint32_t)((remaining_seconds - 0.001) * 1000.0);
            SDL_Delay(delay_ms);
            now = SDL_GetPerformanceCounter();
        }

        while (now < next_frame_target) {
            now = SDL_GetPerformanceCounter();
        }

        double jitter = (double)((int64_t)now - (int64_t)next_frame_target) / perf_freq_dbl;
        if (jitter < 0) jitter = -jitter;
        if (jitter > max_jitter) max_jitter = jitter;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            cImGui_ImplSDL3_ProcessEvent(&event);
            switch (event.type) {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                nes.cpu.halt = 1;
                break;

            case SDL_EVENT_KEY_DOWN: {
                if ((event.key.scancode == SDL_SCANCODE_BACKSPACE) ||
                    (event.key.scancode == SDL_SCANCODE_DELETE)) {
                    nes_reset(&nes);
                } else {
                    SDL_HideCursor();
                }

                uint8_t mask = get_button_mask(event.key.scancode);
                if (mask) {
                    input_live |= mask;
                    input_latch |= mask;
                }
                break;
            }

            case SDL_EVENT_KEY_UP: {
                uint8_t mask = get_button_mask(event.key.scancode);
                if (mask) {
                    input_live &= ~mask;
                }
                break;
            }

            case SDL_EVENT_MOUSE_MOTION:
                SDL_ShowCursor();
                break;
            }
        }

        nes.input.controller[0] = input_latch;
        input_latch = input_live;


        uint64_t work_start = SDL_GetPerformanceCounter();
        nes_clock(&nes);

        if (nes.apu.audio_stream) {
            apu_flush_audio(&nes.apu);
        }


        gui_draw(&gui, &nes);

        uint64_t work_end = SDL_GetPerformanceCounter();
        double frame_work_time = (double)(work_end - work_start) / perf_freq_dbl;

        if (frame_work_time > max_frame_time) max_frame_time = frame_work_time;
        if (frame_work_time < min_frame_time) min_frame_time = frame_work_time;


        double adjustment = 0.0;

        if (nes.apu.audio_stream) {
            int queued = SDL_GetAudioStreamQueued(nes.apu.audio_stream);
            int diff = queued - AUDIO_TARGET_QUEUED_BYTES;

            if (abs(diff) > AUDIO_SYNC_THRESHOLD) {
                double correction = (double)diff * 0.0000001;
                if (correction > MAX_ADJUSTMENT) correction = MAX_ADJUSTMENT;
                if (correction < -MAX_ADJUSTMENT) correction = -MAX_ADJUSTMENT;
                adjustment = correction;
            }
        }

        next_frame_target += (uint64_t)((NES_FRAME_TIME_SEC + adjustment) * perf_freq_dbl);

        now = SDL_GetPerformanceCounter();
        if (now > next_frame_target + perf_freq) {
             next_frame_target = now;
        }


        frame_count_stats++;
        double time_since_stats = (double)(now - last_stats_time) / perf_freq_dbl;

        if (time_since_stats >= 1.0) {
            double max_jitter_us = max_jitter * 1000000.0;
            double fps = frame_count_stats / time_since_stats;
            double avg_work = (max_frame_time + min_frame_time) / 2.0 * 1000.0;

            printf("[STATS] FPS: %.2f | Max Jitter: %.2fus | Work: %.2fms | AudioQ: %d B\n",
                   fps, max_jitter_us, avg_work,
                   nes.apu.audio_stream ? SDL_GetAudioStreamQueued(nes.apu.audio_stream) : 0);

            last_stats_time = now;
            frame_count_stats = 0;
            max_jitter = 0.0;
            max_frame_time = 0.0;
            min_frame_time = 1.0;
        }
    }

    nes_deinit(&nes);
    gui_deinit(&gui);

    return 0;
}
