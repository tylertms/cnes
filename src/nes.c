#include "nes.h"
#include "cpu.h"
#include <string.h>

void nes_init(_nes* nes, _gui* gui) {
    memset(nes, 0, sizeof(_nes));

    nes->cpu.p_ppu = &nes->ppu;
    nes->cpu.p_cart = &nes->cart;
    nes->cpu.p_input = &nes->input;
    nes->ppu.p_cart = &nes->cart;
    nes->ppu.p_gui = gui;
    nes->ppu.p_cpu = &nes->cpu;
}

void nes_reset(_nes* nes) {
    cpu_reset(&nes->cpu);
}

inline void nes_clock(_nes* nes) {
    uint8_t frame_complete = 0;
    while (!frame_complete) {
        frame_complete =
            ppu_clock(&nes->ppu) |
            ppu_clock(&nes->ppu) |
            ppu_clock(&nes->ppu) ;

        cpu_clock(&nes->cpu);
    }
}
