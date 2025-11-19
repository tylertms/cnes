#include "ppu.h"
#include "cart.h"

void ppu_clock(_ppu* ppu) {

}

uint8_t ppu_read(_ppu* ppu, uint16_t addr) {
    uint8_t data = 0x00;
    addr &= 0x3FFF;

    if (0x0000 <= addr && addr <= 0x1FFF) {
        data = cart_ppu_read(ppu->p_cart, addr);
    } else if (0x2000 <= addr && addr <= 0x3EFF) {
        uint16_t index = (addr - 0x2000) & 0x0FFF;
        uint8_t logic_nt = index >> 10;
        uint16_t offset = index & 0x03FF;

        uint8_t physical_nt = physical_nametable(ppu->p_cart, logic_nt);
        data = ppu->nametable[(physical_nt << 10) | offset];

    } else if (0x3F00 <= addr && addr <= 0x3FFF) {
        uint8_t backdrop = (addr & 0x03) == 0x00;
        if (backdrop) data = ppu->palette_idx[addr & 0x0F];
        else data = ppu->palette_idx[addr & 0x1F];
    }

    return data;
}

void ppu_write(_ppu* ppu, uint16_t addr, uint8_t data) {
    addr &= 0x3FFF;

    if (0x0000 <= addr && addr <= 0x1FFF) {
        cart_ppu_write(ppu->p_cart, addr, data);
    } else if (0x2000 <= addr && addr <= 0x3EFF) {
        uint16_t index = (addr - 0x2000) & 0x0FFF;
        uint8_t logic_nt = index >> 10;
        uint16_t offset = index & 0x03FF;

        uint8_t phys_nt = physical_nametable(ppu->p_cart, logic_nt);
        ppu->nametable[(phys_nt << 10) | offset] = data;

    } else if (0x3F00 <= addr && addr <= 0x3FFF) {
        uint8_t backdrop = (addr & 0x03) == 0x00;
        if (backdrop) ppu->palette_idx[addr & 0x0F] = data;
        else ppu->palette_idx[addr & 0x1F] = data;
    }
}


uint8_t ppu_cpu_read(_ppu* ppu, uint16_t addr) {
    switch (addr) {
        case PPUSTATUS: return ppustatus_cpu_read(ppu);
        case OAMDATA:   return oamdata_cpu_read(ppu);
        case PPUDATA:   return ppudata_cpu_read(ppu);
        default:        return 0x00;
    }
}

void ppu_cpu_write(_ppu* ppu, uint16_t addr, uint8_t data) {
    switch (addr) {
        case PPUCTRL:   return ppuctrl_cpu_write(ppu, data);
        case PPUMASK:   return ppumask_cpu_write(ppu, data);
        case OAMADDR:   return oamaddr_cpu_write(ppu, data);
        case OAMDATA:   return oamdata_cpu_write(ppu, data);
        case PPUSCROLL: return ppuscroll_cpu_write(ppu, data);
        case PPUADDR:   return ppuaddr_cpu_write(ppu, data);
        case PPUDATA:   return ppudata_cpu_write(ppu, data);
        case OAMDMA:    return oamdma_cpu_write(ppu, data);
        default:        return;
    }
}

uint8_t ppustatus_cpu_read(_ppu* ppu) {
    uint8_t data = (ppu->ppustatus & 0xE0) | (ppu->data_buffer & 0x1F);
    ppu->ppustatus &= ~VBLANK;
    ppu->write_toggle = 0;

    return data;
}

uint8_t oamdata_cpu_read(_ppu* ppu) {
    uint8_t data = 0x00;
    return 0x00;
}

uint8_t ppudata_cpu_read(_ppu* ppu) {
    uint8_t data = ppu->data_buffer;
    ppu->data_buffer = ppu_read(ppu, ppu->vram_addr);

    if (0x3F00 <= ppu->vram_addr && ppu->vram_addr <= 0x3FFF) {
        data = ppu->data_buffer;
    }

    uint8_t increment = ppu->ppuctrl & INC_MODE ? 32 : 1;
    ppu->vram_addr += increment;

    return data;
}


void ppuctrl_cpu_write(_ppu* ppu, uint8_t data) {
    ppu->ppuctrl = data;
}

void ppumask_cpu_write(_ppu* ppu, uint8_t data) {
    ppu->ppumask = data;
}

void oamaddr_cpu_write(_ppu* ppu, uint8_t data) {

}

void oamdata_cpu_write(_ppu* ppu, uint8_t data) {

}

void ppuscroll_cpu_write(_ppu* ppu, uint8_t data) {

}

void ppuaddr_cpu_write(_ppu* ppu, uint8_t data) {
    if (!ppu->write_toggle) {
        ppu->tmp_vram_addr = (ppu->tmp_vram_addr & 0x00FF) | (data << 8);
        ppu->write_toggle = 1;
    } else {
        ppu->tmp_vram_addr = (ppu->tmp_vram_addr & 0xFF00) | data;
        ppu->write_toggle = 0;
        ppu->vram_addr = ppu->tmp_vram_addr;
    }
}

void ppudata_cpu_write(_ppu* ppu, uint8_t data) {
    ppu_write(ppu, ppu->vram_addr, data);
    uint8_t increment = ppu->ppuctrl & INC_MODE ? 32 : 1;
    ppu->vram_addr += increment;
}

void oamdma_cpu_write(_ppu* ppu, uint8_t data) {

}

uint8_t physical_nametable(_cart* cart, uint8_t logical) {
    if (cart->alt_ntbl_layout) {
        return logical & 0x03;
    }

    if (cart->ntbl_layout == 0) {
        return (logical & 0x02) ? 1 : 0;
    } else {
        return (logical & 0x01) ? 1 : 0;
    }
}
