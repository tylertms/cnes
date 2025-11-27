#include "../mapper.h"

typedef struct _axrom {
    uint8_t prg_bank;
} _axrom;

uint8_t map_init_007(_cart* cart) {
    _axrom* axrom = calloc(1, sizeof(_axrom));
    cart->mapper.data = axrom;

    axrom->prg_bank = 0;
    cart->mirror = MIRROR_SINGLE0;
    return 0;
}

uint8_t map_deinit_007(_cart* cart) {
    free(cart->mapper.data);
    return 0;
}

uint8_t map_irq_pending_007(_cart *cart) {
    (void)cart;
    return 0;
}

uint8_t map_cpu_read_007(_cart* cart, uint16_t addr) {
    uint8_t data = 0x00;
    _axrom* axrom = cart->mapper.data;

    if (0x8000 <= addr && addr <= 0xFFFF) {
        uint32_t offset = (axrom->prg_bank * 0x8000) + (addr & 0x7FFF);
        data = cart->prg_rom.data[offset];
    }

    return data;
}

void map_cpu_write_007(_cart* cart, uint16_t addr, uint8_t data) {
    if (0x8000 <= addr && addr <= 0xFFFF) {
        _axrom* axrom = cart->mapper.data;
        axrom->prg_bank = (data & 0x07) & (cart->prg_rom_banks - 1);
        cart->mirror = (data & 0x10) ? MIRROR_SINGLE1 : MIRROR_SINGLE0;
    }
}

uint8_t map_ppu_read_007(_cart* cart, uint16_t addr) {
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

void map_ppu_write_007(_cart* cart, uint16_t addr, uint8_t data) {
    if (0x0000 <= addr && addr <= 0x1FFF) {
        if (cart->chr_ram.size) {
            cart->chr_ram.data[addr & (cart->chr_ram.size - 1)] = data;
        }
    }
}
