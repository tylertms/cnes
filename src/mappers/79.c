#include "../mapper.h"

typedef struct _mdata {
    uint8_t prg_bank;
    uint8_t chr_bank;
} _mdata;

uint8_t map_init_79(_cart* cart) {
    _mdata* mdata = calloc(1, sizeof(_mdata));
    cart->mapper.data = mdata;
    return 0;
}

uint8_t map_deinit_79(_cart* cart) {
    free(cart->mapper.data);
    return 0;
}

uint8_t map_irq_pending_79(_cart *cart) {
    (void)cart;
    return 0;
}

uint8_t map_cpu_read_79(_cart* cart, uint16_t addr) {
    uint8_t data = 0x00;
    _mdata* mdata = cart->mapper.data;

    if (0x8000 <= addr && addr <= 0xFFFF) {
        uint32_t offset = (mdata->prg_bank * 0x8000) + (addr & 0x7FFF);
        if (cart->prg_rom.size)         data = cart->prg_rom.data[offset];
        else if (cart->prg_ram.size)    data = cart->prg_ram.data[offset];
        else if (cart->prg_nvram.size)  data = cart->prg_nvram.data[offset];
    }

    return data;
}

void map_cpu_write_79(_cart* cart, uint16_t addr, uint8_t data) {
    _mdata* mdata = cart->mapper.data;
    uint16_t ctrl_mask = 0xE100;
    uint16_t ctrl_bits = 0x4100;

    if (addr >= 0x6000 && addr <= 0x7FFF) {
        uint32_t offset = (mdata->prg_bank * 0x8000) + (addr & 0x7FFF);
        if (cart->prg_ram.size)         cart->prg_ram.data[offset] = data;
        else if (cart->prg_nvram.size)  cart->prg_nvram.data[offset] = data;
    } else if ((addr & ctrl_mask) == ctrl_bits) {
        _mdata* mdata = cart->mapper.data;
        mdata->prg_bank = (data & 0x08) >> 3;
        mdata->chr_bank = data & 0x07;
    }
}

uint8_t map_ppu_read_79(_cart* cart, uint16_t addr) {
    uint8_t data = 0x00;
    _mdata* mdata = cart->mapper.data;

    if (0x0000 <= addr && addr <= 0x1FFF) {
        uint16_t offset = (mdata->chr_bank * 0x2000) + (addr & 0x1FFF);
        if (cart->chr_rom.size)         data = cart->chr_rom.data[offset];
        else if (cart->chr_ram.size)    data = cart->chr_ram.data[offset];
        else if (cart->chr_nvram.size)  data = cart->chr_nvram.data[offset];
    }

    return data;
}

void map_ppu_write_79(_cart* cart, uint16_t addr, uint8_t data) {
    _mdata* mdata = cart->mapper.data;

    if (0x0000 <= addr && addr <= 0x1FFF) {
        uint16_t offset = (mdata->chr_bank * 0x2000) + (addr & 0x1FFF);
        if (cart->chr_rom.size)         cart->chr_rom.data[offset] = data;
        else if (cart->chr_ram.size)    cart->chr_ram.data[offset] = data;
        else if (cart->chr_nvram.size)  cart->chr_nvram.data[offset] = data;
    }
}
