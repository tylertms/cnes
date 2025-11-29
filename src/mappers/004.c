#include "../mapper.h"

typedef struct _mmc3 {
    uint8_t bank_select;
    uint8_t bank_reg[8];

    uint8_t ram_protect;
    uint8_t irq_latch;
    uint8_t irq_counter;
    uint8_t irq_reload_flag;
    uint8_t irq_enable;

    uint8_t irq_pending;

    uint32_t prg_base[4];
    uint32_t chr_base[8];
} _mmc3;

void mmc3_update_prg(_cart* cart, _mmc3* mmc3) {
    uint8_t prg_mode = (mmc3->bank_select >> 6) & 1;
    uint8_t total_8k = (uint8_t)(cart->prg_rom_banks * 2);
    uint8_t mask = total_8k - 1;

    uint8_t b6 = mmc3->bank_reg[6] & mask;
    uint8_t b7 = mmc3->bank_reg[7] & mask;
    uint8_t last  = total_8k - 1;
    uint8_t last2 = total_8k - 2;

    if (!prg_mode) {
        mmc3->prg_base[0] = (uint32_t)b6    << 13;
        mmc3->prg_base[1] = (uint32_t)b7    << 13;
        mmc3->prg_base[2] = (uint32_t)last2 << 13;
        mmc3->prg_base[3] = (uint32_t)last  << 13;
    } else {
        mmc3->prg_base[0] = (uint32_t)last2 << 13;
        mmc3->prg_base[1] = (uint32_t)b7    << 13;
        mmc3->prg_base[2] = (uint32_t)b6    << 13;
        mmc3->prg_base[3] = (uint32_t)last  << 13;
    }
}

void mmc3_update_chr(_cart* cart, _mmc3* mmc3) {
    uint8_t chr_mode = (mmc3->bank_select >> 7) & 1;

    uint32_t total_1k;
    if (cart->chr_rom.size) {
        total_1k = (uint32_t)(cart->chr_rom.size >> 10);
    } else {
        total_1k = (uint32_t)(cart->chr_ram.size >> 10);
    }
    uint32_t mask = total_1k - 1;

    uint8_t b0 = mmc3->bank_reg[0] & (uint8_t)(mask & ~1u);
    uint8_t b1 = mmc3->bank_reg[1] & (uint8_t)(mask & ~1u);
    uint8_t b2 = mmc3->bank_reg[2] & (uint8_t)mask;
    uint8_t b3 = mmc3->bank_reg[3] & (uint8_t)mask;
    uint8_t b4 = mmc3->bank_reg[4] & (uint8_t)mask;
    uint8_t b5 = mmc3->bank_reg[5] & (uint8_t)mask;

    if (!chr_mode) {
        mmc3->chr_base[0] = ((uint32_t)b0     & mask) << 10;
        mmc3->chr_base[1] = ((uint32_t)(b0+1) & mask) << 10;
        mmc3->chr_base[2] = ((uint32_t)b1     & mask) << 10;
        mmc3->chr_base[3] = ((uint32_t)(b1+1) & mask) << 10;
        mmc3->chr_base[4] = ((uint32_t)b2     & mask) << 10;
        mmc3->chr_base[5] = ((uint32_t)b3     & mask) << 10;
        mmc3->chr_base[6] = ((uint32_t)b4     & mask) << 10;
        mmc3->chr_base[7] = ((uint32_t)b5     & mask) << 10;
    } else {
        mmc3->chr_base[0] = ((uint32_t)b2     & mask) << 10;
        mmc3->chr_base[1] = ((uint32_t)b3     & mask) << 10;
        mmc3->chr_base[2] = ((uint32_t)b4     & mask) << 10;
        mmc3->chr_base[3] = ((uint32_t)b5     & mask) << 10;
        mmc3->chr_base[4] = ((uint32_t)b0     & mask) << 10;
        mmc3->chr_base[5] = ((uint32_t)(b0+1) & mask) << 10;
        mmc3->chr_base[6] = ((uint32_t)b1     & mask) << 10;
        mmc3->chr_base[7] = ((uint32_t)(b1+1) & mask) << 10;
    }
}

void mmc3_scanline_tick(_cart* cart) {
    _mmc3* mmc3 = cart->mapper.data;

    if (mmc3->irq_reload_flag || mmc3->irq_counter == 0) {
        mmc3->irq_counter = mmc3->irq_latch;
        mmc3->irq_reload_flag = 0;
    } else {
        mmc3->irq_counter--;
    }

    if (mmc3->irq_counter == 0 && mmc3->irq_enable) {
        mmc3->irq_pending = 1;
    }
}

uint8_t map_init_004(_cart* cart) {
    _mmc3* mmc3 = calloc(1, sizeof(_mmc3));
    if (mmc3 == NULL) return 1;
    cart->mapper.data = mmc3;

    uint8_t total_8k = (uint8_t)(cart->prg_rom_banks * 2);
    uint8_t last  = total_8k - 1;
    uint8_t last2 = total_8k - 2;

    mmc3->bank_reg[6] = last2;
    mmc3->bank_reg[7] = last;

    mmc3_update_prg(cart, mmc3);
    mmc3_update_chr(cart, mmc3);

    return 0;
}

uint8_t map_deinit_004(_cart* cart) {
    free(cart->mapper.data);
    return 0;
}

uint8_t map_irq_pending_004(_cart *cart) {
    _mmc3* mmc3 = cart->mapper.data;
    return mmc3->irq_pending;
}

uint8_t map_cpu_read_004(_cart* cart, uint16_t addr) {
    _mmc3* mmc3 = (_mmc3*)cart->mapper.data;
    uint8_t data = 0x00;

    if (addr >= 0x6000 && addr <= 0x7FFF) {
        if (cart->prg_ram.size) {
            uint16_t mask = (uint16_t)(cart->prg_ram.size - 1);
            uint16_t offset = (addr - 0x6000) & mask;
            data = cart->prg_ram.data[offset];
        } else if (cart->prg_nvram.size) {
            uint16_t mask = (uint16_t)(cart->prg_nvram.size - 1);
            uint16_t offset = (addr - 0x6000) & mask;
            data = cart->prg_nvram.data[offset];
        }
    } else if (0x8000 <= addr && addr <= 0xFFFF) {
        uint8_t index = (addr >> 13) & 0x03;
        uint32_t offset = mmc3->prg_base[index] + (addr & 0x1FFF);
        offset &= cart->prg_rom.size - 1;
        data = cart->prg_rom.data[offset];
    }

    return data;
}

void map_cpu_write_004(_cart* cart, uint16_t addr, uint8_t data) {
    _mmc3* mmc3 = (_mmc3*)cart->mapper.data;

    if (addr >= 0x6000 && addr <= 0x7FFF) {
        if (cart->prg_ram.size && (mmc3->ram_protect & 0xC0) == 0x80) {
            uint16_t mask = (uint16_t)(cart->prg_ram.size - 1);
            uint16_t offset = (addr - 0x6000) & mask;
            cart->prg_ram.data[offset] = data;
        } else if (cart->prg_nvram.size && (mmc3->ram_protect & 0xC0) == 0x80) {
            uint16_t mask = (uint16_t)(cart->prg_nvram.size - 1);
            uint16_t offset = (addr - 0x6000) & mask;
            cart->prg_nvram.data[offset] = data;
        }
    } else if (addr >= 0x8000 && addr <= 0x9FFF) {
        if (addr & 1) {
            uint8_t bank = mmc3->bank_select & 7;
            mmc3->bank_reg[bank] = data;
            mmc3_update_chr(cart, mmc3);
            mmc3_update_prg(cart, mmc3);
        } else {
            mmc3->bank_select = data;
            mmc3_update_chr(cart, mmc3);
            mmc3_update_prg(cart, mmc3);
        }
    } else if (addr >= 0xA000 && addr <= 0xBFFF) {
        if (addr & 1) {
            mmc3->ram_protect = data;
        } else {
            cart->mirror = (data & 1) ? MIRROR_HORIZONTAL : MIRROR_VERTICAL;
        }
    } else if (addr >= 0xC000 && addr <= 0xDFFF) {
        if (addr & 1) {
            mmc3->irq_reload_flag = 1;
        } else {
            mmc3->irq_latch = data;
        }
    } else if (addr >= 0xE000 && addr <= 0xFFFF) {
        if (addr & 1) {
            mmc3->irq_enable = 1;
        } else {
            mmc3->irq_enable = 0;
            mmc3->irq_pending = 0;
        }
    }
}

uint8_t map_ppu_read_004(_cart* cart, uint16_t addr) {
    _mmc3* mmc3 = (_mmc3*)cart->mapper.data;
    uint8_t data = 0x00;

    if (0x0000 <= addr && addr <= 0x1FFF) {
        uint8_t index = (addr >> 10) & 7;
        uint32_t offset = mmc3->chr_base[index] + (addr & 0x03FF);

        if (cart->chr_rom.size) {
            offset &= cart->chr_rom.size - 1;
            data = cart->chr_rom.data[offset];
        } else if (cart->chr_ram.size) {
            offset &= cart->chr_ram.size - 1;
            data = cart->chr_ram.data[offset];
        }
    }

    return data;
}

void map_ppu_write_004(_cart* cart, uint16_t addr, uint8_t data) {
    _mmc3* mmc3 = (_mmc3*)cart->mapper.data;

    if (0x0000 <= addr && addr <= 0x1FFF) {
        if (cart->chr_ram.size) {
            uint8_t index = (addr >> 10) & 7;
            uint32_t offset = mmc3->chr_base[index] + (addr & 0x03FF);
            offset &= cart->chr_ram.size - 1;
            cart->chr_ram.data[offset] = data;
        }
    }
}
