#pragma once
#include "cart.h"
#include <stdint.h>
#include <stdlib.h>

#define MAPDEF(num) \
    void map_init_##num(_cart* cart); \
    void map_deinit_##num(_cart* cart); \
    uint8_t map_cpu_read_##num(_cart* cart, uint16_t addr); \
    void map_cpu_write_##num(_cart* cart, uint16_t addr, uint8_t data); \
    uint8_t map_ppu_read_##num(_cart* cart, uint16_t addr); \
    void map_ppu_write_##num(_cart* cart, uint16_t addr, uint8_t data);

#define MAPPER(num) \
    [num] = {map_init_##num, map_deinit_##num, map_cpu_read_##num, map_cpu_write_##num, map_ppu_read_##num, map_ppu_write_##num, NULL}

/* DEFINES */
MAPDEF(000)
MAPDEF(001)
MAPDEF(002)
MAPDEF(003)

/* MAPPER TABLE */
static _mapper mappers[768] = {
    MAPPER(000),
    MAPPER(001),
    MAPPER(002),
    MAPPER(003)
};
