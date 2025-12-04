#include "nes.h"
#include "gui.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <SDL3/SDL.h>

//#define CNES_NO_STATS
#define NES_REFRESH_RATE 60.0988138974405
#define NES_FRAME_TIME_SEC (1.0 / NES_REFRESH_RATE)

#define AUDIO_TARGET_QUEUED_BYTES (6144)
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

static uint8_t poll_controller_input(void) {
    const bool* state = SDL_GetKeyboardState(NULL);
    uint8_t mask = 0;

    if (state[SDL_SCANCODE_X])     mask |= 0x80;
    if (state[SDL_SCANCODE_Z])     mask |= 0x40;
    if (state[SDL_SCANCODE_A])     mask |= 0x20;
    if (state[SDL_SCANCODE_S])     mask |= 0x10;
    if (state[SDL_SCANCODE_UP])    mask |= 0x08;
    if (state[SDL_SCANCODE_DOWN])  mask |= 0x04;
    if (state[SDL_SCANCODE_LEFT])  mask |= 0x02;
    if (state[SDL_SCANCODE_RIGHT]) mask |= 0x01;

    return mask;
}

int main(int argc, char** argv) {
    print_build_info();

    _gui gui;
    if (gui_init(&gui) != CNES_SUCCESS) {
        gui_deinit(&gui);
        return CNES_FAILURE;
    }

    _nes nes;
    if (argc > 1) {
        size_t path_len = strlen(argv[1]) + 1;
        nes.cart.rom_path = (char*)malloc(path_len);
        strncpy(nes.cart.rom_path, argv[1], path_len);
        gui_set_title(&gui, &nes.cart);
    } else {
        nes.cart.rom_path = NULL;
    }

    if (nes_init(&nes) != CNES_SUCCESS) {
        gui_deinit(&gui);
        nes_deinit(&nes);
        return CNES_FAILURE;
    }

    uint64_t perf_freq = SDL_GetPerformanceFrequency();
    double perf_freq_dbl = (double)perf_freq;
    uint64_t next_frame_target = SDL_GetPerformanceCounter();

#ifndef CNES_NO_STATS
    uint64_t last_stats_time = next_frame_target;
    uint64_t frame_count_stats = 0;
    uint64_t dropped_frames_stats = 0;
#endif

    double max_jitter = 0.0;
    double max_frame_time = 0.0;
    double min_frame_time = 1.0;

    while (!nes.cpu.halt && !gui.quit) {
        if (nes.hard_reset_pending) {
            nes_hard_reset(&nes);
            gui_set_title(&gui, &nes.cart);
        }

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
                gui.quit = 1;
                break;

            case SDL_EVENT_KEY_DOWN:
                if (ImGui_GetIO()->WantCaptureKeyboard) break;

                #ifdef __APPLE__
                    uint16_t shortcut_pressed = (event.key.mod & SDL_KMOD_GUI);
                #else
                    uint16_t shortcut_pressed = (event.key.mod & SDL_KMOD_CTRL);
                #endif

                if (shortcut_pressed) {
                    if (event.key.key == SDLK_O) {
                        gui_open_file_dialog(&gui, &nes);
                        continue;
                    } else if (event.key.key == SDLK_S) {
                        nes_soft_reset(&nes);
                        continue;
                    } else if (event.key.key == SDLK_R) {
                        nes_hard_reset(&nes);
                        continue;
                    } else if (shortcut_pressed && event.key.key == SDLK_Q) {
                        nes.cpu.halt = 1;
                        continue;
                    } else {
                        SDL_HideCursor();
                    }
                }

                break;

            case SDL_EVENT_MOUSE_MOTION:
                SDL_ShowCursor();
                break;
            }
        }

        nes.input.controller[0] = poll_controller_input();

        uint64_t work_start = SDL_GetPerformanceCounter();

        if (nes.cart.loaded) {
            nes_clock(&nes);
            apu_flush_audio(&nes.apu);
        }

        uint64_t current_time = SDL_GetPerformanceCounter();
        uint64_t frame_deadline = next_frame_target + (uint64_t)(NES_FRAME_TIME_SEC * perf_freq_dbl);

        uint8_t skip_draw = false;
        if (current_time > frame_deadline) {
            skip_draw = true;
#ifndef CNES_NO_STATS
            dropped_frames_stats++;
#endif
        }

        if (!skip_draw) {
            gui_draw(&gui, &nes);
        }

        uint64_t work_end = SDL_GetPerformanceCounter();
        double frame_work_time = (double)(work_end - work_start) / perf_freq_dbl;

        if (frame_work_time > max_frame_time) max_frame_time = frame_work_time;
        if (frame_work_time < min_frame_time) min_frame_time = frame_work_time;

        double adjustment = 0.0;

        if (nes.cart.loaded && nes.apu.audio_stream) {
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

#ifndef CNES_NO_STATS
        frame_count_stats++;
        double time_since_stats = (double)(now - last_stats_time) / perf_freq_dbl;

        if (time_since_stats >= 1.0) {
            double max_jitter_ms = max_jitter * 1000.0;
            double fps = frame_count_stats / time_since_stats;
            double avg_work = (max_frame_time + min_frame_time) / 2.0 * 1000.0;

            printf("[STATS] FPS: %.2f | Drop: %llu | Jitter: %4.2fms | Work: %.2fms | AudioQ: %d B\n",
                   fps,
                   (unsigned long long)dropped_frames_stats,
                   max_jitter_ms,
                   avg_work,
                   nes.apu.audio_stream ? SDL_GetAudioStreamQueued(nes.apu.audio_stream) : 0);

            last_stats_time = now;
            frame_count_stats = 0;
            dropped_frames_stats = 0;
            max_jitter = 0.0;
            max_frame_time = 0.0;
            min_frame_time = 1.0;
        }
#endif
    }

    nes_deinit(&nes);
    gui_deinit(&gui);

    return 0;
}
