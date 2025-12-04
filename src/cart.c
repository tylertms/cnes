#include "cart.h"
#include "mapper.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

CNES_RESULT cart_load(_cart* cart) {
    FILE* rom = fopen(cart->rom_path, "rb");
    if (rom == NULL) {
        fprintf(stderr, "ERROR: Failed to open .nes file!\n");
        return CNES_FAILURE;
    }

    uint8_t header[0x10];
    if (fread(header, 1, 0x10, rom) != 0x10) {
        fprintf(stderr, "ERROR: Failed to read header from .nes file!\n");
        return CNES_FAILURE;
    };

    if (memcmp(header, "NES\x1A", 4)) {
        fprintf(stderr, "ERROR: .nes file is not valid!\n");
        return CNES_FAILURE;
    }

    cart->prg_rom_banks = header[4];
    cart->chr_rom_banks = header[5];
    cart->ntbl_layout = header[6] & 0x01;
    cart->trainer = header[6] & 0x04;
    cart->alt_ntbl_layout = header[6] & 0x08;
    cart->mapper_id = (header[6] >> 4) | (header[7] & 0xF0);

    if (cart->alt_ntbl_layout) cart->mirror = MIRROR_FOUR;
    else cart->mirror = cart->ntbl_layout ? MIRROR_VERTICAL : MIRROR_HORIZONTAL;

    uint8_t nes2 = (header[7] & 0x0C) == 0x08;
    if (nes2) parse_nes2(cart, header);
    else parse_ines(cart, header);

    if (mapper_load(cart) != CNES_SUCCESS) {
        fprintf(stderr, "ERROR: Failed to find and initialize mapper!\n");
        return CNES_FAILURE;
    }

    if (cart->trainer)
        fseek(rom, 0x200, SEEK_CUR);

    size_t prg_rom_size = cart->prg_rom_banks * 0x4000;
    size_t chr_rom_size = cart->chr_rom_banks * 0x2000;


    size_t prg_ram_size = 0, prg_nvram_size = 0;
    size_t chr_ram_size = 0, chr_nvram_size = 0;
    if (nes2) {
        prg_ram_size = cart->prg_ram_shift ? (size_t)64 << cart->prg_ram_shift : 0;
        prg_nvram_size = cart->prg_nvram_shift ? (size_t)64 << cart->prg_nvram_shift : 0;
        chr_ram_size = cart->chr_ram_shift ? (size_t)64 << cart->chr_ram_shift : 0;
        chr_nvram_size = cart->chr_nvram_shift ? (size_t)64 << cart->chr_nvram_shift : 0;
    } else {
        if (cart->prg_ram_banks == 0) prg_ram_size = 0x2000;
        else prg_ram_size = cart->prg_ram_banks * 0x2000;
        if (cart->chr_rom_banks == 0) chr_ram_size = 0x2000;
    }

    if (prg_rom_size) {
        cart->prg_rom = (_mem){
            .data = calloc(1, prg_rom_size),
            .size = prg_rom_size,
            .writeable = 0
        };

        size_t prg_rom_read = fread(cart->prg_rom.data, 0x01, prg_rom_size, rom);
        if (prg_rom_read != prg_rom_size) {
            fprintf(stderr, "ERROR: Failed to read program rom from .nes file!\n");
            return CNES_FAILURE;
        }
    }

    if (chr_rom_size) {
        cart->chr_rom = (_mem){
            .data = calloc(1, chr_rom_size),
            .size = chr_rom_size,
            .writeable = 0
        };

        size_t chr_rom_read = fread(cart->chr_rom.data, 0x01, chr_rom_size, rom);
        if (chr_rom_read != chr_rom_size) {
            fprintf(stderr, "ERROR: Failed to read character rom from .nes file!\n");
            return CNES_FAILURE;
        }
    }

    if (prg_ram_size) {
        cart->prg_ram = (_mem){
            .data = calloc(1, prg_ram_size),
            .size = prg_ram_size,
            .writeable = 1
        };
    }

    if (prg_nvram_size) {
        cart->prg_nvram = (_mem){
            .data = calloc(1, prg_nvram_size),
            .size = prg_nvram_size,
            .writeable = 1
        };
    }

    if (chr_ram_size) {
        cart->chr_ram = (_mem){
            .data = calloc(1, chr_ram_size),
            .size = chr_ram_size,
            .writeable = 1
        };
    }

    if (chr_nvram_size) {
        cart->chr_nvram = (_mem){
            .data = calloc(1, chr_nvram_size),
            .size = chr_nvram_size,
            .writeable = 1
        };
    }

    return CNES_SUCCESS;
}

void cart_unload(_cart* cart) {
    if (cart->prg_rom.data) {
        free(cart->prg_rom.data);
        memset(&cart->prg_rom, 0, sizeof(_mem));
    }

    if (cart->chr_rom.data) {
        free(cart->chr_rom.data);
        memset(&cart->chr_rom, 0, sizeof(_mem));
    }

    if (cart->prg_ram.data) {
        free(cart->prg_ram.data);
        memset(&cart->prg_ram, 0, sizeof(_mem));
    }

    if (cart->prg_nvram.data) {
        free(cart->prg_nvram.data);
        memset(&cart->prg_nvram, 0, sizeof(_mem));
    }

    if (cart->chr_ram.data) {
        free(cart->chr_ram.data);
        memset(&cart->chr_ram, 0, sizeof(_mem));
    }

    if (cart->chr_nvram.data) {
        free(cart->chr_nvram.data);
        memset(&cart->chr_nvram, 0, sizeof(_mem));
    }

    if (cart->mapper.deinit) {
        cart->mapper.deinit(cart);
    }
}

void parse_ines(_cart* cart, uint8_t header[16]) {
    cart->prg_ram_banks = header[8];
    cart->tv_system = header[9] & 0x01;
}

void parse_nes2(_cart* cart, uint8_t header[16]) {
    cart->mapper_id |= (uint16_t)header[8] << 8;
    cart->prg_rom_banks |= (uint16_t)(header[9] & 0x0F) << 8;
    cart->chr_rom_banks |= (uint16_t)(header[9] & 0xF0) << 4;

    cart->prg_ram_shift = header[10] & 0x0F;
    cart->prg_nvram_shift = header[10] >> 4;
    cart->chr_ram_shift = header[11] & 0x0F;
    cart->chr_nvram_shift = header[11] >> 4;

    cart->cpu_ppu_timing = header[12] & 0x03;
    cart->misc_roms = header[14] & 0x03;
    cart->expansion_device = header[15] & 0x3F;
}

uint8_t cart_cpu_read(_cart* cart, uint16_t addr) {
    return cart->mapper.cpu_read(cart, addr);
}

void cart_cpu_write(_cart* cart, uint16_t addr, uint8_t data) {
    cart->mapper.cpu_write(cart, addr, data);
}

uint8_t cart_ppu_read(_cart* cart, uint16_t addr) {
    return cart->mapper.ppu_read(cart, addr);
}

void cart_ppu_write(_cart* cart, uint16_t addr, uint8_t data) {
    cart->mapper.ppu_write(cart, addr, data);
}
