#pragma once
#include <stdint.h>
#include <stddef.h>

#define NMI_VECTOR 0xFFFA
#define RST_VECTOR 0xFFFC
#define IRQ_VECTOR 0xFFFE

#define IN(opcode, mode, cycles, opcount) \
    {#opcode, #mode, _##mode, op_##opcode, am_##mode, cycles, opcount}

#define ILLEGAL_INSTRUC IN(___, ___, 0, 0)
#define OPF(opcode) op_##opcode(_cpu* cpu)
#define AMF(mode) am_##mode(_cpu* cpu)

typedef struct _cpu _cpu;
typedef struct _ppu _ppu;
typedef struct _cart _cart;

typedef struct _instr {
    uint8_t opcode[4];          // name of opcode
    uint8_t mode[4];            // name of addressing mode
    uint8_t mode_num;           // enum value of addressing mode
    uint8_t (*ex_op)(_cpu*);    // fn pointer to operation
    uint8_t (*ex_am)(_cpu*);    // fn pointer to addressing mode
    uint8_t cycles;             // number of cpu cycles for execution
    uint8_t opcount;            // number of operands after opcode
} _instr;

typedef struct _cpu {
    uint8_t a;              // accumulator
    uint8_t x;              // x register
    uint8_t y;              // y register
    uint8_t p;              // status flags
    uint8_t s;              // stack pointer
    uint16_t pc;            // program counter

    _instr instr;           // active instruction
    uint16_t op_addr;       // address of first operand
    uint8_t op_data;        // data buffer from address mode to operation

    uint8_t halt;           // halt execution
    uint8_t cycles;         // instr cycle counter
    size_t total_cycles;  // total cycle counter
    uint8_t ram[0x800];     // cpu memory

    _ppu* p_ppu;              // ref for ppu regs cpu-side
    _cart* p_cart;            // ref for cart mapper cpu-side
} _cpu;

typedef enum _cpu_flag {
    CARRY       = (1 << 0),
    ZERO        = (1 << 1),
    IRQ_DS      = (1 << 2),
    DECIMAL     = (1 << 3),
    BREAK       = (1 << 4),
    UNUSED      = (1 << 5),
    OVERFLOW    = (1 << 6),
    NEGATIVE    = (1 << 7),
} _cpu_flag;

typedef enum _addr_mode {
    _acc, _imp, _imm, _zpg, _zpx, _zpy, _abs, _abx,
    _aby, _idr, _idx, _idy, _rel, ____
} _addr_mode;

uint8_t OPF(adc), OPF(and), OPF(asl), OPF(bcc), OPF(bcs), OPF(beq), OPF(bit), OPF(bmi),
        OPF(bne), OPF(bpl), OPF(brk), OPF(bvc), OPF(bvs), OPF(clc), OPF(cld), OPF(cli),
        OPF(clv), OPF(cmp), OPF(cpx), OPF(cpy), OPF(dec), OPF(dex), OPF(dey), OPF(eor),
        OPF(inc), OPF(inx), OPF(iny), OPF(jmp), OPF(jsr), OPF(lda), OPF(ldx), OPF(ldy),
        OPF(lsr), OPF(nop), OPF(ora), OPF(pha), OPF(php), OPF(pla), OPF(plp), OPF(rol),
        OPF(ror), OPF(rti), OPF(rts), OPF(sbc), OPF(sec), OPF(sed), OPF(sei), OPF(sta),
        OPF(stx), OPF(sty), OPF(tax), OPF(tay), OPF(tsx), OPF(txa), OPF(txs), OPF(tya),
        OPF(___);

uint8_t AMF(acc), AMF(imp), AMF(imm), AMF(zpg), AMF(zpx), AMF(zpy), AMF(abs), AMF(abx),
        AMF(aby), AMF(idr), AMF(idx), AMF(idy), AMF(rel), AMF(___);

void cpu_clock(_cpu* cpu);
void cpu_reset(_cpu* cpu);
void cpu_irq(_cpu* cpu);
void cpu_nmi(_cpu* cpu, uint8_t brk);

uint8_t cpu_read(_cpu* cpu, uint16_t addr);
void cpu_write(_cpu* cpu, uint16_t addr, uint8_t data);

uint8_t no_fetch(_cpu* cpu);
uint8_t cpu_fetch(_cpu* cpu);
void cpu_write_back(_cpu* cpu, uint8_t result);

uint8_t get_flag(_cpu* cpu, _cpu_flag flag);
void set_flag(_cpu* cpu, _cpu_flag flag, uint8_t set);

void push(_cpu* cpu, uint8_t data);
uint8_t pull(_cpu* cpu);

void print_state(_cpu* cpu);

static _instr instructions[256] = {
    IN(brk,imp,7,0), IN(ora,idx,6,1), ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, IN(ora,zpg,3,1), IN(asl,zpg,5,1), ILLEGAL_INSTRUC,     // 0x00 - 0x07
    IN(php,imp,3,0), IN(ora,imm,2,1), IN(asl,acc,2,0), ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, IN(ora,abs,4,2), IN(asl,abs,6,2), ILLEGAL_INSTRUC,     // 0x08 - 0x0F
    IN(bpl,rel,2,1), IN(ora,idy,5,1), ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, IN(ora,zpx,4,1), IN(asl,zpx,6,1), ILLEGAL_INSTRUC,     // 0x10 - 0x17
    IN(clc,imp,2,0), IN(ora,aby,4,2), ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, IN(ora,abx,4,2), IN(asl,abx,7,2), ILLEGAL_INSTRUC,     // 0x18 - 0x1F
    IN(jsr,abs,6,2), IN(and,idx,6,1), ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, IN(bit,zpg,3,1), IN(and,zpg,3,1), IN(rol,zpg,5,1), ILLEGAL_INSTRUC,     // 0x20 - 0x27
    IN(plp,imp,4,0), IN(and,imm,2,1), IN(rol,acc,2,0), ILLEGAL_INSTRUC, IN(bit,abs,4,2), IN(and,abs,4,2), IN(rol,abs,6,2), ILLEGAL_INSTRUC,     // 0x28 - 0x2F
    IN(bmi,rel,2,1), IN(and,idy,5,1), ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, IN(and,zpx,4,1), IN(rol,zpx,6,1), ILLEGAL_INSTRUC,     // 0x30 - 0x37
    IN(sec,imp,2,0), IN(and,aby,4,2), ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, IN(and,abx,4,2), IN(rol,abx,7,2), ILLEGAL_INSTRUC,     // 0x38 - 0x3F
    IN(rti,imp,6,0), IN(eor,idx,6,1), ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, IN(eor,zpg,3,1), IN(lsr,zpg,5,1), ILLEGAL_INSTRUC,     // 0x40 - 0x47
    IN(pha,imp,3,0), IN(eor,imm,2,1), IN(lsr,acc,2,0), ILLEGAL_INSTRUC, IN(jmp,abs,3,2), IN(eor,abs,4,2), IN(lsr,abs,6,2), ILLEGAL_INSTRUC,     // 0x48 - 0x4F
    IN(bvc,rel,2,1), IN(eor,idy,5,1), ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, IN(eor,zpx,4,1), IN(lsr,zpx,6,1), ILLEGAL_INSTRUC,     // 0x50 - 0x57
    IN(cli,imp,2,0), IN(eor,aby,4,2), ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, IN(eor,abx,4,2), IN(lsr,abx,7,2), ILLEGAL_INSTRUC,     // 0x58 - 0x5F
    IN(rts,imp,6,0), IN(adc,idx,6,1), ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, IN(adc,zpg,3,1), IN(ror,zpg,5,1), ILLEGAL_INSTRUC,     // 0x60 - 0x67
    IN(pla,imp,4,0), IN(adc,imm,2,1), IN(ror,acc,2,0), ILLEGAL_INSTRUC, IN(jmp,idr,5,2), IN(adc,abs,4,2), IN(ror,abs,6,2), ILLEGAL_INSTRUC,     // 0x68 - 0x6F
    IN(bvs,rel,2,1), IN(adc,idy,5,1), ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, IN(adc,zpx,4,1), IN(ror,zpx,6,1), ILLEGAL_INSTRUC,     // 0x70 - 0x77
    IN(sei,imp,2,0), IN(adc,aby,4,2), ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, IN(adc,abx,4,2), IN(ror,abx,7,2), ILLEGAL_INSTRUC,     // 0x78 - 0x7F
    ILLEGAL_INSTRUC, IN(sta,idx,6,1), ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, IN(sty,zpg,3,1), IN(sta,zpg,3,1), IN(stx,zpg,3,1), ILLEGAL_INSTRUC,     // 0x80 - 0x87
    IN(dey,imp,2,0), ILLEGAL_INSTRUC, IN(txa,imp,2,0), ILLEGAL_INSTRUC, IN(sty,abs,4,2), IN(sta,abs,4,2), IN(stx,abs,4,2), ILLEGAL_INSTRUC,     // 0x88 - 0x8F
    IN(bcc,rel,2,1), IN(sta,idy,6,1), ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, IN(sty,zpx,4,1), IN(sta,zpx,4,1), IN(stx,zpy,4,1), ILLEGAL_INSTRUC,     // 0x90 - 0x97
    IN(tya,imp,2,0), IN(sta,aby,5,2), IN(txs,imp,2,0), ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, IN(sta,abx,5,2), ILLEGAL_INSTRUC, ILLEGAL_INSTRUC,     // 0x98 - 0x9F
    IN(ldy,imm,2,1), IN(lda,idx,6,1), IN(ldx,imm,2,1), ILLEGAL_INSTRUC, IN(ldy,zpg,3,1), IN(lda,zpg,3,1), IN(ldx,zpg,3,1), ILLEGAL_INSTRUC,     // 0xA0 - 0xA7
    IN(tay,imp,2,0), IN(lda,imm,2,1), IN(tax,imp,2,0), ILLEGAL_INSTRUC, IN(ldy,abs,4,2), IN(lda,abs,4,2), IN(ldx,abs,4,2), ILLEGAL_INSTRUC,     // 0xA8 - 0xAF
    IN(bcs,rel,2,1), IN(lda,idy,5,1), ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, IN(ldy,zpx,4,1), IN(lda,zpx,4,1), IN(ldx,zpy,4,1), ILLEGAL_INSTRUC,     // 0xB0 - 0xB7
    IN(clv,imp,2,0), IN(lda,aby,4,2), IN(tsx,imp,2,0), ILLEGAL_INSTRUC, IN(ldy,abx,4,2), IN(lda,abx,4,2), IN(ldx,aby,4,2), ILLEGAL_INSTRUC,     // 0xB8 - 0xBF
    IN(cpy,imm,2,1), IN(cmp,idx,6,1), ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, IN(cpy,zpg,3,1), IN(cmp,zpg,3,1), IN(dec,zpg,5,1), ILLEGAL_INSTRUC,     // 0xC0 - 0xC7
    IN(iny,imp,2,0), IN(cmp,imm,2,1), IN(dex,imp,2,0), ILLEGAL_INSTRUC, IN(cpy,abs,4,2), IN(cmp,abs,4,2), IN(dec,abs,6,2), ILLEGAL_INSTRUC,     // 0xC8 - 0xCF
    IN(bne,rel,2,1), IN(cmp,idy,5,1), ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, IN(cmp,zpx,4,1), IN(dec,zpx,6,1), ILLEGAL_INSTRUC,     // 0xD0 - 0xD7
    IN(cld,imp,2,0), IN(cmp,aby,4,2), ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, IN(cmp,abx,4,2), IN(dec,abx,7,2), ILLEGAL_INSTRUC,     // 0xD8 - 0xDF
    IN(cpx,imm,2,1), IN(sbc,idx,6,1), ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, IN(cpx,zpg,3,1), IN(sbc,zpg,3,1), IN(inc,zpg,5,1), ILLEGAL_INSTRUC,     // 0xE0 - 0xE7
    IN(inx,imp,2,0), IN(sbc,imm,2,1), IN(nop,imp,2,0), ILLEGAL_INSTRUC, IN(cpx,abs,4,2), IN(sbc,abs,4,2), IN(inc,abs,6,2), ILLEGAL_INSTRUC,     // 0xE8 - 0xEF
    IN(beq,rel,2,1), IN(sbc,idy,5,1), ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, IN(sbc,zpx,4,1), IN(inc,zpx,6,1), ILLEGAL_INSTRUC,     // 0xF0 - 0xF7
    IN(sed,imp,2,0), IN(sbc,aby,4,2), ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, ILLEGAL_INSTRUC, IN(sbc,abx,4,2), IN(inc,abx,7,2), ILLEGAL_INSTRUC,     // 0xF8 - 0xFF
};
