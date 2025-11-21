// cpu.c
#include "cpu.h"
#include "cart.h"
#include "input.h"
#include "ppu.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

inline void cpu_clock(_cpu* cpu) {
    cpu->total_cycles++;
    if (--cpu->cycles) return;

    if (cpu->nmi_pending) {
        cpu->nmi_pending = 0;
        cpu_nmi(cpu, 0);
        return;
    }

    if (cpu->irq_pending && !get_flag(cpu, IRQ_DS)) {
        if (cpu->irq_delay) {
            cpu->irq_delay = 0;
        } else {
            cpu_irq(cpu);
            return;
        }
    }

    uint8_t opcode = cpu_read(cpu, cpu->pc++);
    cpu->instr = instructions[opcode];
    cpu->cycles = cpu->instr.cycles;
    uint8_t am_cycle = cpu->instr.ex_am(cpu);

    // print_state(cpu);
    // printf("\n");

    uint8_t op_cycle = cpu->instr.ex_op(cpu);
    cpu->cycles += (am_cycle & op_cycle);
}

void cpu_reset(_cpu* cpu) {
    uint16_t low = cpu_read(cpu, RST_VECTOR);
    uint16_t high = cpu_read(cpu, RST_VECTOR + 1);
    cpu->pc = (high << 8) | low;

    cpu->a = 0x00;
    cpu->x = 0x00;
    cpu->y = 0x00;
    cpu->s = 0xFD;
    cpu->p = 0x00 | UNUSED | IRQ_DS;

    cpu->op_addr = 0x0000;
    cpu->op_data = 0x00;

    cpu->cycles = 7;
    cpu->halt = 0;
}

void cpu_irq(_cpu* cpu) {
    if (get_flag(cpu, IRQ_DS))
        return;

    push(cpu, cpu->pc >> 8);
    push(cpu, cpu->pc & 0xFF);

    set_flag(cpu, BREAK, 0);
    set_flag(cpu, UNUSED, 1);
    set_flag(cpu, IRQ_DS, 1);
    push(cpu, cpu->p);

    uint16_t low = cpu_read(cpu, IRQ_VECTOR);
    uint16_t high = cpu_read(cpu, IRQ_VECTOR + 1);
    cpu->pc = (high << 8) | low;
    cpu->cycles = 7;
}

void cpu_nmi(_cpu* cpu, uint8_t brk) {
    if (brk) cpu->pc++;

    push(cpu, cpu->pc >> 8);
    push(cpu, cpu->pc & 0xFF);

    set_flag(cpu, BREAK, brk);
    set_flag(cpu, UNUSED, 1);
    set_flag(cpu, IRQ_DS, 1);
    push(cpu, cpu->p);
    set_flag(cpu, BREAK, 0);

    uint16_t low = cpu_read(cpu, brk ? IRQ_VECTOR : NMI_VECTOR);
    uint16_t high = cpu_read(cpu, brk ? IRQ_VECTOR + 1 : NMI_VECTOR + 1);
    cpu->pc = (high << 8) | low;
    cpu->cycles = 8;
}

uint8_t cpu_read(_cpu* cpu, uint16_t addr) {
    uint8_t data = 0x00;

    if (0x0000 <= addr && addr <= 0x1FFF) {
        data = cpu->ram[addr & 0x07FF];
    } else if (0x2000 <= addr && addr <= 0x3FFF) {
        if (cpu->p_ppu) {
            uint16_t reg_addr = 0x2000 | (addr & 0x0007);
            data = ppu_cpu_read(cpu->p_ppu, reg_addr);
        }
    } else if (0x4016 <= addr && addr <= 0x4017) {
        data = !!(cpu->p_input->input_state[addr & 0x0001] & 0x80);
        cpu->p_input->input_state[addr & 0x0001] <<= 1;
    } else if (0x4020 <= addr && addr <= 0xFFFF) {
        if (cpu->p_cart) {
            data = cart_cpu_read(cpu->p_cart, addr);
        }
    }

    return data;
}

void cpu_write(_cpu* cpu, uint16_t addr, uint8_t data) {
    if (0x0000 <= addr && addr <= 0x1FFF) {
        cpu->ram[addr & 0x07FF] = data;
    } else if (0x2000 <= addr && addr <= 0x3FFF) {
        if (cpu->p_ppu) {
            uint16_t reg_addr = 0x2000 | (addr & 0x0007);
            ppu_cpu_write(cpu->p_ppu, reg_addr, data);
        }
    } else if (0x4016 <= addr && addr <= 0x4017) {
        uint8_t snapshot = cpu->p_input->controller[addr & 0x0001];
        cpu->p_input->input_state[addr & 0x0001] = snapshot;
    } else if (0x4020 <= addr && addr <= 0xFFFF) {
        if (cpu->p_cart) {
            cart_cpu_write(cpu->p_cart, addr, data);
        }
    }
}

uint8_t no_fetch(_cpu* cpu) {
    return cpu->instr.mode_num == _imp ||
        cpu->instr.mode_num == _acc;
}

uint8_t cpu_fetch(_cpu* cpu) {
    if (no_fetch(cpu)) return cpu->op_data;
    return cpu_read(cpu, cpu->op_addr);
}

void cpu_write_back(_cpu* cpu, uint8_t result) {
    if (no_fetch(cpu)) cpu->a = result & 0xFF;
    else cpu_write(cpu, cpu->op_addr, result & 0xFF);
}

uint8_t get_flag(_cpu* cpu, _cpu_flag flag) {
    return !!(cpu->p & flag);
}

void set_flag(_cpu* cpu, _cpu_flag flag, uint8_t set) {
    if (set) cpu->p |= flag;
    else cpu->p &= ~flag;
}

void push(_cpu* cpu, uint8_t data) {
    cpu_write(cpu, 0x0100 + cpu->s--, data);
}

uint8_t pull(_cpu* cpu) {
    return cpu_read(cpu, 0x0100 + ++cpu->s);
}

void branch(_cpu* cpu) {
    cpu->cycles++;
    uint16_t res = cpu->op_addr + cpu->pc;

    if ((res & 0xFF00) != (cpu->pc & 0xFF00))
        cpu->cycles++;

    cpu->pc = res;
}

/* address modes */

uint8_t am_acc(_cpu* cpu) {
    cpu->op_data = cpu->a;
    return 0;
}

uint8_t am_imp(_cpu* cpu) {
    cpu->op_data = cpu->a;
    return 0;
}

uint8_t am_imm(_cpu* cpu) {
    cpu->op_addr = cpu->pc++;
    return 0;
}

uint8_t am_zpg(_cpu* cpu) {
    cpu->op_addr = cpu_read(cpu, cpu->pc++);
    return 0;
}

uint8_t am_zpx(_cpu* cpu) {
    cpu->op_addr = (cpu_read(cpu, cpu->pc++) + cpu->x) & 0xFF;
    return 0;
}

uint8_t am_zpy(_cpu* cpu) {
    cpu->op_addr = (cpu_read(cpu, cpu->pc++) + cpu->y) & 0xFF;
    return 0;
}

uint8_t am_abs(_cpu* cpu) {
    uint16_t low = cpu_read(cpu, cpu->pc++);
    uint16_t high = cpu_read(cpu, cpu->pc++);
    cpu->op_addr = (high << 8) | low;
    return 0;
}

uint8_t am_abx(_cpu* cpu) {
    uint16_t low = cpu_read(cpu, cpu->pc++);
    uint16_t high = cpu_read(cpu, cpu->pc++);
    cpu->op_addr = ((high << 8) | low) + cpu->x;
    return (cpu->op_addr & 0xFF00) != (high << 8);
}

uint8_t am_aby(_cpu* cpu) {
    uint16_t low = cpu_read(cpu, cpu->pc++);
    uint16_t high = cpu_read(cpu, cpu->pc++);
    cpu->op_addr = ((high << 8) | low) + cpu->y;
    return (cpu->op_addr & 0xFF00) != (high << 8);
}

uint8_t am_idr(_cpu* cpu) {
    uint16_t p_low = cpu_read(cpu, cpu->pc++);
    uint16_t p_high = cpu_read(cpu, cpu->pc++);
    uint16_t ptr = (p_high << 8) | p_low;

    uint16_t high_addr = (p_low == 0xFF) ? (ptr & 0xFF00) : (ptr + 1);
    cpu->op_addr = (cpu_read(cpu, high_addr) << 8) | cpu_read(cpu, ptr);
    return 0;
}

uint8_t am_idx(_cpu* cpu) {
    uint16_t base = cpu_read(cpu, cpu->pc++);
    uint16_t low = cpu_read(cpu, (base + cpu->x) & 0x00FF);
    uint16_t high = cpu_read(cpu, (base + cpu->x + 1) & 0x00FF);
    cpu->op_addr = (high << 8) | low;
    return 0;
}

uint8_t am_idy(_cpu* cpu) {
    uint16_t base = cpu_read(cpu, cpu->pc++);
    uint16_t low = cpu_read(cpu, base & 0x00FF);
    uint16_t high = cpu_read(cpu, (base + 1) & 0x00FF);
    cpu->op_addr = ((high << 8) | low) + cpu->y;
    return (cpu->op_addr & 0xFF00) != (high << 8);
}

uint8_t am_rel(_cpu* cpu) {
    cpu->op_addr = cpu_read(cpu, cpu->pc++);
    if (cpu->op_addr & 0x80) cpu->op_addr |= 0xFF00;
    return 0;
}

/* Operations */

uint8_t op_adc(_cpu* cpu) {
    uint8_t memory = cpu_fetch(cpu);
    uint16_t res = (uint16_t)cpu->a + (uint16_t)memory + get_flag(cpu, CARRY);

    uint16_t overflow = (res ^ cpu->a) & (res ^ memory) & 0x80;
    set_flag(cpu, CARRY, res > 0xFF);
    set_flag(cpu, ZERO, (res & 0xFF) == 0x00);
    set_flag(cpu, OVERFLOW, overflow);
    set_flag(cpu, NEGATIVE, res & 0x80);

    cpu->a = res & 0xFF;
	return 1;
}

uint8_t op_ahx(_cpu* cpu) {
    uint8_t hi = (uint8_t)((cpu->op_addr >> 8) + 1);
    uint8_t val = cpu->a & cpu->x & hi;
    cpu_write(cpu, cpu->op_addr, val);
    return 0;
}

uint8_t op_alr(_cpu* cpu) {
    uint8_t memory = cpu_fetch(cpu);
    cpu->a &= memory;

    set_flag(cpu, CARRY, cpu->a & 0x01);
    cpu->a >>= 1;

    set_flag(cpu, ZERO, cpu->a == 0x00);
    set_flag(cpu, NEGATIVE, cpu->a & 0x80);
    return 0;
}

uint8_t op_anc(_cpu* cpu) {
    uint8_t memory = cpu_fetch(cpu);
    cpu->a &= memory;

    set_flag(cpu, ZERO, cpu->a == 0x00);
    set_flag(cpu, NEGATIVE, cpu->a & 0x80);
    set_flag(cpu, CARRY, cpu->a & 0x80);
    return 0;
}

uint8_t op_and(_cpu* cpu) {
    uint8_t memory = cpu_fetch(cpu);
    cpu->a &= memory;
    set_flag(cpu, ZERO, cpu->a == 0x00);
    set_flag(cpu, NEGATIVE, cpu->a & 0x80);
	return 1;
}

uint8_t op_arr(_cpu* cpu) {
    uint8_t memory = cpu_fetch(cpu);
    cpu->a &= memory;

    uint8_t old_c = get_flag(cpu, CARRY);
    cpu->a = (cpu->a >> 1) | (old_c << 7);

    set_flag(cpu, ZERO, cpu->a == 0x00);
    set_flag(cpu, NEGATIVE, cpu->a & 0x80);

    uint8_t bit6 = (cpu->a >> 6) & 1;
    uint8_t bit5 = (cpu->a >> 5) & 1;
    set_flag(cpu, CARRY, bit6);
    set_flag(cpu, OVERFLOW, bit6 ^ bit5);
    return 0;
}

uint8_t op_asl(_cpu* cpu) {
    uint8_t memory = cpu_fetch(cpu);
    uint16_t res = (uint16_t)memory << 1;
    set_flag(cpu, CARRY, res > 255);
    set_flag(cpu, ZERO, (res & 0xFF) == 0x00);
    set_flag(cpu, NEGATIVE, res & 0x80);
    cpu_write_back(cpu, res);
	return 0;
}

uint8_t op_axs(_cpu* cpu) {
    uint8_t memory = cpu_fetch(cpu);
    uint8_t ax = cpu->a & cpu->x;
    uint8_t res = ax - memory;

    set_flag(cpu, CARRY, ax >= memory);
    set_flag(cpu, ZERO, res == 0x00);
    set_flag(cpu, NEGATIVE, res & 0x80);

    cpu->x = res;
    return 0;
}

uint8_t op_bcc(_cpu* cpu) {
    if (!get_flag(cpu, CARRY))
        branch(cpu);

	return 0;
}

uint8_t op_bcs(_cpu* cpu) {
    if (get_flag(cpu, CARRY))
        branch(cpu);
	return 0;
}

uint8_t op_beq(_cpu* cpu) {
    if (get_flag(cpu, ZERO))
        branch(cpu);
    return 0;
}

uint8_t op_bit(_cpu* cpu) {
    uint8_t memory = cpu_fetch(cpu);
    uint8_t res = cpu->a & memory;
    set_flag(cpu, ZERO, res == 0x00);
    set_flag(cpu, OVERFLOW, memory & 0x40);
    set_flag(cpu, NEGATIVE, memory & 0x80);
	return 0;
}

uint8_t op_bmi(_cpu* cpu) {
    if (get_flag(cpu, NEGATIVE))
        branch(cpu);
	return 0;
}

uint8_t op_bne(_cpu* cpu) {
    if (!get_flag(cpu, ZERO))
        branch(cpu);
	return 0;
}

uint8_t op_bpl(_cpu* cpu) {
    if (!get_flag(cpu, NEGATIVE))
        branch(cpu);
	return 0;
}

uint8_t op_brk(_cpu* cpu) {
    cpu_nmi(cpu, 1);
    return 0;
}

uint8_t op_bvc(_cpu* cpu) {
    if (!get_flag(cpu, OVERFLOW))
        branch(cpu);
	return 0;
}

uint8_t op_bvs(_cpu* cpu) {
    if (get_flag(cpu, OVERFLOW))
        branch(cpu);
	return 0;
}

uint8_t op_clc(_cpu* cpu) {
    set_flag(cpu, CARRY, 0);
	return 0;
}

uint8_t op_cld(_cpu* cpu) {
    set_flag(cpu, DECIMAL, 0);
	return 0;
}

uint8_t op_cli(_cpu* cpu) {
    uint8_t old_irq_ds = get_flag(cpu, IRQ_DS);
    set_flag(cpu, IRQ_DS, 0);
    cpu->irq_delay = old_irq_ds && cpu->irq_pending;
    return 0;
}

uint8_t op_clv(_cpu* cpu) {
    set_flag(cpu, OVERFLOW, 0);
	return 0;
}

uint8_t op_cmp(_cpu* cpu) {
    uint8_t memory = cpu_fetch(cpu);
    set_flag(cpu, CARRY, cpu->a >= memory);
    set_flag(cpu, ZERO, cpu->a == memory);
    set_flag(cpu, NEGATIVE, (cpu->a - memory) & 0x80);
    return 1;
}

uint8_t op_cpx(_cpu* cpu) {
    uint8_t memory = cpu_fetch(cpu);
    set_flag(cpu, CARRY, cpu->x >= memory);
    set_flag(cpu, ZERO, cpu->x == memory);
    set_flag(cpu, NEGATIVE, (cpu->x - memory) & 0x80);
	return 0;
}

uint8_t op_cpy(_cpu* cpu) {
    uint8_t memory = cpu_fetch(cpu);
    set_flag(cpu, CARRY, cpu->y >= memory);
    set_flag(cpu, ZERO, cpu->y == memory);
    set_flag(cpu, NEGATIVE, (cpu->y - memory) & 0x80);
	return 0;
}

uint8_t op_dcp(_cpu* cpu) {
    uint8_t memory = cpu_fetch(cpu);
    memory--;

    cpu_write(cpu, cpu->op_addr, memory);

    set_flag(cpu, CARRY, cpu->a >= memory);
    set_flag(cpu, ZERO, cpu->a == memory);
    set_flag(cpu, NEGATIVE, (cpu->a - memory) & 0x80);
    return 0;
}

uint8_t op_dec(_cpu* cpu) {
    uint8_t memory = cpu_fetch(cpu) - 1;
    cpu_write(cpu, cpu->op_addr, memory);
    set_flag(cpu, ZERO, memory == 0x00);
    set_flag(cpu, NEGATIVE, memory & 0x80);
	return 0;
}

uint8_t op_dex(_cpu* cpu) {
    cpu->x--;
    set_flag(cpu, ZERO, cpu->x == 0x00);
    set_flag(cpu, NEGATIVE, cpu->x & 0x80);
	return 0;
}

uint8_t op_dey(_cpu* cpu) {
    cpu->y--;
    set_flag(cpu, ZERO, cpu->y == 0x00);
    set_flag(cpu, NEGATIVE, cpu->y & 0x80);
	return 0;
}

uint8_t op_eor(_cpu* cpu) {
    uint8_t memory = cpu_fetch(cpu);
    cpu->a ^= memory;
    set_flag(cpu, ZERO, cpu->a == 0x00);
    set_flag(cpu, NEGATIVE, cpu->a & 0x80);
	return 1;
}

uint8_t op_hlt(_cpu* cpu) {
    cpu->halt = 1;
    return 0;
}

uint8_t op_inc(_cpu* cpu) {
    uint8_t memory = cpu_fetch(cpu) + 1;
    cpu_write(cpu, cpu->op_addr, memory);
    set_flag(cpu, ZERO, memory == 0x00);
    set_flag(cpu, NEGATIVE, memory & 0x80);
	return 0;
}

uint8_t op_inx(_cpu* cpu) {
    cpu->x++;
    set_flag(cpu, ZERO, cpu->x == 0x00);
    set_flag(cpu, NEGATIVE, cpu->x & 0x80);
	return 0;
}

uint8_t op_iny(_cpu* cpu) {
    cpu->y++;
    set_flag(cpu, ZERO, cpu->y == 0x00);
    set_flag(cpu, NEGATIVE, cpu->y & 0x80);
	return 0;
}

uint8_t op_isc(_cpu* cpu) {
    uint8_t memory = cpu_fetch(cpu);
    memory++;

    cpu_write(cpu, cpu->op_addr, memory);

    uint16_t inv = (uint16_t)memory ^ 0xFF;
    uint16_t res = (uint16_t)cpu->a + inv + get_flag(cpu, CARRY);

    uint16_t overflow = (res ^ cpu->a) & (res ^ inv) & 0x80;

    set_flag(cpu, CARRY, res > 0xFF);
    set_flag(cpu, ZERO, (res & 0xFF) == 0x00);
    set_flag(cpu, OVERFLOW, overflow);
    set_flag(cpu, NEGATIVE, res & 0x80);

    cpu->a = (uint8_t)res;
    return 0;
}

uint8_t op_jmp(_cpu* cpu) {
    cpu->pc = cpu->op_addr;
	return 0;
}

uint8_t op_jsr(_cpu* cpu) {
    cpu->pc--;
    push(cpu, cpu->pc >> 8);
    push(cpu, cpu->pc & 0xFF);
    cpu->pc = cpu->op_addr;
	return 0;
}

uint8_t op_las(_cpu* cpu) {
    uint8_t memory = cpu_fetch(cpu);
    uint8_t res = memory & cpu->s;

    cpu->a = res;
    cpu->x = res;
    cpu->s = res;

    set_flag(cpu, ZERO, res == 0x00);
    set_flag(cpu, NEGATIVE, res & 0x80);
    return 1;
}

uint8_t op_lax(_cpu* cpu) {
    uint8_t memory = cpu_fetch(cpu);
    cpu->a = memory;
    cpu->x = memory;
    set_flag(cpu, ZERO, memory == 0x00);
    set_flag(cpu, NEGATIVE, memory & 0x80);
    return 1;
}

uint8_t op_lda(_cpu* cpu) {
    uint8_t memory = cpu_fetch(cpu);
    cpu->a = memory;
    set_flag(cpu, ZERO, memory == 0x00);
    set_flag(cpu, NEGATIVE, memory & 0x80);
	return 1;
}

uint8_t op_ldx(_cpu* cpu) {
    uint8_t memory = cpu_fetch(cpu);
    cpu->x = memory;
    set_flag(cpu, ZERO, memory == 0x00);
    set_flag(cpu, NEGATIVE, memory & 0x80);
	return 1;
}

uint8_t op_ldy(_cpu* cpu) {
    uint8_t memory = cpu_fetch(cpu);
    cpu->y = memory;
    set_flag(cpu, ZERO, memory == 0x00);
    set_flag(cpu, NEGATIVE, memory & 0x80);
	return 1;
}

uint8_t op_lsr(_cpu* cpu) {
    uint8_t memory = cpu_fetch(cpu);
    uint8_t res = memory >> 1;

    set_flag(cpu, CARRY, memory & 0x01);
    set_flag(cpu, ZERO, res == 0x00);
    set_flag(cpu, NEGATIVE, 0);

    cpu_write_back(cpu, res);
	return 0;
}

uint8_t op_lxa(_cpu* cpu) {
    uint8_t memory = cpu_fetch(cpu);
    cpu->a &= memory;
    cpu->x = cpu->a;

    set_flag(cpu, ZERO, cpu->a == 0x00);
    set_flag(cpu, NEGATIVE, cpu->a & 0x80);
    return 0;
}

uint8_t op_nop(_cpu* cpu) {
	return 0;
}

uint8_t op_ora(_cpu* cpu) {
    uint8_t memory = cpu_fetch(cpu);
    cpu->a |= memory;
    set_flag(cpu, ZERO, cpu->a == 0x00);
    set_flag(cpu, NEGATIVE, cpu->a & 0x80);
	return 1;
}

uint8_t op_pha(_cpu* cpu) {
    push(cpu, cpu->a);
	return 0;
}

uint8_t op_php(_cpu* cpu) {
    push(cpu, cpu->p | BREAK | UNUSED);
	return 0;
}

uint8_t op_pla(_cpu* cpu) {
    cpu->a = pull(cpu);
    set_flag(cpu, ZERO, cpu->a == 0x00);
    set_flag(cpu, NEGATIVE, cpu->a & 0x80);
	return 0;
}

uint8_t op_plp(_cpu* cpu) {
    cpu->p = pull(cpu) | UNUSED;
    return 0;
}

uint8_t op_rol(_cpu* cpu) {
    uint8_t memory = cpu_fetch(cpu);
    uint8_t res = (memory << 1) | get_flag(cpu, CARRY);

    set_flag(cpu, CARRY, memory & 0x80);
    set_flag(cpu, ZERO, res == 0x00);
    set_flag(cpu, NEGATIVE, res & 0x80);

    cpu_write_back(cpu, res);
	return 0;
}

uint8_t op_rla(_cpu* cpu) {
    uint8_t memory = cpu_fetch(cpu);
    uint8_t old_c = get_flag(cpu, CARRY);

    set_flag(cpu, CARRY, memory & 0x80);
    memory = (memory << 1) | old_c;

    cpu_write(cpu, cpu->op_addr, memory);

    cpu->a &= memory;
    set_flag(cpu, ZERO, cpu->a == 0x00);
    set_flag(cpu, NEGATIVE, cpu->a & 0x80);
    return 0;
}

uint8_t op_ror(_cpu* cpu) {
    uint8_t memory = cpu_fetch(cpu);
    uint8_t res = (memory >> 1) | (get_flag(cpu, CARRY) << 7);

    set_flag(cpu, CARRY, memory & 0x01);
    set_flag(cpu, ZERO, res == 0x00);
    set_flag(cpu, NEGATIVE, res & 0x80);

    cpu_write_back(cpu, res);
	return 0;
}

uint8_t op_rra(_cpu* cpu) {
    uint8_t memory = cpu_fetch(cpu);
    uint8_t old_c = get_flag(cpu, CARRY);

    uint8_t new_c = memory & 0x01;
    memory = (memory >> 1) | (old_c << 7);
    set_flag(cpu, CARRY, new_c);

    cpu_write(cpu, cpu->op_addr, memory);

    uint16_t res = (uint16_t)cpu->a + (uint16_t)memory + get_flag(cpu, CARRY);
    uint16_t overflow = (res ^ cpu->a) & (res ^ memory) & 0x80;

    set_flag(cpu, CARRY, res > 0xFF);
    set_flag(cpu, ZERO, (res & 0xFF) == 0x00);
    set_flag(cpu, OVERFLOW, overflow);
    set_flag(cpu, NEGATIVE, res & 0x80);

    cpu->a = (uint8_t)res;
    return 0;
}

uint8_t op_rti(_cpu* cpu) {
    cpu->p = pull(cpu) & ~BREAK & ~UNUSED;
    cpu->pc = pull(cpu);
    cpu->pc |= (uint16_t)pull(cpu) << 8;
	return 0;
}

uint8_t op_rts(_cpu* cpu) {
    cpu->pc = pull(cpu);
    cpu->pc |= (uint16_t)pull(cpu) << 8;
    cpu->pc++;
	return 0;
}

uint8_t op_sax(_cpu* cpu) {
    uint8_t data = cpu->a & cpu->x;
    cpu_write(cpu, cpu->op_addr, data);
    return 0;
}

uint8_t op_sbc(_cpu* cpu) {
    uint8_t memory = cpu_fetch(cpu);
    uint16_t value = (uint16_t)memory ^ 0xFF;
    uint16_t res = (uint16_t)cpu->a + value + get_flag(cpu, CARRY);

    uint16_t overflow = (res ^ cpu->a) & (res ^ value) & 0x80;
    set_flag(cpu, CARRY, res > 0xFF);
    set_flag(cpu, ZERO, (res & 0xFF) == 0x00);
    set_flag(cpu, OVERFLOW, overflow);
    set_flag(cpu, NEGATIVE, res & 0x80);

    cpu->a = res & 0xFF;
    return 1;
}

uint8_t op_sec(_cpu* cpu) {
    set_flag(cpu, CARRY, 1);
	return 0;
}

uint8_t op_sed(_cpu* cpu) {
    set_flag(cpu, DECIMAL, 1);
	return 0;
}

uint8_t op_sei(_cpu* cpu) {
    set_flag(cpu, IRQ_DS, 1);
	return 0;
}

uint8_t op_shx(_cpu* cpu) {
    uint8_t hi = (uint8_t)((cpu->op_addr >> 8) + 1);
    uint8_t val = cpu->x & hi;
    cpu_write(cpu, cpu->op_addr, val);
    return 0;
}

uint8_t op_shy(_cpu* cpu) {
    uint8_t hi = (uint8_t)((cpu->op_addr >> 8) + 1);
    uint8_t val = cpu->y & hi;
    cpu_write(cpu, cpu->op_addr, val);
    return 0;
}

uint8_t op_slo(_cpu* cpu) {
    uint8_t memory = cpu_fetch(cpu);

    uint16_t res = (uint16_t)memory << 1;
    set_flag(cpu, CARRY, res > 0xFF);
    memory = (uint8_t)res;

    cpu_write(cpu, cpu->op_addr, memory);

    cpu->a |= memory;
    set_flag(cpu, ZERO, cpu->a == 0x00);
    set_flag(cpu, NEGATIVE, cpu->a & 0x80);
    return 0;
}

uint8_t op_sre(_cpu* cpu) {
    uint8_t memory = cpu_fetch(cpu);
    set_flag(cpu, CARRY, memory & 0x01);
    memory >>= 1;

    cpu_write(cpu, cpu->op_addr, memory);
    cpu->a ^= memory;

    set_flag(cpu, ZERO, cpu->a == 0x00);
    set_flag(cpu, NEGATIVE, cpu->a & 0x80);
    return 0;
}

uint8_t op_sta(_cpu* cpu) {
    cpu_write(cpu, cpu->op_addr, cpu->a);
	return 0;
}

uint8_t op_stx(_cpu* cpu) {
    cpu_write(cpu, cpu->op_addr, cpu->x);
	return 0;
}

uint8_t op_sty(_cpu* cpu) {
    cpu_write(cpu, cpu->op_addr, cpu->y);
	return 0;
}

uint8_t op_tas(_cpu* cpu) {
    uint8_t tmp = cpu->a & cpu->x;
    cpu->s = tmp;

    uint8_t hi = (uint8_t)((cpu->op_addr >> 8) + 1);
    uint8_t val = tmp & hi;
    cpu_write(cpu, cpu->op_addr, val);
    return 0;
}

uint8_t op_tax(_cpu* cpu) {
    cpu->x = cpu->a;
    set_flag(cpu, ZERO, cpu->x == 0x00);
    set_flag(cpu, NEGATIVE, cpu->x & 0x80);
	return 0;
}

uint8_t op_tay(_cpu* cpu) {
    cpu->y = cpu->a;
    set_flag(cpu, ZERO, cpu->y == 0x00);
    set_flag(cpu, NEGATIVE, cpu->y & 0x80);
	return 0;
}

uint8_t op_tsx(_cpu* cpu) {
    cpu->x = cpu->s;
    set_flag(cpu, ZERO, cpu->x == 0x00);
    set_flag(cpu, NEGATIVE, cpu->x & 0x80);
	return 0;
}

uint8_t op_txa(_cpu* cpu) {
    cpu->a = cpu->x;
    set_flag(cpu, ZERO, cpu->a == 0x00);
    set_flag(cpu, NEGATIVE, cpu->a & 0x80);
	return 0;
}

uint8_t op_txs(_cpu* cpu) {
    cpu->s = cpu->x;
	return 0;
}

uint8_t op_tya(_cpu* cpu) {
    cpu->a = cpu->y;
    set_flag(cpu, ZERO, cpu->a == 0x00);
    set_flag(cpu, NEGATIVE, cpu->a & 0x80);
	return 0;
}

uint8_t op_xaa(_cpu* cpu) {
    uint8_t value = cpu_fetch(cpu);
    cpu->a = cpu->x & value;
    set_flag(cpu, ZERO, cpu->a == 0x00);
    set_flag(cpu, NEGATIVE, cpu->a & 0x80);
    return 0;
}



uint8_t* up_mnem(uint8_t* str) {
    unsigned long len = strlen((const char*)str);
    uint8_t* new = (uint8_t*)calloc(len + 1, sizeof(uint8_t));

    for (uint8_t i = 0; i < len; i++)
        new[i] = toupper(str[i]);

    return new;
}

void print_state(_cpu* cpu) {
    uint16_t start_pc = cpu->pc - cpu->instr.opcount - 1;
    printf("$%04X  ", start_pc);

    uint8_t ops[3];
    for (uint8_t am_op = 0; am_op < 3; am_op++) {
        if (am_op <= cpu->instr.opcount) {
            uint8_t op_data = cpu_read(cpu, start_pc + am_op);
            printf("%02X ", op_data);
            ops[am_op] = op_data;
        } else {
            printf("   ");
        }
    }

    uint8_t* am_name = up_mnem(cpu->instr.mode);
    printf(" (%s) ", am_name);
    free(am_name);

    uint8_t* op_name = up_mnem(cpu->instr.opcode);
    printf(" %s ", op_name);
    free(op_name);

    switch (cpu->instr.mode_num) {
        case _abs: printf("$%04X                      ", cpu->op_addr); break;
        case _abx: printf("$%02X%02X,X @ %04X             ", ops[2], ops[1], cpu->op_addr); break;
        case _aby: printf("$%02X%02X,Y @ %04X             ", ops[2], ops[1], cpu->op_addr); break;
        case _imm: printf("#$%02X                       ", ops[1]); break;
        case _zpg: printf("$%02X                        ", ops[1]); break;
        case _zpx: printf("$%02X,X @ %02X                 ", ops[1], cpu->op_addr); break;
        case _zpy: printf("$%02X,X @ %02X                 ", ops[1], cpu->op_addr); break;
        case _imp: printf("                           "); break;
        case _acc: printf("A                          "); break;
        case _idr: printf("($%02X%02X) = %04X             ", ops[2], ops[1], cpu->op_addr); break;
        case _idx: printf("($%02X,X) @ %02X = %04X        ", ops[1], (uint8_t)(ops[1] + cpu->x), cpu->op_addr); break;
        case _idy: printf("($%02X,Y) @ %02X = %04X        ", ops[1], (uint8_t)(ops[1] + cpu->y), cpu->op_addr); break;
        case _rel: printf("$%04X                      ", (uint16_t)(cpu->op_addr + cpu->pc)); break;
        default: break;
    }

    uint8_t stat_str[9];
    stat_str[0] = get_flag(cpu, NEGATIVE) ? 'N' : 'n';
    stat_str[1] = get_flag(cpu, OVERFLOW) ? 'V' : 'v';
    stat_str[2] = '-';
    stat_str[3] = get_flag(cpu, BREAK) ? 'B' : 'b';
    stat_str[4] = get_flag(cpu, DECIMAL) ? 'D' : 'd';
    stat_str[5] = get_flag(cpu, IRQ_DS) ? 'I' : 'i';
    stat_str[6] = get_flag(cpu, ZERO) ? 'Z' : 'z';
    stat_str[7] = get_flag(cpu, CARRY) ? 'C' : 'c';
    stat_str[8] = 0;

    printf("A:%02X X:%02X Y:%02X ST:%s SP:%02X PPU: %03d,%03d CY:%06llu",
        cpu->a, cpu->x, cpu->y, stat_str, cpu->s, cpu->p_ppu->cycle, cpu->p_ppu->scanline, (unsigned long long)cpu->total_cycles);
}
