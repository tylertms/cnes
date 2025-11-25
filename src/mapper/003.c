#include "../mapper.h"

typedef struct _cnrom {
    uint8_t chr_bank;
} _cnrom;

void map_init_003(_cart* cart) {
    _cnrom* cnrom = malloc(sizeof(_cnrom));
    cart->mapper.data = cnrom;

    *cnrom = (_cnrom){
        .chr_bank = 0
    };
}

void map_deinit_003(_cart* cart) {
    free(cart->mapper.data);
}

uint8_t map_cpu_read_003(_cart* cart, uint16_t addr) {
    uint8_t data = 0x00;

    if (0x6000 <= addr && addr <= 0x7FFF) {
        if (cart->prg_ram.size) {
            uint16_t offset = (addr - 0x6000) & (cart->prg_ram.size - 1);
            data = cart->prg_ram.data[offset];
        }
    } else if (0x8000 <= addr && addr <= 0xFFFF) {
        uint16_t offset = (addr - 0x8000) & (cart->prg_rom.size - 1);
        data = cart->prg_rom.data[offset];
    }

    return data;
}

void map_cpu_write_003(_cart* cart, uint16_t addr, uint8_t data) {
    if (0x6000 <= addr && addr <= 0x7FFF) {
        if (cart->prg_ram.size) {
            uint16_t offset = (addr - 0x6000) & (cart->prg_ram.size - 1);
            cart->prg_ram.data[offset] = data;
        }
    } else if (0x8000 <= addr && addr <= 0xFFFF) {
        _cnrom* cnrom = cart->mapper.data;
        cnrom->chr_bank = data & (cart->chr_rom_banks - 1);
    }
}

uint8_t map_ppu_read_003(_cart* cart, uint16_t addr) {
    uint8_t data = 0x00;

    if (0x0000 <= addr && addr <= 0x1FFF) {
        _cnrom* cnrom = cart->mapper.data;

        if (cart->chr_rom.size) {
            uint16_t offset = (cnrom->chr_bank * 0x2000 + addr) & (cart->chr_rom.size - 1);
            data = cart->chr_rom.data[offset];
        } else {
            uint16_t offset = addr & (cart->chr_ram.size - 1);
            data = cart->chr_ram.data[offset];
        }
    }

    return data;
}

void map_ppu_write_003(_cart* cart, uint16_t addr, uint8_t data) {
    if (0x0000 <= addr && addr <= 0x1FFF) {
        if (cart->chr_ram.size) {
            _cnrom* cnrom = cart->mapper.data;
            uint16_t offset = (cnrom->chr_bank * 0x2000 + addr) & (cart->chr_ram.size - 1);
            cart->chr_ram.data[offset] = data;
        }
    }
}
