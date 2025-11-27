#include "apu.h"
#include "cpu.h"
#include <SDL3/SDL_hints.h>
#include <stdio.h>

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
        NULL,
        NULL
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

void apu_clock(_apu* apu) {
    clock_triangle(&apu->triangle);
    clock_dmc(apu);

    apu->apu_divider ^= 1;
    if (apu->apu_divider == 0) {
        clock_pulse(&apu->pulse1);
        clock_pulse(&apu->pulse2);
        clock_noise(&apu->noise);
    }

    apu->env_acc += 1.0;
    apu->len_sweep_acc += 1.0;

    const double env_period = CPU_FREQ_NTSC / 240.0;
    const double len_sweep_period = CPU_FREQ_NTSC / 120.0;

    if (apu->env_acc >= env_period) {
        apu->env_acc -= env_period;
        clock_envelope(&apu->pulse1);
        clock_envelope(&apu->pulse2);
        clock_noise_envelope(&apu->noise);
        clock_triangle_linear(&apu->triangle);
    }

    if (apu->len_sweep_acc >= len_sweep_period) {
        apu->len_sweep_acc -= len_sweep_period;
        clock_length(&apu->pulse1);
        clock_length(&apu->pulse2);
        clock_noise_length(&apu->noise);
        clock_triangle_length(&apu->triangle);
        clock_sweep(&apu->pulse1, 1);
        clock_sweep(&apu->pulse2, 0);
    }

    const double s = SAMPLE_RATE / CPU_FREQ_NTSC;
    apu->sample_acc += s;

    if (apu->sample_acc >= 1.0) {
        apu->sample_acc -= 1.0;

        uint8_t raw_pulse1 = sample_pulse(&apu->pulse1, apu->status.enable_pulse1);
        uint8_t raw_pulse2 = sample_pulse(&apu->pulse2, apu->status.enable_pulse2);
        uint8_t raw_triangle = sample_triangle(&apu->triangle, apu->status.enable_triangle);
        uint8_t raw_noise = sample_noise(&apu->noise, apu->status.enable_noise);
        uint8_t raw_dmc = sample_dmc(&apu->dmc, apu->status.enable_dmc);

        int gate_pulse1 = apu->status.enable_pulse1 && apu->pulse1.length > 0 &&
                      apu->pulse1.timer >= 8 && !apu->pulse1.sweep_mute &&
                      ((apu->pulse1.constant_volume ? apu->pulse1.volume_env : apu->pulse1.env) > 0);

        int gate_pulse2 = apu->status.enable_pulse2 && apu->pulse2.length > 0 &&
                      apu->pulse2.timer >= 8 && !apu->pulse2.sweep_mute &&
                      ((apu->pulse2.constant_volume ? apu->pulse2.volume_env : apu->pulse2.env) > 0);

        int gate_triangle = apu->status.enable_triangle && apu->triangle.length > 0 &&
                       apu->triangle.linear_counter > 0 && apu->triangle.timer >= 2;

        int gate_noise = apu->status.enable_noise && apu->noise.length > 0 &&
                         ((apu->noise.constant_volume ? apu->noise.volume_env : apu->noise.env) > 0);

        int gate_dmc = apu->status.enable_dmc && !apu->dmc.silence;

        apu->pulse1_release = release_smooth(apu->pulse1_release, gate_pulse1);
        apu->pulse2_release = release_smooth(apu->pulse2_release, gate_pulse2);
        apu->triangle_release = release_smooth(apu->triangle_release, gate_triangle);
        apu->noise_release = release_smooth(apu->noise_release, gate_noise);
        apu->dmc_release = release_smooth(apu->dmc_release, gate_dmc);

        float s_pulse1 = raw_pulse1 * apu->pulse1_release;
        float s_pulse2 = raw_pulse2 * apu->pulse2_release;
        float s_triangle = raw_triangle * apu->triangle_release;
        float s_noise = raw_noise * apu->noise_release;
        float s_dmc = raw_dmc * apu->dmc_release;

        float sample = mix(s_pulse1, s_pulse2, s_triangle, s_noise, s_dmc);
        apu->sample_buffer[apu->sample_write++] = sample * GLOBAL_VOLUME;

        if (apu->sample_write == APU_BUFFER_SAMPLES) {
            SDL_PutAudioStreamData(
                apu->audio_stream,
                apu->sample_buffer,
                APU_BUFFER_SAMPLES * sizeof(float)
            );
            apu->sample_write = 0;
        }
    }
}

void apu_reset(_apu* apu) {
    _apu saved = *apu;
    memset(apu, 0, sizeof(_apu));

    apu->audio_stream = saved.audio_stream;
    apu->p_cpu = saved.p_cpu;

    apu->noise.shift_reg = 1;
    apu->noise.timer = noise_period[0];
    apu->noise.timer_value = apu->noise.timer;

    apu->dmc.timer = dmc_period[0];
    apu->dmc.timer_value = apu->dmc.timer;
    apu->dmc.bits_remaining = 8;
    apu->dmc.sample_buffer_empty = 1;
    apu->dmc.silence = 1;
}

uint8_t apu_cpu_read(_apu* apu, uint16_t addr) {
    if (addr != 0x4015) return 0x00;

    uint8_t data = 0;
    if (apu->pulse1.length   > 0) data |= 0x01;
    if (apu->pulse2.length   > 0) data |= 0x02;
    if (apu->triangle.length > 0) data |= 0x04;
    if (apu->noise.length    > 0) data |= 0x08;
    if (apu->dmc.bytes_remaining > 0) data |= 0x10;
    if (apu->dmc.irq_pending) data |= 0x40;
    if (apu->frame_counter_irq) data |= 0x80;

    apu->frame_counter_irq = 0;

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
        _status prev = apu->status;

        apu->status.enable_dmc = (data & 0x10) >> 4;
        apu->status.enable_noise = (data & 0x08) >> 3;
        apu->status.enable_triangle = (data & 0x04) >> 2;
        apu->status.enable_pulse2 = (data & 0x02) >> 1;
        apu->status.enable_pulse1 = (data & 0x01) >> 0;

        if (!apu->status.enable_pulse1 && prev.enable_pulse1)
            apu->pulse1.length = 0;
        if (!apu->status.enable_pulse2 && prev.enable_pulse2)
            apu->pulse2.length = 0;
        if (!apu->status.enable_triangle && prev.enable_triangle)
            apu->triangle.length = 0;
        if (!apu->status.enable_noise && prev.enable_noise)
            apu->noise.length = 0;

        if (!apu->status.enable_dmc) {
            apu->dmc.bytes_remaining = 0;
        } else if (!prev.enable_dmc && apu->status.enable_dmc) {
            if (apu->dmc.bytes_remaining == 0) {
                apu->dmc.current_address =
                    0xC000u + ((uint16_t)apu->dmc.sample_address << 6);
                apu->dmc.bytes_remaining =
                    ((uint16_t)apu->dmc.sample_length << 4) + 1;
            }
        }

        apu->dmc.irq_pending = 0;

        if (apu->status.enable_triangle && !prev.enable_triangle) {
            apu->triangle.seq_step = 15;
            apu->triangle.timer_value = apu->triangle.timer;
            apu->triangle.active = 0;
        }
    }
}

float mix(float pulse1, float pulse2, float triangle, float noise, float dmc) {
    float pulse_sum = pulse1 + pulse2;
    float tnd_sum   = triangle / 8227.0f + noise / 12241.0f + dmc / 22638.0f;

    float pulse_out = 0.0f;
    if (pulse_sum > 0.0f) {
        pulse_out = 95.88f / (8128.0f / pulse_sum + 100.0f);
    }

    float tnd_out = 0.0f;
    if (tnd_sum > 0.0f) {
        tnd_out = 159.79f / (1.0f / tnd_sum + 100.0f);
    }

    return pulse_out + tnd_out;
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
            apu->pulse1.sweep_reload = 1;
            break;

        case 0x4002:
            apu->pulse1.timer &= 0x0700;
            apu->pulse1.timer |= data;
            break;

        case 0x4003:
            apu->pulse1.timer &= 0x00FF;
            apu->pulse1.timer |= (uint16_t)(data & 0x07) << 8;
            apu->pulse1.length_counter_load = (data & 0xF8) >> 3;

            apu->pulse1.step = 0;
            apu->pulse1.env_start = 1;
            apu->pulse1.length = length_table[apu->pulse1.length_counter_load];
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
            apu->pulse2.sweep_reload = 1;
            break;

        case 0x4006:
            apu->pulse2.timer &= 0x0700;
            apu->pulse2.timer |= data;
            break;

        case 0x4007:
            apu->pulse2.timer &= 0x00FF;
            apu->pulse2.timer |= (uint16_t)(data & 0x07) << 8;
            apu->pulse2.length_counter_load = (data & 0xF8) >> 3;

            apu->pulse2.step = 0;
            apu->pulse2.env_start = 1;
            apu->pulse2.length = length_table[apu->pulse2.length_counter_load];
            break;

        default: break;
    }
}

void clock_envelope(_pulse* p) {
    if (p->env_start) {
        p->env_start = 0;
        p->env = 15;
        p->env_div = p->volume_env;
    } else {
        if (p->env_div == 0) {
            p->env_div = p->volume_env;
            if (p->env > 0) {
                p->env--;
            } else if (p->env_loop) {
                p->env = 15;
            }
        } else {
            p->env_div--;
        }
    }
}

void clock_length(_pulse* p) {
    if (!p->env_loop && p->length > 0) {
        p->length--;
    }
}

static uint16_t sweep_target(_pulse* p, int is_pulse1) {
    uint16_t t = p->timer;
    uint16_t delta = t >> p->shift;
    if (!p->negate) {
        return t + delta;
    } else {
        if (is_pulse1)
            return t - delta - 1;
        else
            return t - delta;
    }
}

void clock_sweep(_pulse* p, int is_pulse1) {
    if (!p->sweep_enable || p->shift == 0) {
        p->sweep_mute = 0;
        return;
    }

    if (p->sweep_reload) {
        p->sweep_reload = 0;
        p->sweep_div = p->period;
    } else {
        if (p->sweep_div > 0) {
            p->sweep_div--;
        } else {
            p->sweep_div = p->period;
            uint16_t target = sweep_target(p, is_pulse1);
            if (target <= 0x7FF && p->timer >= 8) {
                p->timer = target;
            }
        }
    }

    uint16_t target = sweep_target(p, is_pulse1);
    if (target > 0x7FF) {
        p->sweep_mute = 1;
    } else {
        p->sweep_mute = 0;
    }
}

void clock_pulse(_pulse* p) {
    if (p->timer_value == 0) {
        p->timer_value = p->timer;
        p->step = (p->step - 1) & 7;
    } else {
        p->timer_value--;
    }
}

uint8_t sample_pulse(_pulse* p, uint8_t enabled) {
    uint8_t vol = p->constant_volume ? p->volume_env : p->env;

    uint8_t on = enabled && p->length > 0 &&
        p->timer >= 8 && !p->sweep_mute && (vol > 0);

    uint8_t bit = (pulse_duty[p->duty] >> p->step) & 1;

    if (on) {
        p->active = 1;
    } else if (p->active && bit == 0) {
        p->active = 0;
    }

    if (!p->active) return 0;
    if (!bit) return 0;

    return vol & 0x0F;
}

#define FC_PERIOD 29830
#define FC_STEP1  3729
#define FC_STEP2  7457
#define FC_STEP3  11186
#define FC_STEP4  14916

void clock_frame_counter(_apu* apu) {
    apu->frame_cycle++;
    int c = apu->frame_cycle;

    if (c == FC_STEP1 || c == FC_STEP2 || c == FC_STEP3 || c == FC_STEP4) {
        clock_envelope(&apu->pulse1);
        clock_envelope(&apu->pulse2);
    }

    if (apu->frame_cycle >= FC_PERIOD) {
        apu->frame_cycle -= FC_PERIOD;
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

            apu->triangle.linear_reload = 1;
            apu->triangle.seq_step = 15;
            apu->triangle.length = length_table[apu->triangle.length_counter_load];
            break;

        default: break;
    }
}

void clock_triangle_linear(_triangle* t) {
    if (t->linear_reload) {
        t->linear_counter = t->linear_counter_load;
    } else if (t->linear_counter > 0) {
        t->linear_counter--;
    }

    if (!t->counter_control) {
        t->linear_reload = 0;
    }
}

void clock_triangle_length(_triangle* t) {
    if (!t->counter_control && t->length > 0) {
        t->length--;
    }
}

void clock_triangle(_triangle* t) {
    if (t->timer < 2) return;

    if (t->timer_value == 0) {
        t->timer_value = t->timer;
        if (t->length > 0 && t->linear_counter > 0) {
            t->seq_step = (t->seq_step + 1) & 31;
        }
    } else {
        t->timer_value--;
    }
}

uint8_t sample_triangle(_triangle* t, uint8_t enabled) {
    uint8_t on = enabled && t->length > 0 &&
        t->linear_counter > 0 && t->timer >= 2;

    uint8_t value = triangle_seq[t->seq_step];

    if (on) {
        t->active = 1;
    } else if (t->active && value == 0) {
        t->active = 0;
    }

    if (!t->active) return 0;

    return value;
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
            apu->noise.timer  = noise_period[apu->noise.period];
            break;

        case 0x400F:
            apu->noise.length_counter_load = (data & 0xF8) >> 3;
            apu->noise.env_start = 1;
            apu->noise.length = length_table[apu->noise.length_counter_load];
            apu->noise.timer_value = apu->noise.timer;
            break;

        default: break;
    }
}

void clock_noise_envelope(_noise* n) {
    if (n->env_start) {
        n->env_start = 0;
        n->env = 15;
        n->env_div = n->volume_env;
    } else {
        if (n->env_div == 0) {
            n->env_div = n->volume_env;
            if (n->env > 0) {
                n->env--;
            } else if (n->env_loop) {
                n->env = 15;
            }
        } else {
            n->env_div--;
        }
    }
}

void clock_noise_length(_noise* n) {
    if (!n->env_loop && n->length > 0) {
        n->length--;
    }
}

void clock_noise(_noise* n) {
    if (n->timer_value == 0) {
        n->timer_value = n->timer;

        uint16_t feedback;
        if (n->mode) {
            feedback = ((n->shift_reg & 0x0001) ^ ((n->shift_reg & 0x0040) >> 6));
        } else {
            feedback = ((n->shift_reg & 0x0001) ^ ((n->shift_reg & 0x0002) >> 1));
        }

        n->shift_reg >>= 1;
        if (feedback) {
            n->shift_reg |= 0x4000;
        } else {
            n->shift_reg &= ~0x4000;
        }
    } else {
        n->timer_value--;
    }
}

uint8_t sample_noise(_noise* n, uint8_t enabled) {
    uint8_t vol = n->constant_volume ? n->volume_env : n->env;

    if (!enabled) return 0;
    if (n->length == 0) return 0;
    if (vol == 0) return 0;
    if (n->shift_reg & 1) return 0;

    return vol & 0x0F;
}

void dmc_cpu_write(_apu* apu, uint16_t addr, uint8_t data) {
    switch (addr) {
        case 0x4010:
            apu->dmc.irq_enable = (data & 0x80) >> 7;
            apu->dmc.loop = (data & 0x40) >> 6;
            apu->dmc.frequency = (data & 0x0F) >> 0;
            apu->dmc.timer = dmc_period[apu->dmc.frequency];
            break;

        case 0x4011:
            apu->dmc.load_counter = (data & 0x7F) >> 0;
            apu->dmc.output = apu->dmc.load_counter;
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

static void dmc_start_sample(_apu* apu) {
    _dmc* d = &apu->dmc;

    d->current_address =
        0xC000u + ((uint16_t)d->sample_address << 6);
    d->bytes_remaining =
        ((uint16_t)d->sample_length << 4) + 1;
}

static void dmc_fill_sample_buffer(_apu* apu) {
    _dmc* d = &apu->dmc;

    if (d->bytes_remaining == 0) return;
    if (!d->sample_buffer_empty) return;
    if (d->dma_active) return;

    d->dma_active = 1;
    d->dma_cycles_left = 4;
    d->dma_addr = d->current_address;
}

void dmc_dma_complete(_apu* apu) {
    _dmc* d = &apu->dmc;
    uint8_t b = cpu_read(apu->p_cpu, d->dma_addr);

    d->sample_buffer = b;
    d->sample_buffer_empty = 0;

    d->current_address++;
    if (d->current_address == 0x0000) {
        d->current_address = 0x8000;
    }

    d->bytes_remaining--;
    if (d->bytes_remaining == 0) {
        if (d->loop) {
            dmc_start_sample(apu);
        } else if (d->irq_enable) {
            d->irq_pending = 1;
        }
    }
}

void clock_dmc(_apu* apu) {
    _dmc* d = &apu->dmc;

    if (!apu->status.enable_dmc) return;

    if (d->timer_value == 0) {
        d->timer_value = d->timer;

        if (!d->silence) {
            if (d->shift_reg & 1) {
                if (d->output <= 125) d->output += 2;
            } else {
                if (d->output >= 2) d->output -= 2;
            }
        }

        d->shift_reg >>= 1;
        d->bits_remaining--;
        if (d->bits_remaining == 0) {
            d->bits_remaining = 8;
            if (d->sample_buffer_empty) {
                d->silence = 1;
            } else {
                d->silence = 0;
                d->shift_reg = d->sample_buffer;
                d->sample_buffer_empty = 1;
            }
        }

        dmc_fill_sample_buffer(apu);
    } else {
        d->timer_value--;
    }
}

uint8_t sample_dmc(_dmc* d, uint8_t enabled) {
    if (!enabled) return 0;
    return d->output;
}
