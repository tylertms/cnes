#include "apu.h"
#include <SDL3/SDL_hints.h>
#include <stdio.h>
#include <math.h>

void apu_init(_apu* apu) {
    SDL_AudioSpec spec;
    SDL_zero(spec);
    spec.channels = 1;
    spec.format = SDL_AUDIO_F32;
    spec.freq = SAMPLE_RATE;

    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "512");

    apu->audio_stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
        &spec,
        apu_callback,
        apu
    );

    if (!apu->audio_stream) {
        printf("Failed to open audio stream: %s\n", SDL_GetError());
    } else {
        SDL_ResumeAudioStreamDevice(apu->audio_stream);
    }
}

void apu_deinit(_apu* apu) {
    if (apu->audio_stream) {
        SDL_DestroyAudioStream(apu->audio_stream);
        apu->audio_stream = NULL;
    }
}

void apu_callback(void *userdata, SDL_AudioStream *astream, int additional_amount, int total_amount) {
    const float freq = 440.0f;
    const float volume = 0.2f;

    int samples = additional_amount / sizeof(float);
    float tmp[4096];

    if (samples > 4096) samples = 4096;

    //uint32_t sine_sample_index = ((_apu*)userdata)->sine_sample_index;
    /*for (int i = 0; i < samples; i++) {
        float t = (float)sine_sample_index / SAMPLE_RATE;
        tmp[i] = 0;// = sinf(2.0f * M_PI * freq * t) * volume;
        sine_sample_index++;

        if (sine_sample_index >= (int)SAMPLE_RATE)
            sine_sample_index = 0;
            }*/
    //((_apu*)userdata)->sine_sample_index = sine_sample_index;

    SDL_PutAudioStreamData(astream, tmp, samples * sizeof(float));
}

void apu_clock(_apu* apu) {

}

void apu_reset(_apu* apu) {

}

uint8_t apu_cpu_read(_apu* apu, uint16_t addr) {
    uint8_t data = 0x00;

    if (addr == 0x4015) {
        data = 0x00;
    }

    return data;
}

void apu_cpu_write(_apu* apu, uint16_t addr, uint8_t data) {
    if (0x4000 <= addr && addr <= 0x4003) {
        pulse1_cpu_write(apu, addr, data);
    } else if (0x4004 <= addr && addr <= 0x4007) {
        pulse2_cpu_write(apu, addr, data);
    } else if (0x4008 <= addr && addr <= 0x400B) {
        triangle_cpu_write(apu, addr, data);
    } else if (0x400C <= addr && addr <= 0x400F) {
        noise_cpu_write(apu, addr, data);
    } else if (0x4010 <= addr && addr <= 0x4013) {
        dmc_cpu_write(apu, addr, data);
    } else if (addr == 0x4015) {
        apu->status.enable_dmc = (data & 0x10) >> 4;
        apu->status.enable_noise = (data & 0x08) >> 3;
        apu->status.enable_triangle = (data & 0x04) >> 2;
        apu->status.enable_pulse2 = (data & 0x02) >> 1;
        apu->status.enable_pulse1 = (data & 0x01) >> 0;
    }
}

void pulse1_cpu_write(_apu* apu, uint16_t addr, uint8_t data) {
    switch (addr) {
        case 0x4000:
            apu->pulse1.duty = (data & 0xC0) >> 6;
            apu->pulse1.env_loop = (data & 0x20) >> 5;
            apu->pulse1.constant_volume = (data & 0x10) >> 4;
            apu->pulse1.volume_env = (data & 0x0F) >> 0;
            break;

        case 0x4001:
            apu->pulse1.sweep_enable = (data & 0x80) >> 7;
            apu->pulse1.period = (data & 0x70) >> 4;
            apu->pulse1.negate = (data & 0x08) >> 3;
            apu->pulse1.shift = (data & 0x07) >> 0;
            break;

        case 0x4002:
            apu->pulse1.timer &= 0x0700;
            apu->pulse1.timer |= data;
            break;

        case 0x4003:
            apu->pulse1.timer &= 0x00FF;
            apu->pulse1.timer |= (uint16_t)(data & 0x07) << 8;
            apu->pulse1.length_counter_load = (data & 0xF8) >> 3;
            break;

        default: break;
    }
}

void pulse2_cpu_write(_apu* apu, uint16_t addr, uint8_t data) {
    switch (addr) {
        case 0x4004:
            apu->pulse2.duty = (data & 0xC0) >> 6;
            apu->pulse2.env_loop = (data & 0x20) >> 5;
            apu->pulse2.constant_volume = (data & 0x10) >> 4;
            apu->pulse2.volume_env = (data & 0x0F) >> 0;
            break;

        case 0x4005:
            apu->pulse2.sweep_enable = (data & 0x80) >> 7;
            apu->pulse2.period = (data & 0x70) >> 4;
            apu->pulse2.negate = (data & 0x08) >> 3;
            apu->pulse2.shift = (data & 0x07) >> 0;
            break;

        case 0x4006:
            apu->pulse2.timer &= 0x0700;
            apu->pulse2.timer |= data;
            break;

        case 0x4007:
            apu->pulse2.timer &= 0x00FF;
            apu->pulse2.timer |= (uint16_t)(data & 0x07) << 8;
            apu->pulse2.length_counter_load = (data & 0xF8) >> 3;
            break;

        default: break;
    }
}

void triangle_cpu_write(_apu* apu, uint16_t addr, uint8_t data) {
    switch (addr) {
        case 0x4008:
            apu->triangle.counter_control = (data & 0x80) >> 7;
            apu->triangle.linear_counter_load = (data & 0x7F) >> 0;
            break;

        case 0x400A:
            apu->triangle.timer &= 0x0700;
            apu->triangle.timer |= data;
            break;

        case 0x400B:
            apu->triangle.timer &= 0x00FF;
            apu->triangle.timer |= (uint16_t)(data & 0x07) << 8;
            apu->triangle.length_counter_load = (data & 0xF8) >> 3;
            break;

        default: break;
    }
}

void noise_cpu_write(_apu* apu, uint16_t addr, uint8_t data) {
    switch (addr) {
        case 0x400C:
            apu->noise.env_loop = (data & 0x20) >> 5;
            apu->noise.constant_volume = (data & 0x10) >> 4;
            apu->noise.volume_env = (data & 0x0F) >> 0;
            break;

        case 0x400E:
            apu->noise.mode = (data & 0x80) >> 7;
            apu->noise.period = (data & 0x0F) >> 0;
            break;

        case 0x400F:
            apu->noise.length_counter_load = (data & 0xF8) >> 3;
            break;

        default: break;
    }
}

void dmc_cpu_write(_apu* apu, uint16_t addr, uint8_t data) {
    switch (addr) {
        case 0x4010:
            apu->dmc.irq_enable = (data & 0x80) >> 7;
            apu->dmc.loop = (data & 0x40) >> 6;
            apu->dmc.frequency = (data & 0x0F) >> 0;
            break;

        case 0x4011:
            apu->dmc.load_counter = (data & 0x7F) >> 0;
            break;

        case 0x4012:
            apu->dmc.sample_address = data;
            break;

        case 0x4013:
            apu->dmc.sample_length = data;
            break;

        default: break;
    }
}
