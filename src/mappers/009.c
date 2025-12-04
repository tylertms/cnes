#include "../mapper.h"

#define LATCH_FD 0
#define LATCH_FE 1

typedef struct _mdata {
    uint8_t prg_bank;

    uint8_t chr_low_fd;
    uint8_t chr_low_fe;
    uint8_t chr_high_fd;
    uint8_t chr_high_fe;

    uint8_t latch_low;
    uint8_t latch_high;
} _mdata;

CNES_RESULT map_init_9(_cart* cart) {
    _mdata* mdata = calloc(1, sizeof(_mdata));
    if (mdata == NULL) return CNES_FAILURE;
    cart->mapper.data = mdata;

    mdata->latch_low = LATCH_FD;
    mdata->latch_high = LATCH_FD;
    return CNES_SUCCESS;
}

CNES_RESULT map_deinit_9(_cart* cart) {
    free(cart->mapper.data);
    return CNES_SUCCESS;
}

CNES_RESULT map_irq_pending_9(_cart* cart) {
    (void)cart;
    return 0;
}

uint8_t map_cpu_read_9(_cart* cart, uint16_t addr) {
    uint8_t data = 0x00;
    _mdata* mdata = cart->mapper.data;

    uint32_t prg_mask = (cart->prg_rom.size >> 13) - 1;
    if (0x6000 <= addr && addr <= 0x7FFF) {
        if (cart->prg_ram.size) data = cart->prg_ram.data[addr & 0x1FFF];
    }
    else if (0x8000 <= addr && addr <= 0x9FFF) {
        uint32_t bank = mdata->prg_bank & prg_mask;
        uint32_t offset = (bank * 0x2000) + (addr & 0x1FFF);
        data = cart->prg_rom.data[offset];
    }
    else if (0xA000 <= addr && addr <= 0xFFFF) {
        uint32_t offset_end = 0x6000 - (addr - 0xA000);
        uint32_t offset = cart->prg_rom.size - offset_end;
        if (offset < cart->prg_rom.size) {
            data = cart->prg_rom.data[offset];
        }
    }

    return data;
}

void map_cpu_write_9(_cart* cart, uint16_t addr, uint8_t data) {
    _mdata* mdata = cart->mapper.data;

    if (0x6000 <= addr && addr <= 0x7FFF) {
        if (cart->prg_ram.size) cart->prg_ram.data[addr & 0x1FFF] = data;
    }
    else if (0xA000 <= addr && addr <= 0xAFFF) {
        mdata->prg_bank = data & 0x0F;
    }
    else if (0xB000 <= addr && addr <= 0xBFFF) {
        mdata->chr_low_fd = data & 0x1F;
    }
    else if (0xC000 <= addr && addr <= 0xCFFF) {
        mdata->chr_low_fe = data & 0x1F;
    }
    else if (0xD000 <= addr && addr <= 0xDFFF) {
        mdata->chr_high_fd = data & 0x1F;
    }
    else if (0xE000 <= addr && addr <= 0xEFFF) {
        mdata->chr_high_fe = data & 0x1F;
    }
    else if (0xF000 <= addr && addr <= 0xFFFF) {
        cart->mirror = (data & 0x01) ? MIRROR_HORIZONTAL : MIRROR_VERTICAL;
    }
}

uint8_t map_ppu_read_9(_cart* cart, uint16_t addr) {
    uint8_t data = 0x00;
    _mdata* mdata = cart->mapper.data;

    uint8_t requested_bank = 0;
    if (0x0000 <= addr && addr <= 0x0FFF) {
        requested_bank = (mdata->latch_low == LATCH_FD) ? mdata->chr_low_fd : mdata->chr_low_fe;
    } else {
        requested_bank = (mdata->latch_high == LATCH_FD) ? mdata->chr_high_fd : mdata->chr_high_fe;
    }

    if (0x0000 <= addr && addr <= 0x1FFF) {
        uint32_t offset = (requested_bank * 0x1000) + (addr & 0x0FFF);
        if (cart->chr_rom.size) {
            data = cart->chr_rom.data[offset % cart->chr_rom.size];
        }
    }

    if (addr == 0x0FD8) {
        mdata->latch_low = LATCH_FD;
    }
    else if (addr == 0x0FE8) {
        mdata->latch_low = LATCH_FE;
    }
    else if (addr >= 0x1FD8 && addr <= 0x1FDF) {
        mdata->latch_high = LATCH_FD;
    }
    else if (addr >= 0x1FE8 && addr <= 0x1FEF) {
        mdata->latch_high = LATCH_FE;
    }

    return data;
}

void map_ppu_write_9(_cart* cart, uint16_t addr, uint8_t data) {
    if (0x0000 <= addr && addr <= 0x1FFF) {
         if (cart->chr_ram.size) cart->chr_ram.data[addr] = data;
    }
}

REGISTER_MAPPER(9,
    map_init_9,
    map_deinit_9,
    map_irq_pending_9,
    map_cpu_read_9,
    map_cpu_write_9,
    map_ppu_read_9,
    map_ppu_write_9
)
