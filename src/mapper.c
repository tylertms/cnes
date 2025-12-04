#include "mapper.h"
#include <stdio.h>

_mapper mapper_table[256] = {0};

CNES_RESULT mapper_load(_cart* cart) {
    uint16_t id = cart->mapper_id & 0x0FFF;
    if (mapper_table[id].init) {
        cart->mapper = mapper_table[id];
        return cart->mapper.init(cart);
    } else {
        fprintf(stderr, "ERROR: Mapper %03d is currently unsupported!\n", id);
        return CNES_FAILURE;
    }
}
