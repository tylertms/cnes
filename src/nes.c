#include "nes.h"
#include <string.h>

void nes_init(_nes *nes) {
    memset(nes, 0, sizeof(_nes));

    // attach references for cpu-side access
    nes->cpu.p_ppu = &nes->ppu;
    nes->cpu.p_cart = &nes->cart;
}

void nes_reset(_nes *nes) {
    cpu_reset(&nes->cpu);
}

void nes_clock(_nes *nes) {
    ppu_clock(&nes->ppu);

    if (++nes->cpu_div == 3) {
        nes->cpu_div = 0;
        cpu_clock(&nes->cpu);
    }
}
