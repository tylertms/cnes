#pragma once
#include "cart.h"
#include <stdint.h>
#include <stdlib.h>

#define MAPDEF(num) \
    uint8_t map_init_##num(_cart* cart); \
    uint8_t map_deinit_##num(_cart* cart); \
    uint8_t map_irq_pending_##num(_cart* cart); \
    uint8_t map_cpu_read_##num(_cart* cart, uint16_t addr); \
    void map_cpu_write_##num(_cart* cart, uint16_t addr, uint8_t data); \
    uint8_t map_ppu_read_##num(_cart* cart, uint16_t addr); \
    void map_ppu_write_##num(_cart* cart, uint16_t addr, uint8_t data); \

#define MAPPER(num) \
    [num] = {map_init_##num, map_deinit_##num, map_irq_pending_##num, map_cpu_read_##num, map_cpu_write_##num, map_ppu_read_##num, map_ppu_write_##num, NULL}

/* DEFINES */
MAPDEF(0)
MAPDEF(1)
MAPDEF(2)
MAPDEF(3)
MAPDEF(4)
MAPDEF(7)
MAPDEF(79)
MAPDEF(148)

/* MAPPER TABLE */
static const _mapper mappers[768] = {
    MAPPER(0),
    MAPPER(1),
    MAPPER(2),
    MAPPER(3),
    MAPPER(4),
    MAPPER(7),
    MAPPER(79),
    MAPPER(148)
};

/* MISC */
void mmc3_scanline_tick(_cart* cart);
