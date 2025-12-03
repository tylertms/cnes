#pragma once
#include <SDL3/SDL_audio.h>

#define CPU_FREQ_NTSC 1789773.0
#define SAMPLE_RATE 48000.0
#define APU_MAX_FRAME_SAMPLES 2048
#define RAMP_SAMPLES 64

#define FC4_STEP1  3728
#define FC4_STEP2  7456
#define FC4_STEP3  11185
#define FC4_STEP4  14914
#define FC4_PERIOD 14915

#define FC5_STEP1   3728
#define FC5_STEP2   7456
#define FC5_STEP3   11185
#define FC5_STEP4   14914
#define FC5_STEP5   18640
#define FC5_PERIOD  18641

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

    uint16_t timer_value;
    uint8_t step;
    uint8_t length;

    uint8_t env_start;
    uint8_t env_div;
    uint8_t env;

    uint8_t sweep_div;
    uint8_t sweep_reload;
    uint8_t sweep_mute;
} _pulse;

typedef struct _triangle {
    uint8_t counter_control;
    uint8_t linear_counter_load;
    uint8_t length_counter_load;
    uint16_t timer;

    uint16_t timer_value;
    uint8_t seq_step;
    uint8_t length;
    uint8_t linear_counter;
    uint8_t linear_reload;
} _triangle;

typedef struct _noise {
    uint8_t env_loop;
    uint8_t constant_volume;
    uint8_t volume_env;
    uint8_t mode;
    uint8_t period;
    uint8_t length_counter_load;

    uint8_t length;
    uint8_t env_start;
    uint8_t env_div;
    uint8_t env;

    uint16_t timer;
    uint16_t timer_value;

    uint16_t shift_reg;
} _noise;

typedef struct _dmc {
    uint8_t irq_enable;
    uint8_t loop;
    uint8_t frequency;
    uint8_t load_counter;
    uint8_t sample_address;
    uint8_t sample_length;

    uint16_t timer;
    uint16_t timer_value;

    uint8_t output;
    uint8_t shift_reg;
    uint8_t bits_remaining;

    uint8_t sample_buffer;
    uint8_t sample_buffer_empty;

    uint16_t current_address;
    uint16_t bytes_remaining;

    uint8_t silence;
    uint8_t irq_pending;

    uint8_t dma_active;
    uint8_t dma_cycles_left;
    uint16_t dma_addr;
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

typedef struct _cpu _cpu;

typedef struct _apu {
    SDL_AudioStream* audio_stream;
    uint8_t audio_retry;
    _cpu* p_cpu;

    float sample_buffer[APU_MAX_FRAME_SAMPLES];
    int sample_count;

    _pulse pulse1;
    _pulse pulse2;
    _triangle triangle;
    _noise noise;
    _dmc dmc;

    _status status;
    _frame_counter frame_counter;
    uint8_t frame_counter_irq;

    uint8_t apu_divider;
    double sample_acc;
    int frame_cycle;

    float dc_prev_in;
    float dc_prev_out;

    float pulse1_gain, pulse2_gain, triangle_gain, noise_gain, dmc_gain;
    int pulse1_ramp, pulse2_ramp, triangle_ramp, noise_ramp, dmc_ramp;
} _apu;

uint8_t apu_init(_apu *apu);
void apu_deinit(_apu *apu);
void apu_reset(_apu *apu);
void apu_clock(_apu *apu);
void apu_flush_audio(_apu *apu);

uint8_t apu_cpu_read(_apu *apu, uint16_t addr);
void apu_cpu_write(_apu *apu, uint16_t addr, uint8_t data);

void pulse1_cpu_write(_apu *apu, uint16_t addr, uint8_t data);
void pulse2_cpu_write(_apu *apu, uint16_t addr, uint8_t data);
void triangle_cpu_write(_apu *apu, uint16_t addr, uint8_t data);
void noise_cpu_write(_apu *apu, uint16_t addr, uint8_t data);
void dmc_cpu_write(_apu *apu, uint16_t addr, uint8_t data);
void dmc_dma_complete(_apu* apu);

void clock_pulse_envelope(_pulse *p);
void clock_pulse_length(_pulse *p);
void clock_pulse_sweep(_pulse *p, int is_pulse1);
void clock_pulse(_pulse *p);
uint8_t sample_pulse(_pulse *p);

void clock_triangle_linear(_triangle *t);
void clock_triangle_length(_triangle *t);
void clock_triangle(_triangle *t);
uint8_t sample_triangle(_triangle *t);

void clock_noise_envelope(_noise *n);
void clock_noise_length(_noise *n);
void clock_noise(_noise *n);
uint8_t sample_noise(_noise *n);

void clock_dmc(_apu *apu);
uint8_t sample_dmc(_dmc *d);
void clock_frame_counter(_apu *apu);
