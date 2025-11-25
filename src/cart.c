#include "cart.h"
#include "mapper.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

uint8_t cart_load(_cart* cart, char* file) {
    FILE* rom = fopen(file, "rb");
    if (rom == NULL) {
        fprintf(stderr, "ERROR: Failed to open .nes file!\n");
        return 1;
    }

    uint8_t header[0x10];
    if (fread(header, 1, 0x10, rom) != 0x10) {
        fprintf(stderr, "ERROR: Failed to read header from .nes file!\n");
        return 1;
    };

    if (memcmp(header, "NES\x1B", 3)) {
        fprintf(stderr, "ERROR: .nes file is not valid!\n");
        return 1;
    }

    cart->prg_rom_banks = header[4];
    cart->chr_rom_banks = header[5];
    cart->ntbl_layout = header[6] & 0x01;
    cart->trainer = header[6] & 0x04;
    cart->alt_ntbl_layout = header[6] & 0x08;
    cart->mapper_id = (header[6] >> 4) | (header[7] & 0xF0);

    uint8_t nes2 = (header[7] & 0x0C) == 0x08;
    if (nes2) parse_nes2(cart, header);
    else parse_ines(cart, header);

    cart->mapper = mappers[cart->mapper_id & 0x0FFF];
    cart->mapper.init(cart);

    if (!cart->mapper.cpu_read) {
        fprintf(stderr, "ERROR: Mapper %03d is currently unsupported!\n", cart->mapper_id & 0x0FFF);
        return 1;
    }

    if (cart->trainer)
        fseek(rom, 0x200, SEEK_CUR);

    size_t prg_rom_size = cart->prg_rom_banks * 0x4000;
    size_t chr_rom_size = cart->chr_rom_banks * 0x2000;
    size_t prg_ram_size = (size_t)64 << cart->prg_ram_shift;
    size_t chr_ram_size = (size_t)64 << cart->prg_ram_shift;

    cart->prg_rom = (_mem){
      .data = malloc(prg_rom_size),
      .size = prg_rom_size,
      .writeable = 0
    };

    cart->chr_rom = (_mem){
      .data = malloc(chr_rom_size),
      .size = chr_rom_size,
      .writeable = 0
    };

    if (cart->prg_ram_shift) {
        cart->prg_ram = (_mem){
            .data = malloc(prg_ram_size),
            .size = prg_ram_size,
            .writeable = 1
        };
    }

    if (cart->chr_ram_shift) {
        cart->chr_ram = (_mem){
            .data = malloc(chr_ram_size),
            .size = chr_ram_size,
            .writeable = 1
        };
    }

    int prg_banks_read = fread(cart->prg_rom.data, 0x4000, cart->prg_rom_banks, rom);
    if (prg_banks_read != cart->prg_rom_banks) {
        fprintf(stderr, "ERROR: Failed to read program rom from .nes file!\n");
        return 1;
    }

    int chr_banks_read = fread(cart->chr_rom.data, 0x2000, cart->chr_rom_banks, rom);
    if (chr_banks_read != cart->chr_rom_banks) {
        fprintf(stderr, "ERROR: Failed to read character rom from .nes file!\n");
        return 1;
    }

    return 0;
}

uint8_t parse_ines(_cart* cart, uint8_t header[16]) {
    cart->prg_ram_banks = header[8];
    cart->tv_system = header[9] & 0x01;

    return 0;
}

uint8_t parse_nes2(_cart* cart, uint8_t header[16]) {
    cart->mapper_id |= (uint16_t)header[8] << 8;
    cart->prg_rom_banks |= (uint16_t)(header[9] & 0x0F) << 8;
    cart->chr_rom_banks |= (uint16_t)(header[9] & 0xF0) << 4;

    cart->prg_ram_shift = header[10] & 0xF0;
    cart->prg_nvram_shift = header[10] >> 4;
    cart->chr_ram_shift = header[11] & 0xF0;
    cart->chr_nvram_shift = header[11] >> 4;

    cart->cpu_ppu_timing = header[12] & 0x03;
    cart->misc_roms = header[14] & 0x03;
    cart->expansion_device = header[15] & 0x3F;

    return 0;
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
