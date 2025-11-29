#include "ppu.h"
#include "cpu.h"
#include "cart.h"
#include "gui.h"
#include "mapper.h"
#include "palette.h"
#include <string.h>

static inline void ppu_bus_set(_ppu *ppu, uint8_t value) {
    ppu->ppudata = value;
    ppu->bus_decay = 0x8000;
}

static inline void ppu_bus_decay(_ppu *ppu) {
    if (ppu->bus_decay) {
        ppu->bus_decay--;
        if (!ppu->bus_decay) {
            ppu->ppudata = 0;
        }
    }
}

static inline uint8_t render_enabled(_ppu* ppu) {
    return ppu->ppumask & (BGRND_EN | SPRITE_EN);
}

static inline uint8_t bgrnd_enabled(_ppu* ppu) {
    return ppu->ppumask & BGRND_EN;
}

static inline uint8_t sprite_enabled(_ppu* ppu) {
    return ppu->ppumask & SPRITE_EN;
}

uint8_t ppu_clock(_ppu* ppu) {
    ppu_bus_decay(ppu);

    if (ppu->nmi_delay > 0) {
        ppu->nmi_delay--;

        if (ppu->nmi_delay == NMI_LATCH_THRESHOLD && (ppu->ppuctrl & NMI_EN) && (ppu->ppustatus & VBLANK)) {
             ppu->nmi_forced = 1;
        }

        if (ppu->nmi_delay == 0) {
            uint8_t active = (ppu->ppuctrl & NMI_EN) && (ppu->ppustatus & VBLANK);
            if (ppu->p_cpu && (active || ppu->nmi_forced)) {
                ppu->p_cpu->nmi_pending = 1;
                ppu->nmi_forced = 0;
            }
        }
    }

    uint8_t frame_complete = 0x00;

    const int scanline = ppu->scanline;
    const int cycle = ppu->cycle;

    const uint8_t rendering = render_enabled(ppu);
    const uint8_t visible_scanline = (scanline < NES_H);
    const uint8_t pre_render_scanline = (scanline == NES_ALL_HMAX);
    const uint8_t render_or_prerender = (pre_render_scanline || visible_scanline);

    if (pre_render_scanline && cycle == 1) {
        ppu->ppustatus &= ~(VBLANK | SPRITE_0_HIT | SPRITE_OVERFLOW);
        ppu->suppress_vbl_flag = 0;
        ppu->nmi_forced = 0;
        ppu_update_nmi_state(ppu);

        memset(ppu->sprite_pattern_low, 0x00, sizeof(ppu->sprite_pattern_low));
        memset(ppu->sprite_pattern_high, 0x00, sizeof(ppu->sprite_pattern_high));
    }

    if (scanline == 241 && cycle == 1) {
        if (ppu->suppress_vbl_flag) {
            ppu->suppress_vbl_flag = 0;
        } else {
            ppu->ppustatus |= VBLANK;
            ppu_update_nmi_state(ppu);
        }
        frame_complete = 0x01;
    } else if (ppu->suppress_vbl_flag && (scanline == 241 && cycle > 1)) {
        ppu->suppress_vbl_flag = 0;
    }

    if (render_or_prerender && rendering && cycle == 260 && ppu->p_cart) {
        if (ppu->p_cart->mapper_id == 4) {
            mmc3_scanline_tick(ppu->p_cart);
        }
    }

    if (render_or_prerender) {
        uint8_t in_visible_fetch_range = (cycle >= 2 && cycle <= 257);
        uint8_t in_blank_fetch_range = (cycle >= 321 && cycle <= 337);

        if (in_visible_fetch_range || in_blank_fetch_range) {
            update_shifters(ppu);

            if (rendering) {
                uint8_t stage = (uint8_t)((cycle - 1) & 0x07);

                if (stage == 0) {
                    load_bgrnd_shifters(ppu);
                    ppu->bgrnd_next_id = ppu_read(ppu, 0x2000 | (ppu->vram_addr & 0x0FFF));
                } else if (stage == 2) {
                    uint16_t v = ppu->vram_addr;
                    ppu->bgrnd_next_attr = ppu_read(
                        ppu,
                        0x23C0 |
                        (v & (NTBL_Y | NTBL_X)) |
                        ((v >> 4) & 0x38) |
                        ((v >> 2) & 0x07)
                    );

                    if (v & 0x0040) ppu->bgrnd_next_attr >>= 4;
                    if (v & 0x0002) ppu->bgrnd_next_attr >>= 2;
                    ppu->bgrnd_next_attr &= 0x03;
                } else if (stage == 4) {
                    ppu->bgrnd_next_low = ppu_read(
                        ppu,
                        ((uint16_t)(ppu->ppuctrl & BGRND_SEL) << 8) |
                        ((uint16_t)ppu->bgrnd_next_id << 4) |
                        ((ppu->vram_addr & FINE_Y) >> 12)
                    );
                } else if (stage == 6) {
                    ppu->bgrnd_next_high = ppu_read(
                        ppu,
                        ((uint16_t)(ppu->ppuctrl & BGRND_SEL) << 8) |
                        ((uint16_t)ppu->bgrnd_next_id << 4) |
                        ((ppu->vram_addr & FINE_Y) >> 12) + 8
                    );
                } else if (stage == 7) {
                    increment_scroll_x(ppu);
                }
            }
        }

        if (cycle == NES_W) {
            increment_scroll_y(ppu);
        }

        if (cycle == NES_W + 1) {
            load_bgrnd_shifters(ppu);
            transfer_addr_x(ppu);
        }

        if (rendering && (cycle == 338 || cycle == 340)) {
            ppu->bgrnd_next_id = ppu_read(ppu, 0x2000 | (ppu->vram_addr & 0x0FFF));
        }

        if (pre_render_scanline && cycle >= 280 && cycle <= 304) {
            transfer_addr_y(ppu);
        }

        if (cycle == 257) {
            memset(ppu->sprites, 0xFF, 0x08 * sizeof(_sprite));
            ppu->sprite_count = 0;
            ppu->sprite_0_hit_possible = 0;

            uint16_t next_scanline = (uint16_t)((scanline + 1) % (NES_ALL_HMAX + 1));

            if (rendering && next_scanline < NES_H && scanline < NES_H) {
                int16_t eval_scanline = (pre_render_scanline ? 0 : (int16_t)scanline);
                uint8_t oam_entry = 0;

                while (oam_entry < 64 && ppu->sprite_count < 9) {
                    int16_t offset = eval_scanline - (int16_t)ppu->oam[oam_entry].pos_y;
                    int16_t max_offset = (ppu->ppuctrl & SPRITE_HEIGHT) ? 16 : 8;

                    if (offset >= 0 && offset < max_offset) {
                        if (ppu->sprite_count < 8) {
                            if (!oam_entry) {
                                ppu->sprite_0_hit_possible = 1;
                            }
                            ppu->sprites[ppu->sprite_count++] = ppu->oam[oam_entry];
                        } else {
                            ppu->ppustatus |= SPRITE_OVERFLOW;
                        }
                    }

                    oam_entry++;
                }
            }
        }

        if (cycle == NES_ALL_WMAX) {
            uint16_t next_scanline = (uint16_t)((scanline + 1) % (NES_ALL_HMAX + 1));

            if (next_scanline < NES_H && scanline < NES_H) {
                for (uint8_t i = 0; i < ppu->sprite_count; i++) {
                    _sprite *s = &ppu->sprites[i];

                    int16_t line = (int16_t)scanline - (int16_t)s->pos_y;
                    uint8_t flip_v = (s->attr & FLIP_VERTICAL);
                    uint8_t flip_h = (s->attr & FLIP_HORIZONTAL);

                    uint16_t addr_low, addr_high;

                    if (ppu->ppuctrl & SPRITE_HEIGHT) {
                        uint8_t y = (uint8_t)line;
                        if (flip_v) y = 15 - y;

                        uint8_t tile = s->id & 0xFE;
                        if (y >= 8) tile++;

                        uint16_t table = (s->id & 0x01) ? 0x1000 : 0x0000;
                        addr_low = table | ((uint16_t)tile << 4) | (y & 0x07);
                    } else {
                        uint8_t y = (uint8_t)line & 0x07;
                        if (flip_v) y = 7 - y;

                        uint16_t base = (ppu->ppuctrl & SPRITE_SEL) ? 0x1000 : 0x0000;
                        addr_low = base | ((uint16_t)s->id << 4) | y;
                    }

                    addr_high = addr_low + 8;

                    uint8_t bits_lo = ppu_read(ppu, addr_low);
                    uint8_t bits_hi = ppu_read(ppu, addr_high);

                    if (flip_h) {
                        bits_lo = reverse_byte(bits_lo);
                        bits_hi = reverse_byte(bits_hi);
                    }

                    ppu->sprite_pattern_low[i] = bits_lo;
                    ppu->sprite_pattern_high[i] = bits_hi;
                }
            }
        }
    }

    uint8_t bgrnd_pixel = 0x00;
    uint8_t bgrnd_palette = 0x00;

    if (rendering) {
        uint16_t sel = (uint16_t)(0x8000u >> ppu->fine_x);

        uint8_t pixel_p0 = !!(ppu->bgrnd_pattern_low & sel);
        uint8_t pixel_p1 = !!(ppu->bgrnd_pattern_high & sel);
        bgrnd_pixel = (uint8_t)((pixel_p1 << 1) | pixel_p0);

        uint8_t pal0 = !!(ppu->bgrnd_attr_low & sel);
        uint8_t pal1 = !!(ppu->bgrnd_attr_high & sel);
        bgrnd_palette = (uint8_t)((pal1 << 1) | pal0);
    }

    uint8_t no_left_column = !(ppu->ppumask & BGRND_LC_EN) &&
                             cycle >= 1 && cycle <= 8;

    if (!bgrnd_enabled(ppu) || no_left_column) {
        bgrnd_pixel = 0;
    }

    uint8_t sprite_pixel = 0x00;
    uint8_t sprite_palette = 0x00;
    uint8_t sprite_priority = 0x00;

    if (sprite_enabled(ppu)) {
        ppu->sprite_0_rendered = 0;

        for (uint8_t i = 0; i < ppu->sprite_count; i++) {
            if (ppu->sprites[i].pos_x == 0) {
                uint8_t sp_lo = !!(ppu->sprite_pattern_low[i] & 0x80);
                uint8_t sp_hi = !!(ppu->sprite_pattern_high[i] & 0x80);
                sprite_pixel = (uint8_t)((sp_hi << 1) | sp_lo);

                sprite_palette = (ppu->sprites[i].attr & SPRITE_PALETTE) + 0x04;
                sprite_priority = !(ppu->sprites[i].attr & PRIORITY);

                if (sprite_pixel) {
                    if (i == 0) {
                        ppu->sprite_0_rendered = 1;
                    }
                    break;
                }
            }
        }
    }

    if (!(ppu->ppumask & SPRITE_LC_EN) && cycle >= 1 && cycle <= 8) {
        sprite_pixel = 0;
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
        uint8_t rendering_both = bgrnd_enabled(ppu) && sprite_enabled(ppu);

        uint8_t bg_clip_disabled = !(ppu->ppumask & BGRND_LC_EN);
        uint8_t sprite_clip_disabled = !(ppu->ppumask & SPRITE_LC_EN);
        uint8_t left_clip_active = bg_clip_disabled || sprite_clip_disabled;
        uint8_t min_x_pos = left_clip_active ? 9 : 1;

        uint8_t in_range =
            (cycle >= min_x_pos) &&
            (cycle < 256) &&
            (scanline < NES_H);

        if (will_sprite_0_hit && rendering_both && in_range) {
            ppu->ppustatus |= SPRITE_0_HIT;
        }

        if (sprite_priority) {
            pixel = sprite_pixel;
            palette = sprite_palette;
        } else {
            pixel = bgrnd_pixel;
            palette = bgrnd_palette;
        }
    }

    if (visible_scanline && cycle >= 1 && cycle <= NES_W) {
        uint8_t x = (uint8_t)(cycle - 1);
        uint8_t y = (uint8_t)scanline;

        uint32_t color = get_color(
            ppu, palette,
            (ppu->ppumask & EMPHASIS) >> 5,
            pixel
        );

        set_pixel(ppu->p_gui, x, y, color);
    }

    ppu->cycle++;

    if (pre_render_scanline && cycle == 339 &&
        render_enabled(ppu) && ppu->odd_frame) {
        ppu->cycle = NES_ALL_WMAX + 1;
    }

    if (ppu->cycle > NES_ALL_WMAX) {
        ppu->cycle = 0;

        if (++ppu->scanline > NES_ALL_HMAX) {
            ppu->scanline = 0;
            ppu->odd_frame = !ppu->odd_frame;
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
    uint8_t data = 0x00;
    switch (addr) {
        case PPUSTATUS: data = ppustatus_cpu_read(ppu); break;
        case OAMDATA:   data = oamdata_cpu_read(ppu);   break;
        case PPUDATA:   data = ppudata_cpu_read(ppu);   break;
        default:        data = ppu->ppudata;            break;
    }

    return data;
}

void ppu_cpu_write(_ppu* ppu, uint16_t addr, uint8_t data) {
    ppu_bus_set(ppu, data);

    switch (addr) {
        case PPUCTRL:   ppuctrl_cpu_write(ppu, data);   break;
        case PPUMASK:   ppumask_cpu_write(ppu, data);   break;
        case OAMADDR:   oamaddr_cpu_write(ppu, data);   break;
        case OAMDATA:   oamdata_cpu_write(ppu, data);   break;
        case PPUSCROLL: ppuscroll_cpu_write(ppu, data); break;
        case PPUADDR:   ppuaddr_cpu_write(ppu, data);   break;
        case PPUDATA:   ppudata_cpu_write(ppu, data);   break;
        case OAMDMA:    oamdma_cpu_write(ppu, data);    break;
        default: return;
    }
}

uint8_t ppustatus_cpu_read(_ppu* ppu) {
    uint8_t data = (ppu->ppustatus & 0xE0) | (ppu->ppudata & 0x1F);

    if (ppu->scanline == 241) {
        if (ppu->cycle == 1) {
            data &= ~VBLANK;
            ppu->suppress_vbl_flag = 1;
            if (ppu->nmi_delay > 0) ppu->nmi_delay = 0;
            ppu->nmi_forced = 0;
        } else if (ppu->cycle == 2) {
            data |= VBLANK;
            ppu->ppustatus &= ~VBLANK;
            if (ppu->nmi_delay > 0) ppu->nmi_delay = 0;
            ppu->nmi_forced = 0;
        } else if (ppu->cycle == 3) {
            data |= VBLANK;
            ppu->ppustatus &= ~VBLANK;
        } else {
            ppu->ppustatus &= ~VBLANK;
            if (!ppu->nmi_forced && ppu->nmi_delay > 0) {
                 ppu->nmi_delay = 0;
            }
        }
    } else {
        ppu->ppustatus &= ~VBLANK;
        if (!ppu->nmi_forced && ppu->nmi_delay > 0) ppu->nmi_delay = 0;
    }

    ppu->write_toggle = 0;
    ppu_update_nmi_state(ppu);
    ppu_bus_set(ppu, data);

    return data;
}

uint8_t oamdata_cpu_read(_ppu* ppu) {
    uint8_t data = ((uint8_t*)ppu->oam)[ppu->oamaddr];
    ppu_bus_set(ppu, data);
    return data;
}

uint8_t ppudata_cpu_read(_ppu* ppu) {
    uint16_t addr = ppu->vram_addr;
    uint8_t data = 0x00;

    if (0x3F00 <= addr && addr <= 0x3FFF) {
        uint16_t buffer_addr = addr & 0x2FFF;
        ppu->data_buffer = ppu_read(ppu, buffer_addr);

        uint8_t pal = ppu_read(ppu, addr) & 0x3F;
        data = (ppu->ppudata & 0xC0) | pal;
    } else {
        data = ppu->data_buffer;
        ppu->data_buffer = ppu_read(ppu, addr);
    }

    ppu->vram_addr += ppu->ppuctrl & INC_MODE ? 32 : 1;
    ppu_bus_set(ppu, data);

    return data;
}


void ppuctrl_cpu_write(_ppu* ppu, uint8_t data) {
    ppu->ppuctrl = data;
    ppu->tram_addr = (ppu->tram_addr & 0xF3FF) | ((data & 0x03) << 10);
    ppu_update_nmi_state(ppu);
}

void ppumask_cpu_write(_ppu* ppu, uint8_t data) {
    ppu->ppumask = data;
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
    switch (cart->mirror) {
        case MIRROR_HORIZONTAL: return (logical >> 1) & 1;
        case MIRROR_VERTICAL:   return logical & 1;
        case MIRROR_SINGLE0:    return 0;
        case MIRROR_SINGLE1:    return 1;
        case MIRROR_FOUR:       return logical & 3;
        default: return 0;
    }
}

void increment_scroll_x(_ppu* ppu) {
    if (!render_enabled(ppu)) return;

    if ((ppu->vram_addr & COARSE_X) == COARSE_X) {
        ppu->vram_addr &= ~COARSE_X;
        ppu->vram_addr ^= NTBL_X;
    } else {
        ppu->vram_addr += 0x0001;
    }
}

void increment_scroll_y(_ppu* ppu) {
    if (!render_enabled(ppu)) return;

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
    if (!render_enabled(ppu)) return;

    uint16_t tmask = NTBL_X | COARSE_X;
    ppu->vram_addr = (ppu->vram_addr & ~tmask) | (ppu->tram_addr & tmask);
}

void transfer_addr_y(_ppu* ppu) {
    if (!render_enabled(ppu)) return;

    uint16_t tmask = FINE_Y | NTBL_Y | COARSE_Y;
    ppu->vram_addr = (ppu->vram_addr & ~tmask) | (ppu->tram_addr & tmask);
}

void load_bgrnd_shifters(_ppu* ppu) {
    if (!render_enabled(ppu)) return;

    ppu->bgrnd_pattern_low = (ppu->bgrnd_pattern_low & 0xFF00) | ppu->bgrnd_next_low;
    ppu->bgrnd_pattern_high = (ppu->bgrnd_pattern_high & 0xFF00) | ppu->bgrnd_next_high;

    uint8_t next_attr_low = ppu->bgrnd_next_attr & 0x01 ? 0xFF : 0x00;
    uint8_t next_attr_high = ppu->bgrnd_next_attr & 0x02 ? 0xFF : 0x00;

    ppu->bgrnd_attr_low = (ppu->bgrnd_attr_low & 0xFF00) | next_attr_low;
    ppu->bgrnd_attr_high = (ppu->bgrnd_attr_high & 0xFF00) | next_attr_high;
}

void update_shifters(_ppu* ppu) {
    if (render_enabled(ppu)) {
        ppu->bgrnd_pattern_low <<= 1;
        ppu->bgrnd_pattern_high <<= 1;
        ppu->bgrnd_attr_low <<= 1;
        ppu->bgrnd_attr_high <<= 1;
    }

    uint8_t sprite_visible = ppu->cycle >= 1 && ppu->cycle <= (NES_W + 1);
    if (sprite_enabled(ppu) && sprite_visible) {
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

void ppu_update_nmi_state(_ppu *ppu) {
    uint8_t vblank = (ppu->ppustatus & VBLANK) != 0;
    uint8_t enabled = (ppu->ppuctrl & NMI_EN) != 0;

    uint8_t nmi_now = vblank && enabled;

    if (!ppu->nmi_previous && nmi_now) {
        ppu->nmi_delay = NMI_SIGNAL_LATENCY;
        ppu->nmi_forced = 0;
    }

    if (!nmi_now) {
        if (!ppu->nmi_forced) {
            ppu->nmi_delay = 0;
        }
    }

    ppu->nmi_previous = nmi_now;
}

uint32_t get_color(_ppu* ppu, uint8_t palette, uint8_t emphasis, uint8_t pixel) {
    return nes_pal[emphasis & 0x07][ppu_read(ppu, 0x3F00 + (palette << 2) + pixel) & 0x3F];
}
