#pragma once
#include "cpu.h"
#include <stdint.h>
#include <stddef.h>

typedef struct _cart _cart;
typedef uint8_t (*map_fn_ctrl)(_cart*);
typedef uint8_t (*map_fn_read)(_cart*, uint16_t);
typedef void (*map_fn_write)(_cart*, uint16_t, uint8_t);

typedef enum {
    MIRROR_HORIZONTAL = 0,
    MIRROR_VERTICAL   = 1,
    MIRROR_SINGLE0    = 2,
    MIRROR_SINGLE1    = 3,
    MIRROR_FOUR       = 4
} _mirror;

typedef struct _mapper {
    map_fn_ctrl init;
    map_fn_ctrl deinit;
    map_fn_ctrl irq_pending;
    map_fn_read cpu_read;
    map_fn_write cpu_write;
    map_fn_read ppu_read;
    map_fn_write ppu_write;
    void* data;
} _mapper;

typedef struct _mem {
    uint8_t* data;
    size_t size;
    uint8_t writeable;
} _mem;

typedef struct _cart {
    uint8_t loaded;

    _mem prg_rom;
    _mem prg_ram;
    _mem prg_nvram;

    _mem chr_rom;
    _mem chr_ram;
    _mem chr_nvram;

    _mapper mapper;
    _mirror mirror;

    uint16_t prg_rom_banks;
    uint16_t chr_rom_banks;
    uint16_t mapper_id;
    uint8_t ntbl_layout;
    uint8_t has_prg_ram;
    uint8_t trainer;
    uint8_t alt_ntbl_layout;
    uint8_t prg_ram_banks;
    uint8_t tv_system;

    uint8_t prg_ram_shift;
    uint8_t prg_nvram_shift;
    uint8_t chr_ram_shift;
    uint8_t chr_nvram_shift;
    uint8_t cpu_ppu_timing;
    uint8_t misc_roms;
    uint8_t expansion_device;
} _cart;

uint8_t cart_load(_cart* cart, const char* file);
void cart_unload(_cart* cart);
uint8_t parse_ines(_cart* cart, uint8_t header[16]);
uint8_t parse_nes2(_cart* cart, uint8_t header[16]);

uint8_t cart_cpu_read(_cart* cart, uint16_t addr);
void cart_cpu_write(_cart* cart, uint16_t addr, uint8_t data);
uint8_t cart_ppu_read(_cart* cart, uint16_t addr);
void cart_ppu_write(_cart* cart, uint16_t addr, uint8_t data);
