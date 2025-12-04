#pragma once
#include "cart.h"
#include "cnes.h"
#include "nes.h"
#include <stdlib.h>

extern _mapper mapper_table[256];

#if defined(_MSC_VER)
    #pragma section(".CRT$XCU", read)
    #define REGISTER_MAPPER(id, init, deinit, irq, cpu_read, cpu_write, ppu_read, ppu_write) \
        static void register_mapper_##id(void) { \
            mapper_table[id] = (_mapper){init, deinit, irq, cpu_read, cpu_write, ppu_read, ppu_write, NULL}; \
        } \
        __declspec(allocate(".CRT$XCU")) void (*mapper_init_##id)(void) = register_mapper_##id;
#elif defined(__GNUC__) || defined(__clang__)
    #define REGISTER_MAPPER(id, init, deinit, irq, cpu_read, cpu_write, ppu_read, ppu_write) \
        __attribute__((constructor)) static void register_mapper_##id(void) { \
            mapper_table[id] = (_mapper){init, deinit, irq, cpu_read, cpu_write, ppu_read, ppu_write, NULL}; \
        }
#else
    #error "Compiler not supported for mapper registration. Please use GCC/Clang/MSVC."
#endif


CNES_RESULT mapper_load(_cart* cart);

// MISC
void mmc3_scanline_tick(_cart* cart);
