#include "mapper.h"

uint8_t map_cpu_read_000(_cart* cart, uint16_t addr) {
    uint8_t data = 0x00;

    if (0x6000 <= addr && addr <= 0x7FFF) {
        uint16_t offset = (addr - 0x6000) & (cart->prg_ram.size - 1);
        data = cart->prg_ram.data[offset];
    } else if (0x8000 <= addr && addr <= 0xFFFF) {
        uint16_t offset = (addr - 0x8000) & (cart->prg_rom.size - 1);
        data = cart->prg_rom.data[offset];
    }

    return data;
}

void map_cpu_write_000(_cart* cart, uint16_t addr, uint8_t data) {
    if (0x6000 <= addr && addr <= 0x7FFF) {
        uint16_t offset = (addr - 0x6000) & (cart->prg_ram.size - 1);
        cart->prg_ram.data[offset] = data;
    } else if (0x8000 <= addr && addr <= 0xFFFF) {
        uint16_t offset = (addr - 0x8000) & (cart->prg_rom.size - 1);
        cart->prg_rom.data[offset] = data;
    }
}

uint8_t map_ppu_read_000(_cart* cart, uint16_t addr) {
    uint8_t data = 0x00;

    if (0x0000 <= addr && addr <= 0x1FFF) {
        data = cart->chr_rom.data[addr];
    }

    return data;
}

void map_ppu_write_000(_cart* cart, uint16_t addr, uint8_t data) {
    return;
}
