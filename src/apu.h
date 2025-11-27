#pragma once
#include <SDL3/SDL_audio.h>

#define CPU_FREQ_NTSC 1789773.0
#define SAMPLE_RATE 48000.0
#define APU_BUFFER_SAMPLES 512
#define GLOBAL_VOLUME 2.0

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

    uint8_t active;
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

    uint8_t active;
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
    _cpu* p_cpu;

    float sample_buffer[APU_BUFFER_SAMPLES];
    int sample_write;

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
    double env_acc;
    double len_sweep_acc;
    int frame_cycle;

    float pulse1_release;
    float pulse2_release;
    float triangle_release;
    float noise_release;
    float dmc_release;
} _apu;

static const uint8_t pulse_duty[4] = {
    0x01, 0x03, 0x0F, 0xFC
};

static const uint8_t length_table[32] = {
    10, 254, 20,  2, 40,  4, 80,  6,
   160,   8, 60, 10, 14, 12, 26, 14,
    12,  16, 24, 18, 48, 20, 96, 22,
   192,  24, 72, 26, 16, 28, 32, 30
};

static const uint8_t triangle_seq[32] = {
    15, 14, 13, 12, 11, 10,  9,  8,
     7,  6,  5,  4,  3,  2,  1,  0,
     0,  1,  2,  3,  4,  5,  6,  7,
     8,  9, 10, 11, 12, 13, 14, 15
};

static const uint16_t noise_period[16] = {
      4,    8,   16,   32,
     64,   96,  128,  160,
    202,  254,  380,  508,
    762, 1016, 2034, 4068
};

static const uint16_t dmc_period[16] = {
    428, 380, 340, 320,
    286, 254, 226, 214,
    190, 160, 142, 128,
    106,  85,  72,  54
};

static inline float release_smooth(float rel, int gate_on) {
    const float r = 0.995f;
    return gate_on ? 1.0f : rel * r;
}

void clock_envelope(_pulse *p);
void clock_frame_counter(_apu *apu);
void clock_length(_pulse *p);
void clock_sweep(_pulse *p, int is_pulse1);
void clock_pulse(_pulse *p);
uint8_t sample_pulse(_pulse *p, uint8_t enabled);

void clock_triangle_linear(_triangle *t);
void clock_triangle_length(_triangle *t);
void clock_triangle(_triangle *t);
uint8_t sample_triangle(_triangle *t, uint8_t enabled);

void clock_noise_envelope(_noise *n);
void clock_noise_length(_noise *n);
void clock_noise(_noise *n);
uint8_t sample_noise(_noise *n, uint8_t enabled);

void clock_dmc(_apu *apu);
uint8_t sample_dmc(_dmc *d, uint8_t enabled);

float mix(float pulse1, float pulse2, float triangle, float noise, float dmc);

void apu_init(_apu *apu);
void apu_deinit(_apu *apu);
void apu_clock(_apu *apu);
void apu_reset(_apu *apu);

uint8_t apu_cpu_read(_apu *apu, uint16_t addr);
void apu_cpu_write(_apu *apu, uint16_t addr, uint8_t data);

void pulse1_cpu_write(_apu *apu, uint16_t addr, uint8_t data);
void pulse2_cpu_write(_apu *apu, uint16_t addr, uint8_t data);
void triangle_cpu_write(_apu *apu, uint16_t addr, uint8_t data);
void noise_cpu_write(_apu *apu, uint16_t addr, uint8_t data);
void dmc_cpu_write(_apu *apu, uint16_t addr, uint8_t data);
