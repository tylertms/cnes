#pragma once
#include <stdint.h>

typedef struct _ppu {

} _ppu;

void ppu_clock(_ppu* ppu);

uint8_t ppu_cpu_read(_ppu* ppu, uint16_t addr);
void ppu_cpu_write(_ppu* ppu, uint16_t addr, uint8_t data);

uint8_t ppu_read(_ppu* ppu, uint16_t addr);
void ppu_write(_ppu* ppu, uint16_t addr, uint8_t data);
