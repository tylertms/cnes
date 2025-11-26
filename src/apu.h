#pragma once
#include <SDL3/SDL_audio.h>

#define SAMPLE_RATE 48000.f

typedef struct _pulse {
    uint8_t duty;
    uint8_t env_loop;
    uint8_t constant_volume;
    uint8_t volume_env;
    uint8_t sweep_enable;
    uint8_t period;
    uint8_t negate;
    uint8_t shift;
    uint8_t length_counter_load;
    uint16_t timer;
} _pulse;

typedef struct _triangle {
    uint8_t counter_control;
    uint8_t linear_counter_load;
    uint8_t length_counter_load;
    uint16_t timer;
} _triangle;

typedef struct _noise {
    uint8_t env_loop;
    uint8_t constant_volume;
    uint8_t volume_env;
    uint8_t mode;
    uint8_t period;
    uint8_t length_counter_load;
} _noise;

typedef struct _dmc {
    uint8_t irq_enable;
    uint8_t loop;
    uint8_t frequency;
    uint8_t load_counter;
    uint8_t sample_address;
    uint8_t sample_length;
} _dmc;

typedef struct _status {
    uint8_t enable_dmc;
    uint8_t enable_noise;
    uint8_t enable_triangle;
    uint8_t enable_pulse2;
    uint8_t enable_pulse1;
} _status;

typedef struct _frame_counter {
    uint8_t mode;
    uint8_t irq_inhibit;
} _frame_counter;

typedef struct _apu {
    SDL_AudioStream* audio_stream;

    _pulse pulse1;
    _pulse pulse2;
    _triangle triangle;
    _noise noise;
    _dmc dmc;

    _status status;
    _frame_counter frame_counter;
} _apu;

void apu_init(_apu* apu);
void apu_deinit(_apu* apu);
void apu_callback(void *userdata, SDL_AudioStream *astream, int additional_amount, int total_amount);

void apu_clock(_apu* apu);
void apu_reset(_apu* apu);

uint8_t apu_cpu_read(_apu* apu, uint16_t addr);
void apu_cpu_write(_apu* apu, uint16_t addr, uint8_t data);

void pulse1_cpu_write(_apu* apu, uint16_t addr, uint8_t data);
void pulse2_cpu_write(_apu* apu, uint16_t addr, uint8_t data);
void triangle_cpu_write(_apu* apu, uint16_t addr, uint8_t data);
void noise_cpu_write(_apu* apu, uint16_t addr, uint8_t data);
void dmc_cpu_write(_apu* apu, uint16_t addr, uint8_t data);
