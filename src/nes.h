#pragma once
#include "cpu.h"
#include "ppu.h"
#include "apu.h"
#include "cart.h"
#include "input.h"
#include <stdint.h>
#include <stddef.h>

typedef struct _nes {
    _cpu cpu;
    _ppu ppu;
    _apu apu;
    _cart cart;
    _input input;

    uint32_t master_clock;
    uint8_t cpu_div;
    uint8_t frame_complete;
} _nes;

void nes_init(_nes* nes);
void nes_reset(_nes* nes);
void nes_clock(_nes* nes);
