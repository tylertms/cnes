#include "ppu.h"
#include "cpu.h"
#include "cart.h"
#include "gui.h"
#include "palette.h"

uint8_t ppu_clock(_ppu* ppu) {
    if (ppu->scanline == NES_ALL_HMAX || ppu->scanline < NES_H) {
        if (ppu->scanline == 0 && ppu->cycle == 0) {
            uint8_t skip = ppu->even_frame && (ppu->ppumask & (BGRND_EN | SPRITE_EN));
            if (skip) ppu->cycle = 1;
        }

        if (ppu->scanline == NES_ALL_HMAX) {
            if (ppu->cycle == 0) {
                ppu->even_frame = !ppu->even_frame;
            } else if (ppu->cycle == 1) {
                ppu->ppustatus &= ~(VBLANK | SPRITE_0_HIT | SPRITE_OVERFLOW);

                memset(ppu->sprite_pattern_low, 0x00, sizeof(ppu->sprite_pattern_low));
                memset(ppu->sprite_pattern_high, 0x00, sizeof(ppu->sprite_pattern_high));
            }
        }

        uint8_t vis_fetch_range = ppu->cycle >= 2 && ppu->cycle <= 257;
        uint8_t blank_fetch_range = ppu->cycle >= 321 && ppu->cycle <= 337;
        if (vis_fetch_range || blank_fetch_range) {
            update_shifters(ppu);

            uint8_t stage = (ppu->cycle - 1) & 0x07;
            if (stage == 0) {
                load_bgrnd_shifters(ppu);
                ppu->bgrnd_next_id = ppu_read(ppu, 0x2000 | (ppu->vram_addr & 0xFFF));
            } else if (stage == 2) {
                ppu->bgrnd_next_attr = ppu_read(ppu, 0x23C0 | (ppu->vram_addr & (NTBL_Y | NTBL_X))
                                                            | ((ppu->vram_addr >> 4) & 0x38)
                                                            | ((ppu->vram_addr >> 2) & 0x07));
                if (ppu->vram_addr & 0x0040)
                    ppu->bgrnd_next_attr >>= 4;
                if (ppu->vram_addr & 0x0002)
                    ppu->bgrnd_next_attr >>= 2;

                ppu->bgrnd_next_attr &= 0x03;
            } else if (stage == 4) {
                ppu->bgrnd_next_low = ppu_read(ppu, ((uint16_t)(ppu->ppuctrl & BGRND_SEL) << 8) +
                                                    ((uint16_t)ppu->bgrnd_next_id << 4) +
                                                    ((ppu->vram_addr & FINE_Y) >> 12) + 0);
            } else if (stage == 6) {
                ppu->bgrnd_next_high = ppu_read(ppu, ((uint16_t)(ppu->ppuctrl & BGRND_SEL) << 8) +
                                                    ((uint16_t)ppu->bgrnd_next_id << 4) +
                                                    ((ppu->vram_addr & FINE_Y) >> 12) + 8);
            } else if (stage == 7) {
                increment_scroll_x(ppu);
            }
        }

        if (ppu->cycle == NES_W) {
            increment_scroll_y(ppu);
        }

        if (ppu->cycle == (NES_W + 1)) {
            load_bgrnd_shifters(ppu);
            transfer_addr_x(ppu);
        }

        if (ppu->cycle == 338 || ppu->cycle == 340) {
            ppu->bgrnd_next_id = ppu_read(ppu, 0x2000 | (ppu->vram_addr & 0x0FFF));
        }

        if (ppu->scanline == NES_ALL_HMAX && ppu->cycle >= 280 && ppu->cycle <= 304) {
            transfer_addr_y(ppu);
        }


        // TODO: use proper cycles to perform these operations
        if (ppu->cycle == 257) {
            memset(ppu->sprites, 0xFF, 0x08 * sizeof(_sprite));
            ppu->sprite_count = 0;
            ppu->sprite_0_hit_possible = 0;

            uint16_t next_scanline = (ppu->scanline + 1) % (NES_ALL_HMAX + 1);

            if (next_scanline < NES_H) {
                uint8_t oam_entry = 0;
                while (oam_entry < 64 && ppu->sprite_count < 9) {
                    int16_t offset = (int16_t)ppu->scanline - (int16_t)ppu->oam[oam_entry].pos_y;
                    int16_t max_offset = ppu->ppuctrl & SPRITE_HEIGHT ? 16 : 8;

                    if (offset >= 0 && offset < max_offset) {
                        if (ppu->sprite_count < 8) {
                            if (!oam_entry) ppu->sprite_0_hit_possible = 1;
                            ppu->sprites[ppu->sprite_count++] = ppu->oam[oam_entry];
                        } else {
                            ppu->ppustatus |= SPRITE_OVERFLOW;
                        }
                    }

                    oam_entry++;
                }
            }
        }

        if (ppu->cycle == NES_ALL_WMAX) {
            uint16_t next_scanline = (ppu->scanline + 1) % (NES_ALL_HMAX + 1);

            if (next_scanline < NES_H) {
                for (uint8_t i = 0; i < ppu->sprite_count; i++) {
                    uint8_t sprite_pattern_bits_low;
                    uint8_t sprite_pattern_bits_high;
                    uint16_t sprite_pattern_addr_low;
                    uint16_t sprite_pattern_addr_high;

                    if (ppu->ppuctrl & SPRITE_HEIGHT) {
                        if (ppu->sprites[i].attr & FLIP_VERTICAL) {
                            if (ppu->scanline - ppu->sprites[i].pos_y < 8) {
                                sprite_pattern_addr_low = ((ppu->sprites[i].id & 0x01) << 12) |
                                    (((ppu->sprites[i].id & 0xFE) + 1) << 4) |
                                    (7 - (ppu->scanline - ppu->sprites[i].pos_y) & 0x07);
                            } else {
                                sprite_pattern_addr_low = ((ppu->sprites[i].id & 0x01) << 12) |
                                    ((ppu->sprites[i].id & 0xFE) << 4) |
                                    (7 - (ppu->scanline - ppu->sprites[i].pos_y) & 0x07);
                            }
                        } else {
                            if (ppu->scanline - ppu->sprites[i].pos_y < 8) {
                                sprite_pattern_addr_low = ((ppu->sprites[i].id & 0x01) << 12) |
                                    ((ppu->sprites[i].id & 0xFE) << 4) |
                                    ((ppu->scanline - ppu->sprites[i].pos_y) & 0x07);
                            } else {
                                sprite_pattern_addr_low = ((ppu->sprites[i].id & 0x01) << 12) |
                                    (((ppu->sprites[i].id & 0xFE) + 1) << 4) |
                                    ((ppu->scanline - ppu->sprites[i].pos_y) & 0x07);
                            }
                        }
                    } else {
                        if (ppu->sprites[i].attr & FLIP_VERTICAL) {
                            sprite_pattern_addr_low = ((ppu->ppuctrl & SPRITE_SEL) << 9) |
                                (ppu->sprites[i].id << 4) |
                                (7 - (ppu->scanline - ppu->sprites[i].pos_y));
                        } else {
                            sprite_pattern_addr_low = ((ppu->ppuctrl & SPRITE_SEL) << 9) |
                                (ppu->sprites[i].id << 4) |
                                (ppu->scanline - ppu->sprites[i].pos_y);
                        }
                    }

                    sprite_pattern_addr_high = sprite_pattern_addr_low + 8;

                    sprite_pattern_bits_low = ppu_read(ppu, sprite_pattern_addr_low);
                    sprite_pattern_bits_high = ppu_read(ppu, sprite_pattern_addr_high);

                    if (ppu->sprites[i].attr & FLIP_HORIZONTAL) {
                        sprite_pattern_bits_low = reverse_byte(sprite_pattern_bits_low);
                        sprite_pattern_bits_high = reverse_byte(sprite_pattern_bits_high);
                    }

                    ppu->sprite_pattern_low[i] = sprite_pattern_bits_low;
    				ppu->sprite_pattern_high[i] = sprite_pattern_bits_high;
                }
            }
        }

        if (ppu->scanline == NES_ALL_HMAX && ppu->cycle == 1) {
            uint8_t skip = ppu->even_frame && (ppu->ppumask & (BGRND_EN | SPRITE_EN));
            if (skip) ppu->cycle = 1;
        }
    }

    uint8_t frame_complete = 0x00;

    if ((ppu->scanline == NES_H + 1) && ppu->cycle == 1) {
        ppu->ppustatus |= VBLANK;
        frame_complete = 0x01;

        if (ppu->ppuctrl & NMI_EN) {
            ppu->p_cpu->nmi_pending = 1;
        }
    }

    uint8_t bgrnd_pixel = 0x00;
    uint8_t bgrnd_palette = 0x00;

    if (ppu->ppumask & BGRND_EN) {
        uint16_t sel = 0x8000 >> ppu->fine_x;

        uint8_t pixel_p0 = !!(ppu->bgrnd_pattern_low & sel);
        uint8_t pixel_p1 = !!(ppu->bgrnd_pattern_high & sel);
        bgrnd_pixel = (pixel_p1 << 1) | pixel_p0;

        uint8_t bgrnd_palette0 = !!(ppu->bgrnd_attr_low & sel);
        uint8_t bgrnd_palette1 = !!(ppu->bgrnd_attr_high & sel);
        bgrnd_palette = (bgrnd_palette1 << 1) | bgrnd_palette0;
    }

    uint8_t sprite_pixel = 0x00;
    uint8_t sprite_palette = 0x00;
    uint8_t sprite_priority = 0x00;

    if (ppu->ppumask & SPRITE_EN) {
        ppu->sprite_0_rendered = 0;
        for (uint8_t i = 0; i < ppu->sprite_count; i++) {
            if (!ppu->sprites[i].pos_x) {
                uint8_t sprite_pixel_low = !!(ppu->sprite_pattern_low[i] & 0x80);
                uint8_t sprite_pixel_high = !!(ppu->sprite_pattern_high[i] & 0x80);
                sprite_pixel = (sprite_pixel_high << 1) | sprite_pixel_low;

                sprite_palette = (ppu->sprites[i].attr & SPRITE_PALETTE) + 0x04;
                sprite_priority = !(ppu->sprites[i].attr & PRIORITY);

                if (sprite_pixel) {
                    if (i == 0) ppu->sprite_0_rendered = 1;
                    break;
                }
            }
        }
    }

    uint8_t pixel = 0x00;
    uint8_t palette = 0x00;

    if (!bgrnd_pixel && sprite_pixel) {
        pixel = sprite_pixel;
        palette = sprite_palette;
    } else if (bgrnd_pixel && !sprite_pixel) {
        pixel = bgrnd_pixel;
        palette = bgrnd_palette;
    } else if (bgrnd_pixel && sprite_pixel) {
        uint8_t will_sprite_0_hit = ppu->sprite_0_hit_possible && ppu->sprite_0_rendered;
        uint8_t render_enabled = (ppu->ppumask & BGRND_EN) && (ppu->ppumask & SPRITE_EN);
        uint8_t min_x_pos = (ppu->ppumask & (BGRND_LC_EN | SPRITE_LC_EN)) ? 1 : 9;
        uint8_t in_range = ppu->cycle >= min_x_pos && ppu->cycle < 258 && ppu->scanline < NES_H;
        if (will_sprite_0_hit && render_enabled && in_range) {
            ppu->ppustatus |= SPRITE_0_HIT;
        }

        pixel = sprite_priority ? sprite_pixel : bgrnd_pixel;
        palette = sprite_priority ? sprite_palette : bgrnd_palette;
    }


    if (ppu->scanline < NES_H && ppu->cycle >= 1 && ppu->cycle <= NES_W) {
        uint8_t x = (uint8_t)(ppu->cycle - 1);
        uint8_t y = (uint8_t)ppu->scanline;
        uint32_t color = get_color(ppu, palette, (ppu->ppumask & EMPHASIS) >> 5, pixel);

        set_pixel(ppu->p_gui, x, y, color);
    }


    if (++ppu->cycle > NES_ALL_WMAX) {
        ppu->cycle = 0;

        if (++ppu->scanline > NES_ALL_HMAX) {
            ppu->scanline = 0;
        }
    }

    return frame_complete;
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
    uint8_t data = ((uint8_t*)ppu->oam)[ppu->oamaddr];
    return data;
}

uint8_t ppudata_cpu_read(_ppu* ppu) {
    uint8_t data = ppu->data_buffer;
    ppu->data_buffer = ppu_read(ppu, ppu->vram_addr);

    if (0x3F00 <= ppu->vram_addr && ppu->vram_addr <= 0x3FFF) {
        data = ppu->data_buffer;
    }

    ppu->vram_addr += ppu->ppuctrl & INC_MODE ? 32 : 1;

    return data;
}


void ppuctrl_cpu_write(_ppu* ppu, uint8_t data) {
    ppu->ppuctrl = data;
    ppu->tram_addr = (ppu->tram_addr & 0xF3FF) | ((data & 0x03) << 10);
}

void ppumask_cpu_write(_ppu* ppu, uint8_t data) {
    ppu->ppumask = data;
    ppu->write_toggle = 0;
}

void oamaddr_cpu_write(_ppu* ppu, uint8_t data) {
    ppu->oamaddr = data;
}

void oamdata_cpu_write(_ppu* ppu, uint8_t data) {
    ((uint8_t*)ppu->oam)[ppu->oamaddr++] = data;
}

void ppuscroll_cpu_write(_ppu* ppu, uint8_t data) {
    if (!ppu->write_toggle) {
        ppu->fine_x = data & 0x07;
        ppu->tram_addr = (ppu->tram_addr & ~COARSE_X) | ((data >> 3) & COARSE_X);
        ppu->write_toggle = 1;
    } else {
        uint16_t mask = FINE_Y | COARSE_Y;
        ppu->tram_addr = (ppu->tram_addr & ~mask) | ((data & 0x07) << 12) | ((data >> 3) << 5);
        ppu->write_toggle = 0;
    }
}

void ppuaddr_cpu_write(_ppu* ppu, uint8_t data) {
    if (!ppu->write_toggle) {
        ppu->tram_addr = (ppu->tram_addr & 0x00FF) | ((data & 0x3F) << 8);
        ppu->write_toggle = 1;
    } else {
        ppu->tram_addr = (ppu->tram_addr & 0xFF00) | data;
        ppu->write_toggle = 0;
        ppu->vram_addr = ppu->tram_addr;
    }
}

void ppudata_cpu_write(_ppu* ppu, uint8_t data) {
    ppu_write(ppu, ppu->vram_addr, data);
    ppu->vram_addr += ppu->ppuctrl & INC_MODE ? 32 : 1;
}

void oamdma_cpu_write(_ppu* ppu, uint8_t data) {
    ppu->dma.page = data;
    ppu->dma.addr = 0x00;
    ppu->dma.data = 0x00;
    ppu->dma.is_transfer = 1;
    ppu->dma.dummy_cycle = 1;
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

void increment_scroll_x(_ppu* ppu) {
    if (!(ppu->ppumask & (BGRND_EN | SPRITE_EN))) {
        return;
    }

    if ((ppu->vram_addr & COARSE_X) == COARSE_X) {
        ppu->vram_addr &= ~COARSE_X;
        ppu->vram_addr ^= NTBL_X;
    } else {
        ppu->vram_addr += 0x0001;
    }
}

void increment_scroll_y(_ppu* ppu) {
    if (!(ppu->ppumask & (BGRND_EN | SPRITE_EN))) {
        return;
    }

    if ((ppu->vram_addr & FINE_Y) != FINE_Y) {
        ppu->vram_addr += 0x1000;
        return;
    }

    ppu->vram_addr &= ~FINE_Y;

    uint16_t y = (ppu->vram_addr & COARSE_Y) >> 5;
    if (y == 29) {
        ppu->vram_addr &= ~COARSE_Y;
        ppu->vram_addr ^= NTBL_Y;
    } else if (y == 31) {
        ppu->vram_addr &= ~COARSE_Y;
    } else {
        ppu->vram_addr += 0x20;
    }
}

void transfer_addr_x(_ppu* ppu) {
    if (!(ppu->ppumask & (BGRND_EN | SPRITE_EN))) {
        return;
    }

    uint16_t tmask = NTBL_X | COARSE_X;
    ppu->vram_addr = (ppu->vram_addr & ~tmask) | (ppu->tram_addr & tmask);
}

void transfer_addr_y(_ppu* ppu) {
    if (!(ppu->ppumask & (BGRND_EN | SPRITE_EN))) {
        return;
    }

    uint16_t tmask = FINE_Y | NTBL_Y | COARSE_Y;
    ppu->vram_addr = (ppu->vram_addr & ~tmask) | (ppu->tram_addr & tmask);
}

void load_bgrnd_shifters(_ppu* ppu) {
    ppu->bgrnd_pattern_low = (ppu->bgrnd_pattern_low & 0xFF00) | ppu->bgrnd_next_low;
    ppu->bgrnd_pattern_high = (ppu->bgrnd_pattern_high & 0xFF00) | ppu->bgrnd_next_high;

    uint8_t next_attr_low = ppu->bgrnd_next_attr & 0x01 ? 0xFF : 0x00;
    uint8_t next_attr_high = ppu->bgrnd_next_attr & 0x02 ? 0xFF : 0x00;

    ppu->bgrnd_attr_low = (ppu->bgrnd_attr_low & 0xFF00) | next_attr_low;
    ppu->bgrnd_attr_high = (ppu->bgrnd_attr_high & 0xFF00) | next_attr_high;
}

void update_shifters(_ppu* ppu) {
    if (ppu->ppumask & BGRND_EN) {
        ppu->bgrnd_pattern_low <<= 1;
        ppu->bgrnd_pattern_high <<= 1;
        ppu->bgrnd_attr_low <<= 1;
        ppu->bgrnd_attr_high <<= 1;
    }

    uint8_t sprite_visible = ppu->cycle >= 1 && ppu->cycle <= (NES_W + 1);
    if (ppu->ppumask & SPRITE_EN && sprite_visible) {
        for (uint8_t i = 0; i < ppu->sprite_count; i++) {
            if (ppu->sprites[i].pos_x) {
                ppu->sprites[i].pos_x--;
            } else {
                ppu->sprite_pattern_low[i] <<= 1;
                ppu->sprite_pattern_high[i] <<= 1;
            }
        }
    }


}

uint32_t get_color(_ppu* ppu, uint8_t palette, uint8_t emphasis, uint8_t pixel) {
    return nes_pal[emphasis & 0x07][ppu_read(ppu, 0x3F00 + (palette << 2) + pixel) & 0x3F];
}
