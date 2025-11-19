#pragma once
#include "cart.h"
#include <stdint.h>

#define MAPDEF(num) \
    uint8_t map_cpu_read_##num(_cart* cart, uint16_t addr); \
    void map_cpu_write_##num(_cart* cart, uint16_t addr, uint8_t data); \
    uint8_t map_ppu_read_##num(_cart* cart, uint16_t addr); \
    void map_ppu_write_##num(_cart* cart, uint16_t addr, uint8_t data);

#define MAPPER(num) \
    {map_cpu_read_##num, map_cpu_write_##num, map_ppu_read_##num, map_ppu_write_##num}

MAPDEF(000);

static _mapper mappers[768] = {
    [000] = MAPPER(000)
};
