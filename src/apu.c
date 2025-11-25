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

    uint32_t sine_sample_index = ((_apu*)userdata)->sine_sample_index;
    for (int i = 0; i < samples; i++) {
        float t = (float)sine_sample_index / SAMPLE_RATE;
        tmp[i] = 0;// = sinf(2.0f * M_PI * freq * t) * volume;
        sine_sample_index++;

        if (sine_sample_index >= (int)SAMPLE_RATE)
            sine_sample_index = 0;
    }
    ((_apu*)userdata)->sine_sample_index = sine_sample_index;

    SDL_PutAudioStreamData(astream, tmp, samples * sizeof(float));
}

void apu_clock(_apu* apu) {

}

void apu_reset(_apu* apu) {

}

uint8_t apu_cpu_read(_apu* apu, uint16_t addr) {

}

void apu_cpu_write(_apu* apu, uint16_t addr, uint8_t data) {

}
