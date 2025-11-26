#include "../mapper.h"

typedef struct _mmc1 {
    uint8_t load;
    uint8_t write_count;
    uint8_t control;
    uint8_t chr_bank0;
    uint8_t chr_bank1;
    uint8_t prg_bank;
} _mmc1;

void mmc1_apply_control(_cart* cart, _mmc1* mmc1) {
    uint8_t m = mmc1->control & 0x03;
    switch (m) {
        case 0:     cart->mirror = MIRROR_SINGLE0;      break;
        case 1:     cart->mirror = MIRROR_SINGLE1;      break;
        case 2:     cart->mirror = MIRROR_VERTICAL;     break;
        default:    cart->mirror = MIRROR_HORIZONTAL;   break;
    }
}

void mmc1_commit(_cart* cart, uint16_t addr) {
    _mmc1* mmc1 = (_mmc1*)cart->mapper.data;
    uint8_t value = mmc1->load & 0x1F;

    if (0x8000 <= addr && addr <= 0x9FFF) {
        mmc1->control = value;
        mmc1_apply_control(cart, mmc1);
    } else if (0xA000 <= addr && addr <= 0xBFFF) {
        mmc1->chr_bank0 = value;
    } else if (0xC000 <= addr && addr <= 0xDFFF) {
        mmc1->chr_bank1 = value;
    } else if (0xE000 <= addr && addr <= 0xFFFF) {
        mmc1->prg_bank = value & 0x0F;
    }

    mmc1->load = 0;
    mmc1->write_count = 0;
}

void map_init_001(_cart* cart) {
    _mmc1* mmc1 = calloc(1, sizeof(_mmc1));
    cart->mapper.data = mmc1;

    mmc1->control = 0x1C;
    mmc1_apply_control(cart, mmc1);
}

void map_deinit_001(_cart* cart) {
    free(cart->mapper.data);
}

uint8_t map_cpu_read_001(_cart* cart, uint16_t addr) {
    _mmc1* mmc1 = (_mmc1*)cart->mapper.data;
    uint8_t data = 0;

    if (addr >= 0x6000 && addr <= 0x7FFF) {
        if (cart->prg_ram.size) {
            uint16_t mask = (uint16_t)(cart->prg_ram.size - 1);
            uint16_t off = (addr - 0x6000) & mask;
            data = cart->prg_ram.data[off];
        }
    } else if (0x8000 <= addr && addr <= 0xFFFF) {
        uint8_t mode = (mmc1->control >> 2) & 3;
        uint32_t bank = 0;
        uint16_t inner = (addr - 0x8000) & 0x3FFF;
        uint8_t bank_mask = (uint8_t)(cart->prg_rom_banks - 1);

        switch (mode) {
            case 0:
            case 1:
                bank = (uint32_t)(mmc1->prg_bank & ~1);
                if (addr >= 0xC000) bank++;
                break;

            case 2:
                bank = (addr < 0xC000) ? 0 : (uint32_t)mmc1->prg_bank;
                break;

            default:
                bank = (addr < 0xC000) ? (uint32_t)mmc1->prg_bank : (uint32_t)(cart->prg_rom_banks - 1);
                break;
        }

        bank &= bank_mask;

        uint32_t off = (bank << 14) | inner;
        uint32_t rom_mask = cart->prg_rom.size - 1;
        off &= rom_mask;

        data = cart->prg_rom.data[off];
    }

    return data;
}

void map_cpu_write_001(_cart* cart, uint16_t addr, uint8_t data) {
    _mmc1* mmc1 = (_mmc1*)cart->mapper.data;

    if (addr >= 0x6000 && addr <= 0x7FFF) {
        if (cart->prg_ram.size) {
            uint16_t mask = (uint16_t)(cart->prg_ram.size - 1);
            uint16_t off = (addr - 0x6000) & mask;
            cart->prg_ram.data[off] = data;
        }
    } else if (0x8000 <= addr && addr <= 0xFFFF) {
        if (data & 0x80) {
            mmc1->load = 0;
            mmc1->write_count = 0;
            mmc1->control |= 0x0C;
            mmc1_apply_control(cart, mmc1);
        } else {
            mmc1->load >>= 1;
            mmc1->load |= (data & 1) << 4;
            mmc1->write_count++;

            if (mmc1->write_count == 5) {
                mmc1_commit(cart, addr);
            }
        }
    }
}

uint8_t map_ppu_read_001(_cart* cart, uint16_t addr) {
    _mmc1* mmc1 = (_mmc1*)cart->mapper.data;
    uint8_t data = 0;

    if (0x0000 <= addr && addr <= 0x1FFF) {
        uint8_t mode = (mmc1->control >> 4) & 1;
        uint32_t base;
        uint16_t inner;

        if (!mode) {
            uint8_t bank = mmc1->chr_bank0 & ~1;
            base = (uint32_t)bank << 12;
            inner = addr & 0x1FFF;
        } else {
            uint8_t bank = (addr < 0x1000) ? mmc1->chr_bank0 : mmc1->chr_bank1;
            base = (uint32_t)bank << 12;
            inner = addr & 0x0FFF;
        }

        uint32_t off = base + inner;

        if (cart->chr_rom.size) {
            uint32_t mask = cart->chr_rom.size - 1;
            off &= mask;
            data = cart->chr_rom.data[off];
        } else if (cart->chr_ram.size) {
            uint32_t mask = cart->chr_ram.size - 1;
            off &= mask;
            data = cart->chr_ram.data[off];
        }
    }

    return data;
}

void map_ppu_write_001(_cart* cart, uint16_t addr, uint8_t data) {
    if (cart->chr_ram.size && 0x0000 <= addr && addr <= 0x1FFF) {
        _mmc1* mmc1 = (_mmc1*)cart->mapper.data;
        uint8_t mode = (mmc1->control >> 4) & 1;
        uint32_t base;
        uint16_t inner;

        if (!mode) {
            uint8_t bank = mmc1->chr_bank0 & ~1;
            base = (uint32_t)bank << 12;
            inner = addr & 0x1FFF;
        } else {
            uint8_t bank = (addr < 0x1000) ? mmc1->chr_bank0 : mmc1->chr_bank1;
            base = (uint32_t)bank << 12;
            inner = addr & 0x0FFF;
        }

        uint32_t off = base + inner;
        uint32_t mask = cart->chr_ram.size - 1;
        off &= mask;

        cart->chr_ram.data[off] = data;
    }
}
