#include "nes.h"
#include "cpu.h"
#include "gui.h"
#include "ppu.h"
#include <string.h>

uint8_t nes_init(_nes* nes, char* file, _gui* gui) {
    memset(nes, 0, sizeof(_nes));

    nes->cpu.p_ppu = &nes->ppu;
    nes->cpu.p_cart = &nes->cart;
    nes->cpu.p_input = &nes->input;
    nes->ppu.p_cart = &nes->cart;
    nes->ppu.p_gui = gui;
    nes->ppu.p_cpu = &nes->cpu;

    uint8_t res = cart_load(&nes->cart, file);
    if (res) {
        deinit_gui(gui);
        return 1;
    }

    nes_reset(nes);
    return 0;
}

void nes_reset(_nes* nes) {
    cpu_reset(&nes->cpu);
}

void nes_clock(_nes* nes) {
    uint8_t frame_complete = 0;
    while (!frame_complete) {
        frame_complete =
            ppu_clock(&nes->ppu) |
            ppu_clock(&nes->ppu) |
            ppu_clock(&nes->ppu) ;

        if (!nes->ppu.dma.is_transfer) {
            cpu_clock(&nes->cpu);
        } else {
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
                    nes->ppu.dma.data = cpu_read(&nes->cpu, (nes->ppu.dma.page << 8) | nes->ppu.dma.addr);
                }
            }
        }

        nes->master_clock++;
    }
}
