#include "../mapper.h"

typedef struct _mdata {
    uint8_t load;
    uint8_t write_count;
    uint8_t control;
    uint8_t chr_bank0;
    uint8_t chr_bank1;
    uint8_t prg_bank;
} _mdata;

void apply_control(_cart* cart, _mdata* mdata) {
    uint8_t m = mdata->control & 0x03;
    switch (m) {
        case 0:     cart->mirror = MIRROR_SINGLE0;      break;
        case 1:     cart->mirror = MIRROR_SINGLE1;      break;
        case 2:     cart->mirror = MIRROR_VERTICAL;     break;
        default:    cart->mirror = MIRROR_HORIZONTAL;   break;
    }
}

void commit(_cart* cart, uint16_t addr) {
    _mdata* mdata = (_mdata*)cart->mapper.data;
    uint8_t value = mdata->load & 0x1F;

    if (0x8000 <= addr && addr <= 0x9FFF) {
        mdata->control = value;
        apply_control(cart, mdata);
    } else if (0xA000 <= addr && addr <= 0xBFFF) {
        mdata->chr_bank0 = value & 0x1F;
    } else if (0xC000 <= addr && addr <= 0xDFFF) {
        mdata->chr_bank1 = value & 0x1F;
    } else if (0xE000 <= addr && addr <= 0xFFFF) {
        mdata->prg_bank = value & 0x1F;
    }

    mdata->load = 0;
    mdata->write_count = 0;
}

uint8_t map_init_1(_cart* cart) {
    _mdata* mdata = calloc(1, sizeof(_mdata));
    if (mdata == NULL) return 1;
    cart->mapper.data = mdata;

    mdata->control = 0x1C;
    apply_control(cart, mdata);
    return 0;
}

uint8_t map_deinit_1(_cart* cart) {
    free(cart->mapper.data);
    return 0;
}

uint8_t map_irq_pending_1(_cart *cart) {
    (void)cart;
    return 0;
}

uint8_t map_cpu_read_1(_cart* cart, uint16_t addr) {
    _mdata* mdata = (_mdata*)cart->mapper.data;
    uint8_t data = 0;

    if (addr >= 0x6000 && addr <= 0x7FFF) {
        if (cart->prg_ram.size) {
            uint32_t mask = cart->prg_ram.size - 1;
            uint32_t off = (addr - 0x6000) & mask;
            data = cart->prg_ram.data[off];
        } else if (cart->prg_nvram.size) {
            uint32_t mask = cart->prg_nvram.size - 1;
            uint32_t off = (addr - 0x6000) & mask;
            data = cart->prg_nvram.data[off];
        }
    } else if (0x8000 <= addr && addr <= 0xFFFF) {
        uint8_t mode = (mdata->control >> 2) & 3;
        uint32_t bank = 0;
        uint32_t bank_mask = (cart->prg_rom.size >> 14) - 1;

        switch (mode) {
            case 0:
            case 1:
                bank = (uint32_t)(mdata->prg_bank & 0x1E);
                if (addr >= 0xC000) bank++;
                break;

            case 2:
                bank = (addr < 0xC000) ? 0 : (uint32_t)mdata->prg_bank;
                break;

            default:
                bank = (addr < 0xC000) ? (uint32_t)mdata->prg_bank : bank_mask;
                break;
        }

        bank &= bank_mask;

        uint32_t off = (bank << 14) | (addr & 0x3FFF);
        off &= (cart->prg_rom.size - 1);

        data = cart->prg_rom.data[off];
    }

    return data;
}

void map_cpu_write_1(_cart* cart, uint16_t addr, uint8_t data) {
    _mdata* mdata = (_mdata*)cart->mapper.data;

    if (addr >= 0x6000 && addr <= 0x7FFF) {
        if (cart->prg_ram.size) {
            uint32_t mask = cart->prg_ram.size - 1;
            uint32_t off = (addr - 0x6000) & mask;
            cart->prg_ram.data[off] = data;
        } else if (cart->prg_nvram.size) {
            uint32_t mask = cart->prg_nvram.size - 1;
            uint32_t off = (addr - 0x6000) & mask;
            cart->prg_nvram.data[off] = data;
        }
    } else if (0x8000 <= addr && addr <= 0xFFFF) {
        if (data & 0x80) {
            mdata->load = 0;
            mdata->write_count = 0;
            mdata->control |= 0x0C;
            apply_control(cart, mdata);
        } else {
            mdata->load >>= 1;
            mdata->load |= (data & 1) << 4;
            mdata->write_count++;

            if (mdata->write_count == 5) {
                commit(cart, addr);
            }
        }
    }
}

uint8_t map_ppu_read_1(_cart* cart, uint16_t addr) {
    _mdata* mdata = (_mdata*)cart->mapper.data;
    uint8_t data = 0;

    if (0x0000 <= addr && addr <= 0x1FFF) {
        uint8_t mode = (mdata->control >> 4) & 1;
        uint32_t base;
        uint16_t inner;

        if (!mode) {
            uint8_t bank = mdata->chr_bank0 & 0x1E;
            base = (uint32_t)bank << 12;
            inner = addr & 0x1FFF;
        } else {
            uint8_t bank = (addr < 0x1000) ? mdata->chr_bank0 : mdata->chr_bank1;
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

void map_ppu_write_1(_cart* cart, uint16_t addr, uint8_t data) {
    if (cart->chr_ram.size && 0x0000 <= addr && addr <= 0x1FFF) {
        _mdata* mdata = (_mdata*)cart->mapper.data;
        uint8_t mode = (mdata->control >> 4) & 1;
        uint32_t base;
        uint16_t inner;

        if (!mode) {
            uint8_t bank = mdata->chr_bank0 & 0x1E;
            base = (uint32_t)bank << 12;
            inner = addr & 0x1FFF;
        } else {
            uint8_t bank = (addr < 0x1000) ? mdata->chr_bank0 : mdata->chr_bank1;
            base = (uint32_t)bank << 12;
            inner = addr & 0x0FFF;
        }

        uint32_t off = base + inner;
        uint32_t mask = cart->chr_ram.size - 1;
        off &= mask;

        cart->chr_ram.data[off] = data;
    }
}
