#pragma once
#include "apu.h"
#include "cart.h"
#include "cpu.h"
#include "input.h"
#include "ppu.h"

typedef struct _nes {
    _cpu cpu;
    _ppu ppu;
    _apu apu;
    _cart cart;
    _input input;

    size_t master_clock;
    uint8_t hard_reset_pending;
} _nes;

CNES_RESULT nes_init(_nes* nes);
void nes_deinit(_nes* nes);
void nes_soft_reset(_nes* nes);
void nes_hard_reset(_nes* nes);
void nes_clock(_nes* nes);
