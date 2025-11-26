#include "../mapper.h"

typedef struct _uxrom {
    uint8_t prg_bank_low;
    uint8_t prg_bank_high;
} _uxrom;

void map_init_002(_cart* cart) {
    _uxrom* uxrom = calloc(1, sizeof(_uxrom));
    cart->mapper.data = uxrom;
    uxrom->prg_bank_high = cart->prg_rom_banks - 1;
}

void map_deinit_002(_cart* cart) {
    free(cart->mapper.data);
}

uint8_t map_cpu_read_002(_cart* cart, uint16_t addr) {
    uint8_t data = 0x00;
    _uxrom* uxrom = cart->mapper.data;

    if (0x6000 <= addr && addr <= 0x7FFF) {
        if (cart->prg_ram.size) {
            uint16_t offset = (addr - 0x6000) & (cart->prg_ram.size - 1);
            data = cart->prg_ram.data[offset];
        }
    } else if (0x8000 <= addr && addr <= 0xBFFF) {
        uint32_t offset = (uxrom->prg_bank_low * 0x4000) + (addr & 0x3FFF);
        data = cart->prg_rom.data[offset];
    } else if (0xC000 <= addr && addr <= 0xFFFF) {
        uint32_t offset = (uxrom->prg_bank_high * 0x4000) + (addr & 0x3FFF);
        data = cart->prg_rom.data[offset];
    }

    return data;
}

void map_cpu_write_002(_cart* cart, uint16_t addr, uint8_t data) {
    if (0x6000 <= addr && addr <= 0x7FFF) {
        if (cart->prg_ram.size) {
            uint16_t offset = (addr - 0x6000) & (cart->prg_ram.size - 1);
            cart->prg_ram.data[offset] = data;
        }
    } else if (0x8000 <= addr && addr <= 0xFFFF) {
        _uxrom* uxrom = cart->mapper.data;
        uxrom->prg_bank_low = data & (cart->prg_rom_banks - 1);
    }
}

uint8_t map_ppu_read_002(_cart* cart, uint16_t addr) {
    uint8_t data = 0x00;

    if (0x0000 <= addr && addr <= 0x1FFF) {
        if (cart->chr_rom.size) {
            uint16_t offset = addr & (cart->chr_rom.size - 1);
            data = cart->chr_rom.data[offset];
        } else {
            uint16_t offset = addr & (cart->chr_ram.size - 1);
            data = cart->chr_ram.data[offset];
        }
    }

    return data;
}

void map_ppu_write_002(_cart* cart, uint16_t addr, uint8_t data) {
    if (0x0000 <= addr && addr <= 0x1FFF) {
        if (cart->chr_ram.size) {
            cart->chr_ram.data[addr & (cart->chr_ram.size - 1)] = data;
        }
    }
}
