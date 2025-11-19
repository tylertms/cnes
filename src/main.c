#include "nes.h"

int main(int argc, char** argv) {
    _nes nes;

    nes_init(&nes);
    nes_reset(&nes);

    while (!nes.cpu.halt) {
        nes_clock(&nes);
    }

    return 0;
}
