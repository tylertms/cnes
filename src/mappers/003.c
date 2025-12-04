#include "../mapper.h"

typedef struct _mdata {
    uint8_t chr_bank;
} _mdata;

CNES_RESULT map_init_3(_cart* cart) {
    _mdata* mdata = calloc(1, sizeof(_mdata));
    if (mdata == NULL) return CNES_FAILURE;
    cart->mapper.data = mdata;

    mdata->chr_bank = 0;
    return CNES_SUCCESS;
}

CNES_RESULT map_deinit_3(_cart* cart) {
    free(cart->mapper.data);
    return CNES_SUCCESS;
}

CNES_RESULT map_irq_pending_3(_cart* cart) {
    (void)cart;
    return 0;
}

uint8_t map_cpu_read_3(_cart* cart, uint16_t addr) {
    uint8_t data = 0x00;

    if (0x6000 <= addr && addr <= 0x7FFF) {
        if (cart->prg_ram.size) {
            uint16_t offset = (addr - 0x6000) & (cart->prg_ram.size - 1);
            data = cart->prg_ram.data[offset];
        } else if (cart->prg_nvram.size) {
            uint16_t offset = (addr - 0x6000) & (cart->prg_nvram.size - 1);
            data = cart->prg_nvram.data[offset];
        }
    } else if (0x8000 <= addr && addr <= 0xFFFF) {
        uint16_t offset = (addr - 0x8000) & (cart->prg_rom.size - 1);
        data = cart->prg_rom.data[offset];
    }

    return data;
}

void map_cpu_write_3(_cart* cart, uint16_t addr, uint8_t data) {
    if (0x6000 <= addr && addr <= 0x7FFF) {
        if (cart->prg_ram.size) {
            uint16_t offset = (addr - 0x6000) & (cart->prg_ram.size - 1);
            cart->prg_ram.data[offset] = data;
        } else if (cart->prg_nvram.size) {
            uint16_t offset = (addr - 0x6000) & (cart->prg_nvram.size - 1);
            cart->prg_nvram.data[offset] = data;
        }
    } else if (0x8000 <= addr && addr <= 0xFFFF) {
        _mdata* mdata = cart->mapper.data;
        mdata->chr_bank = data & (cart->chr_rom_banks - 1);
    }
}

uint8_t map_ppu_read_3(_cart* cart, uint16_t addr) {
    uint8_t data = 0x00;

    if (0x0000 <= addr && addr <= 0x1FFF) {
        _mdata* mdata = cart->mapper.data;

        if (cart->chr_rom.size) {
            uint16_t offset = (mdata->chr_bank * 0x2000 + addr) & (cart->chr_rom.size - 1);
            data = cart->chr_rom.data[offset];
        } else {
            uint16_t offset = addr & (cart->chr_ram.size - 1);
            data = cart->chr_ram.data[offset];
        }
    }

    return data;
}

void map_ppu_write_3(_cart* cart, uint16_t addr, uint8_t data) {
    if (0x0000 <= addr && addr <= 0x1FFF) {
        if (cart->chr_ram.size) {
            _mdata* mdata = cart->mapper.data;
            uint16_t offset = (mdata->chr_bank * 0x2000 + addr) & (cart->chr_ram.size - 1);
            cart->chr_ram.data[offset] = data;
        }
    }
}

REGISTER_MAPPER(3,
    map_init_3,
    map_deinit_3,
    map_irq_pending_3,
    map_cpu_read_3,
    map_cpu_write_3,
    map_ppu_read_3,
    map_ppu_write_3
)
