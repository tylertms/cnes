#include "nes.h"
#include "apu.h"
#include "cnes.h"
#include "cpu.h"
#include "ppu.h"
#include <string.h>
#include <stdlib.h>

CNES_RESULT nes_init(_nes* nes) {
    char* rom_path = nes->cart.rom_path;
    memset(nes, 0, sizeof(_nes));

    nes->cpu.p_apu = &nes->apu;
    nes->cpu.p_ppu = &nes->ppu;
    nes->cpu.p_cart = &nes->cart;
    nes->cpu.p_input = &nes->input;
    nes->apu.p_cpu = &nes->cpu;
    nes->ppu.p_cart = &nes->cart;
    nes->ppu.p_cpu = &nes->cpu;

    nes->cart.rom_path = rom_path;

    if (apu_init(&nes->apu) != CNES_SUCCESS) {
        return CNES_FAILURE;
    }

    if (ppu_init(&nes->ppu) != CNES_SUCCESS) {
        return CNES_FAILURE;
    }

    if (nes->cart.rom_path) {
        if (cart_load(&nes->cart) != CNES_SUCCESS) {
            nes->cart.loaded = 0;
            return CNES_FAILURE;
        }

        nes->cart.loaded = 1;
        nes_soft_reset(nes);
    }

    return CNES_SUCCESS;
}

void nes_deinit(_nes* nes) {
    apu_deinit(&nes->apu);
    cart_unload(&nes->cart);
}

void nes_soft_reset(_nes* nes) {
    apu_reset(&nes->apu);
    cpu_reset(&nes->cpu);

    _cart* cart = &nes->cart;
    cart->mapper.deinit(cart);
    cart->mapper.init(cart);
}

void nes_hard_reset(_nes* nes) {
    nes_deinit(nes);
    nes_init(nes);
    nes->hard_reset_pending = 0;
}

void nes_clock(_nes* nes) {
    uint8_t frame_complete = 0;

    while (!frame_complete) {
        frame_complete =
            ppu_clock(&nes->ppu) |
            ppu_clock(&nes->ppu) |
            ppu_clock(&nes->ppu);

        apu_clock(&nes->apu);

        if (nes->apu.dmc.dma_active) {
            if (--nes->apu.dmc.dma_cycles_left == 0) {
                dmc_dma_complete(&nes->apu);
                nes->apu.dmc.dma_active = 0;
            }
        } else if (nes->ppu.dma.is_transfer) {
            if (nes->ppu.dma.dummy_cycle) {
                if (nes->master_clock & 1)
                    nes->ppu.dma.dummy_cycle = 0;
            } else {
                if (nes->master_clock & 1) {
                    ((uint8_t*)nes->ppu.oam)[nes->ppu.dma.addr++] = nes->ppu.dma.data;

                    if (!nes->ppu.dma.addr) {
                        nes->ppu.dma.is_transfer = 0;
                        nes->ppu.dma.dummy_cycle = 1;
                    }
                } else {
                    nes->ppu.dma.data = cpu_read(
                        &nes->cpu,
                        (nes->ppu.dma.page << 8) | nes->ppu.dma.addr
                    );
                }
            }
        } else {
            nes->cpu.irq_pending = (nes->apu.frame_counter_irq || nes->apu.dmc.irq_pending) ||
                    (nes->cart.mapper.irq_pending(&nes->cart));

            cpu_clock(&nes->cpu);
        }

        nes->master_clock++;
    }
}
