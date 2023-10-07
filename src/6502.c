/*
	This file is part of the CMOS6502 project.

	BSD 3-Clause License

	Copyright (c) 2023, Pietro Senesi
	All rights reserved.
*/



#include "6502.h"



#define BIT_O(bit)				(1 << (bit))

#define AM_IMP					0
#define AM_IMM					1
#define AM_ZP0					2
#define AM_ZPX					3
#define AM_ZPY					4
#define AM_ABS					5
#define AM_ABX					6
#define AM_ABY					7
#define AM_IND					8
#define AM_INX					9
#define AM_INY					10

//#define FETCH					data = _6502_read(addr)

//registers (extern)
uint16_t _PC;
uint8_t _A, _X, _Y, _SP, _IR; //'ir' could actually be local
cpu_s_t _P;

//local variables
uint16_t addr;
uint8_t data, fetch;



static uint16_t get_w(uint16_t a) {
	return _6502_read(a) | (_6502_read(a+1) << 8);
}

static void pushc(uint8_t x) {
	_6502_write(__6502_STACK_BOTTOM + (_SP--), x);
}

static uint8_t pullc() {
	return _6502_read(__6502_STACK_BOTTOM + (++_SP));
}

static void pushpc() {
	pushc(_PC >> 8);
	pushc(_PC);
}

static uint16_t pullpc() {
	return pullc() | (pullc() << 8);
}

static void interr(uint16_t vct, uint8_t st) {
	pushpc();
	pushc(st);

	_P.flags.i = 1;
	_PC = get_w(vct);
}



static void w2a(uint8_t x) {_A = x;}
static void w2b(uint8_t x) {_6502_write(addr, x);}
void (*busora[2])(uint8_t x) = {w2a, w2b}; //bypassing some branches by using this evil jump table



//addressing mode functions (relative is equal to immediate (we differentiate in the operative functions))
static void A_imp() {
	data = _A;
}

static void A_imm() {
	addr = _PC++;
	//data = _6502_read((addr = _PC++));
	//addr = _PC++;
	//FETCH;
}

static void A_zp0() {
	addr = _6502_read(_PC++);
	//data = _6502_read((addr = _6502_read(_PC++)));
	//addr = _6502_read(_PC++);
	//FETCH;
}

static void A_zpx() {
	addr = (_6502_read(_PC++) + _X) & 0xff;
	//data = _6502_read((addr = (_6502_read(_PC++) + _X) & 0xff));
	//addr = (_6502_read(_PC++) + _X) & 0xff;
	//FETCH;
}

static void A_zpy() {
	addr = (_6502_read(_PC++) + _Y) & 0xff;
	//data = _6502_read((addr = (_6502_read(_PC++) + _Y) & 0xff));
	//addr = (_6502_read(_PC++) + _Y) & 0xff;
	//FETCH;
}

static void A_abs() {
	addr = get_w(_PC++);
	//data = _6502_read((addr = get_w(_PC++))); //basically we read from the ppu even if we don't, because this function will serve the store istrunctions, and it doesn't want it. You could add a flag in the instruction struct, which determines where to fetch from address or from instruction.
	//addr = get_w(_PC++);
	_PC++;
	//FETCH;
}

static void A_abx() {
	addr = get_w(_PC++) + _X;
	//data = _6502_read((addr = get_w(_PC++) + _X));
	//addr = get_w(_PC++) + _X;
	_PC++;
	//FETCH;
}

static void A_aby() {
	addr = get_w(_PC++) + _Y;
	//data = _6502_read((addr = get_w(_PC++) + _Y));
	//addr = get_w(_PC++) + _Y;
	_PC++;
	//FETCH;
}

static void A_ind() {
	addr = get_w(_PC++); //reusing
	uint8_t x = ((addr & 0xff) == 0xff) - 1;
	_PC++;

	addr = _6502_read(addr) | (_6502_read((addr & 0xff00 & ~x) | ((addr+1) & x)) << 8);
	//data = _6502_read((addr = _6502_read(addr) | (_6502_read((addr & 0xff00 & ~data) | ((addr+1) & data)) << 8)));
}

static void A_inx() {
	addr = get_w((_6502_read(_PC++) + _X) & 0xff);
	//data = _6502_read((addr = get_w((_6502_read(_PC++) + _X) & 0xff)));
	//addr = get_w((_6502_read(_PC++) + _X) & 0xff);
	//FETCH;
}

static void A_iny() {
	addr = get_w(_6502_read(_PC++)) + _Y;
	//data = _6502_read((addr = get_w(_6502_read(_PC++)) + _Y));
	//addr = get_w(_6502_read(_PC++)) + _Y;
	//FETCH;
}

void (*A_funcs[])() = { //addressing modes jump-table
	A_imp,
	A_imm,
	A_zp0,
	A_zpx,
	A_zpy,
	A_abs,
	A_abx,
	A_aby,
	A_ind,
	A_inx,
	A_iny
};



//operative functions
//load/store operations
static void I_lda() {
	//FETCH;
	_A = data;

	_P.flags.z = !_A;
	_P.flags.n = (_A & BIT_O(7)) > 0;
}

static void I_ldx() {
	//FETCH;
	_X = data;

	_P.flags.z = !_X;
	_P.flags.n = (_X & BIT_O(7)) > 0;
}

static void I_ldy() {
	//FETCH;
	_Y = data;

	_P.flags.z = !_Y;
	_P.flags.n = (_Y & BIT_O(7)) > 0;
}

static void I_sta() {
	_6502_write(addr, _A);
}

static void I_stx() {
	_6502_write(addr, _X);
}

static void I_sty() {
	_6502_write(addr, _Y);
}



//register transfers operations
static void I_tax() {
	_X = _A;

	_P.flags.z = !_X;
	_P.flags.n = (_X & BIT_O(7)) > 0;
}

static void I_tay() {
	_Y = _A;

	_P.flags.z = !_Y;
	_P.flags.n = (_Y & BIT_O(7)) > 0;
}

static void I_txa() {
	_A = _X;

	_P.flags.z = !_A;
	_P.flags.n = (_A & BIT_O(7)) > 0;
}

static void I_tya() {
	_A = _Y;

	_P.flags.z = !_A;
	_P.flags.n = (_A & BIT_O(7)) > 0;
}

static void I_tsx() {
	_X = _SP;

	_P.flags.z = !_X;
	_P.flags.n = (_X & BIT_O(7)) > 0;
}

static void I_txs() {
	_SP = _X;
}



//stack pushing/pulling operations
static void I_pha() {
	pushc(_A);
}

static void I_php() {
	pushc(_P._raw);
}

static void I_pla() {
	_A = pullc();

	_P.flags.z = !_A;
	_P.flags.n = (_A & BIT_O(7)) > 0;
}

static void I_plp() {
	_P._raw = pullc() | 0x30;
}



//logical operations
static void I_and() {
	//FETCH;
	_A &= data;

	_P.flags.z = !_A;
	_P.flags.n = (_A & BIT_O(7)) > 0;
}

static void I_ora() {
	//FETCH;
	_A |= data;

	_P.flags.z = !_A;
	_P.flags.n = (_A & BIT_O(7)) > 0;
}

static void I_eor() {
	//FETCH;
	_A ^= data;

	_P.flags.z = !_A;
	_P.flags.n = (_A & BIT_O(7)) > 0;
}

static void I_bit() {
	//FETCH;

	_P.flags.z = !(_A & data & 0xff);
	_P.flags.v = (data & BIT_O(6)) > 0;
	_P.flags.n = (data & BIT_O(7)) > 0;
}



//arithmetic operations
static void I_adc() {
	//FETCH;

	addr = _A + data + _P.flags.c;

	_P.flags.c = addr > 0xff;
	addr &= 0xff;

	_P.flags.z = !addr;
	_P.flags.v = ((_A ^ addr) & (data ^ addr) & BIT_O(7)) > 0;
	_P.flags.n = (addr & BIT_O(7)) > 0;

	_A = addr;
}

static void I_sbc() {
	//FETCH;
	data = ~data;
	addr = _A + data + _P.flags.c;

	_P.flags.c = addr > 0xff;
	addr &= 0xff;

	_P.flags.z = !addr;
	_P.flags.v = ((_A ^ addr) & (data ^ addr) & BIT_O(7)) > 0;
	_P.flags.n = (addr & BIT_O(7)) > 0;

	_A = addr;
}

static void I_cmp() {
	//FETCH;
	_P.flags.c = _A >= data;

	data = _A - data;

	_P.flags.z = !data;
	_P.flags.n = (data & BIT_O(7)) > 0;
}

static void I_cpx() {
	//FETCH;
	_P.flags.c = _X >= data;

	data = _X - data;

	_P.flags.z = !data;
	_P.flags.n = (data & BIT_O(7)) > 0;
}

static void I_cpy() {
	//FETCH;
	_P.flags.c = _Y >= data;

	data = _Y - data;

	_P.flags.z = !data;
	_P.flags.n = (data & BIT_O(7)) > 0;
}



//increment/decrement
static void I_inc() {
	//FETCH;
	_6502_write(addr, ++data);

	_P.flags.z = !data;
	_P.flags.n = (data & BIT_O(7)) > 0;
}

static void I_inx() {
	_P.flags.z = !(++_X);
	_P.flags.n = (_X & BIT_O(7)) > 0;
}

static void I_iny() {
	_P.flags.z = !(++_Y);
	_P.flags.n = (_Y & BIT_O(7)) > 0;
}

static void I_dec() {
	//FETCH;
	_6502_write(addr, --data);

	_P.flags.z = !data;
	_P.flags.n = (data & BIT_O(7)) > 0;
}

static void I_dex() {
	_P.flags.z = !(--_X);
	_P.flags.n = (_X & BIT_O(7)) > 0;
}

static void I_dey() {
	_P.flags.z = !(--_Y);
	_P.flags.n = (_Y & BIT_O(7)) > 0;
}



//shift operations
static void I_asl() {
	////FETCH;
	_P.flags.c = (data & BIT_O(7)) > 0;
	_P.flags.n = (data & BIT_O(6)) > 0;

	//SDL_Log("%d\n", amode);
	data <<= 1;
	_P.flags.z = !data;

	busora[fetch](data);
}

static void I_lsr() {
	//FETCH;
	_P.flags.c = data & 0x1;

	data >>= 1;

	_P.flags.z = !data;
	_P.flags.n = 0;//(data & BIT_O(7)) > 0; always equal to 0

	busora[fetch](data);
}

static void I_rol() {
	//FETCH;

	_P.flags.u = _P.flags.c; //using unused flag only for internal scopes. Gets resetted back once finished
	_P.flags.c = (data & BIT_O(7)) > 0;

	data = (data << 1) | _P.flags.u;
	_P.flags.u = 1;

	_P.flags.z = !data;
	_P.flags.n = (data & BIT_O(7)) > 0;

	busora[fetch](data);
}

static void I_ror() {
	//FETCH;

	_P.flags.u = _P.flags.c; //using unused flag only for internal scopes. Gets resetted back once finished
	_P.flags.c = data & 0x1;

	data = (data >> 1) | (_P.flags.u << 7);
	_P.flags.u = 1;

	_P.flags.z = !data;
	_P.flags.n = (data & BIT_O(7)) > 0;

	busora[fetch](data);
}



//jumps & calls
static void I_jmp() {
	_PC = addr;
}

static void I_jsr() {
	_PC--;
	pushpc();
	_PC = addr;
}

static void I_rts() {
	_PC = pullpc() + 1;
}



//branch operations
#define BR(C)				addr + 1 + ((char) data & (C-1))

//note that the condition given to the macro must be the condition for the negative case.
static void I_bcc() {
	//FETCH;
	_PC = BR(_P.flags.c); //for example, here we won't branch only if carry is set.
}

static void I_bcs() {
	//FETCH;
	_PC = BR(!_P.flags.c);
}

static void I_bne() {
	//FETCH;
	_PC = BR(_P.flags.z);
}

static void I_beq() {
	//FETCH;
	//SDL_Log("%04x\n", _PC);
	_PC = BR(!_P.flags.z);
	//SDL_Log("%04x\n", _PC);
}

static void I_bpl() {
	//FETCH;
	_PC = BR(_P.flags.n);
}

static void I_bmi() {
	//FETCH;
	_PC = BR(!_P.flags.n);
}

static void I_bvc() {
	//FETCH;
	_PC = BR(_P.flags.v);
}

static void I_bvs() {
	//FETCH;
	_PC = BR(!_P.flags.v);
}

#undef BR



//status clear/set operations
static void I_clc() {
	_P.flags.c = 0;
}

static void I_cld() {
	_P.flags.d = 0;
}

static void I_cli() {
	_P.flags.i = 0;
}

static void I_clv() {
	_P.flags.v = 0;
}

static void I_sec() {
	_P.flags.c = 1;
}

static void I_sed() {
	_P.flags.d = 1;
}

static void I_sei() {
	_P.flags.i = 1;
}



//system operations
static void I_brk() {
	interr(__6502_BRK_V, _P._raw | 0x30);
}

static void I_nop() {}

static void I_rti() {
	_P._raw = pullc() | 0x30;
	_PC = pullpc();
}

static void I_xxx() {} //illegal opcodes end up here



struct instr {
	void (*I_func)();
	uint8_t A_func_i : 4;
	uint8_t fetch : 1; //1 = we want to fetch the data from the prepared address, after the addressing-mode function, 0 = we don't
};

struct instr i_jtable[256] = {
	{I_brk, AM_IMM, 1}, {I_ora, AM_INX, 1}, {I_xxx, AM_IMP, 0}, {I_xxx, AM_IMP, 0}, {I_nop, AM_IMP, 0}, {I_ora, AM_ZP0, 1}, {I_asl, AM_ZP0, 1}, {I_xxx, AM_IMP, 0}, {I_php, AM_IMP, 0}, {I_ora, AM_IMM, 1}, {I_asl, AM_IMP, 0}, {I_xxx, AM_IMP, 0}, {I_nop, AM_IMP, 0}, {I_ora, AM_ABS, 1}, {I_asl, AM_ABS, 1}, {I_xxx, AM_IMP, 0},
	{I_bpl, AM_IMM, 1}, {I_ora, AM_INY, 1}, {I_xxx, AM_IMP, 0}, {I_xxx, AM_IMP, 0}, {I_nop, AM_IMP, 0}, {I_ora, AM_ZPX, 1}, {I_asl, AM_ZPX, 1}, {I_xxx, AM_IMP, 0}, {I_clc, AM_IMP, 0}, {I_ora, AM_ABY, 1}, {I_nop, AM_IMP, 0}, {I_xxx, AM_IMP, 0}, {I_nop, AM_IMP, 0}, {I_ora, AM_ABX, 1}, {I_asl, AM_ABX, 1}, {I_xxx, AM_IMP, 0},
	{I_jsr, AM_ABS, 1}, {I_and, AM_INX, 1}, {I_xxx, AM_IMP, 0}, {I_xxx, AM_IMP, 0}, {I_bit, AM_ZP0, 1}, {I_and, AM_ZP0, 1}, {I_rol, AM_ZP0, 1}, {I_xxx, AM_IMP, 0}, {I_plp, AM_IMP, 0}, {I_and, AM_IMM, 1}, {I_rol, AM_IMP, 0}, {I_xxx, AM_IMP, 0}, {I_bit, AM_ABS, 1}, {I_and, AM_ABS, 1}, {I_rol, AM_ABS, 1}, {I_xxx, AM_IMP, 0},
	{I_bmi, AM_IMM, 1}, {I_and, AM_INY, 1}, {I_xxx, AM_IMP, 0}, {I_xxx, AM_IMP, 0}, {I_nop, AM_IMP, 0}, {I_and, AM_ZPX, 1}, {I_rol, AM_ZPX, 1}, {I_xxx, AM_IMP, 0}, {I_sec, AM_IMP, 0}, {I_and, AM_ABY, 1}, {I_nop, AM_IMP, 0}, {I_xxx, AM_IMP, 0}, {I_nop, AM_IMP, 0}, {I_and, AM_ABX, 1}, {I_rol, AM_ABX, 1}, {I_xxx, AM_IMP, 0},
	{I_rti, AM_IMP, 0}, {I_eor, AM_INX, 1}, {I_xxx, AM_IMP, 0}, {I_xxx, AM_IMP, 0}, {I_nop, AM_IMP, 0}, {I_eor, AM_ZP0, 1}, {I_lsr, AM_ZP0, 1}, {I_xxx, AM_IMP, 0}, {I_pha, AM_IMP, 0}, {I_eor, AM_IMM, 1}, {I_lsr, AM_IMP, 0}, {I_xxx, AM_IMP, 0}, {I_jmp, AM_ABS, 1}, {I_eor, AM_ABS, 1}, {I_lsr, AM_ABS, 1}, {I_xxx, AM_IMP, 0},
	{I_bvc, AM_IMM, 1}, {I_eor, AM_INY, 1}, {I_xxx, AM_IMP, 0}, {I_xxx, AM_IMP, 0}, {I_nop, AM_IMP, 0}, {I_eor, AM_ZPX, 1}, {I_lsr, AM_ZPX, 1}, {I_xxx, AM_IMP, 0}, {I_cli, AM_IMP, 0}, {I_eor, AM_ABY, 1}, {I_nop, AM_IMP, 0}, {I_xxx, AM_IMP, 0}, {I_nop, AM_IMP, 0}, {I_eor, AM_ABX, 1}, {I_lsr, AM_ABX, 1}, {I_xxx, AM_IMP, 0},
	{I_rts, AM_IMP, 0}, {I_adc, AM_INX, 1}, {I_xxx, AM_IMP, 0}, {I_xxx, AM_IMP, 0}, {I_nop, AM_IMP, 0}, {I_adc, AM_ZP0, 1}, {I_ror, AM_ZP0, 1}, {I_xxx, AM_IMP, 0}, {I_pla, AM_IMP, 0}, {I_adc, AM_IMM, 1}, {I_ror, AM_IMP, 0}, {I_xxx, AM_IMP, 0}, {I_jmp, AM_IND, 1}, {I_adc, AM_ABS, 1}, {I_ror, AM_ABS, 1}, {I_xxx, AM_IMP, 0},
	{I_bvs, AM_IMM, 1}, {I_adc, AM_INY, 1}, {I_xxx, AM_IMP, 0}, {I_xxx, AM_IMP, 0}, {I_nop, AM_IMP, 0}, {I_adc, AM_ZPX, 1}, {I_ror, AM_ZPX, 1}, {I_xxx, AM_IMP, 0}, {I_sei, AM_IMP, 0}, {I_adc, AM_ABY, 1}, {I_nop, AM_IMP, 0}, {I_xxx, AM_IMP, 0}, {I_nop, AM_IMP, 0}, {I_adc, AM_ABX, 1}, {I_ror, AM_ABX, 1}, {I_xxx, AM_IMP, 0},
	{I_nop, AM_IMP, 0}, {I_sta, AM_INX, 0}, {I_nop, AM_IMP, 0}, {I_xxx, AM_IMP, 0}, {I_sty, AM_ZP0, 1}, {I_sta, AM_ZP0, 0}, {I_stx, AM_ZP0, 1}, {I_xxx, AM_IMP, 0}, {I_dey, AM_IMP, 0}, {I_nop, AM_IMP, 0}, {I_txa, AM_IMP, 0}, {I_xxx, AM_IMP, 0}, {I_sty, AM_ABS, 1}, {I_sta, AM_ABS, 0}, {I_stx, AM_ABS, 1}, {I_xxx, AM_IMP, 0},
	{I_bcc, AM_IMM, 1}, {I_sta, AM_INY, 0}, {I_xxx, AM_IMP, 0}, {I_xxx, AM_IMP, 0}, {I_sty, AM_ZPX, 1}, {I_sta, AM_ZPX, 0}, {I_stx, AM_ZPY, 1}, {I_xxx, AM_IMP, 0}, {I_tya, AM_IMP, 0}, {I_sta, AM_ABY, 0}, {I_txs, AM_IMP, 0}, {I_xxx, AM_IMP, 0}, {I_nop, AM_IMP, 0}, {I_sta, AM_ABX, 0}, {I_xxx, AM_IMP, 0}, {I_xxx, AM_IMP, 0},
	{I_ldy, AM_IMM, 1}, {I_lda, AM_INX, 1}, {I_ldx, AM_IMM, 1}, {I_xxx, AM_IMP, 0}, {I_ldy, AM_ZP0, 1}, {I_lda, AM_ZP0, 1}, {I_ldx, AM_ZP0, 1}, {I_xxx, AM_IMP, 0}, {I_tay, AM_IMP, 0}, {I_lda, AM_IMM, 1}, {I_tax, AM_IMP, 0}, {I_xxx, AM_IMP, 0}, {I_ldy, AM_ABS, 1}, {I_lda, AM_ABS, 1}, {I_ldx, AM_ABS, 1}, {I_xxx, AM_IMP, 0},
	{I_bcs, AM_IMM, 1}, {I_lda, AM_INY, 1}, {I_xxx, AM_IMP, 0}, {I_xxx, AM_IMP, 0}, {I_ldy, AM_ZPX, 1}, {I_lda, AM_ZPX, 1}, {I_ldx, AM_ZPY, 1}, {I_xxx, AM_IMP, 0}, {I_clv, AM_IMP, 0}, {I_lda, AM_ABY, 1}, {I_tsx, AM_IMP, 0}, {I_xxx, AM_IMP, 0}, {I_ldy, AM_ABX, 1}, {I_lda, AM_ABX, 1}, {I_ldx, AM_ABY, 1}, {I_xxx, AM_IMP, 0},
	{I_cpy, AM_IMM, 1}, {I_cmp, AM_INX, 1}, {I_nop, AM_IMP, 0}, {I_xxx, AM_IMP, 0}, {I_cpy, AM_ZP0, 1}, {I_cmp, AM_ZP0, 1}, {I_dec, AM_ZP0, 1}, {I_xxx, AM_IMP, 0}, {I_iny, AM_IMP, 0}, {I_cmp, AM_IMM, 1}, {I_dex, AM_IMP, 0}, {I_xxx, AM_IMP, 0}, {I_cpy, AM_ABS, 1}, {I_cmp, AM_ABS, 1}, {I_dec, AM_ABS, 1}, {I_xxx, AM_IMP, 0},
	{I_bne, AM_IMM, 1}, {I_cmp, AM_INY, 1}, {I_xxx, AM_IMP, 0}, {I_xxx, AM_IMP, 0}, {I_nop, AM_IMP, 0}, {I_cmp, AM_ZPX, 1}, {I_dec, AM_ZPX, 1}, {I_xxx, AM_IMP, 0}, {I_cld, AM_IMP, 0}, {I_cmp, AM_ABY, 1}, {I_nop, AM_IMP, 0}, {I_xxx, AM_IMP, 0}, {I_nop, AM_IMP, 0}, {I_cmp, AM_ABX, 1}, {I_dec, AM_ABX, 1}, {I_xxx, AM_IMP, 0},
	{I_cpx, AM_IMM, 1}, {I_sbc, AM_INX, 1}, {I_nop, AM_IMP, 0}, {I_xxx, AM_IMP, 0}, {I_cpx, AM_ZP0, 1}, {I_sbc, AM_ZP0, 1}, {I_inc, AM_ZP0, 1}, {I_xxx, AM_IMP, 0}, {I_inx, AM_IMP, 0}, {I_sbc, AM_IMM, 1}, {I_nop, AM_IMP, 0}, {I_sbc, AM_IMP, 0}, {I_cpx, AM_ABS, 1}, {I_sbc, AM_ABS, 1}, {I_inc, AM_ABS, 1}, {I_xxx, AM_IMP, 0},
	{I_beq, AM_IMM, 1}, {I_sbc, AM_INY, 1}, {I_xxx, AM_IMP, 0}, {I_xxx, AM_IMP, 0}, {I_nop, AM_IMP, 0}, {I_sbc, AM_ZPX, 1}, {I_inc, AM_ZPX, 1}, {I_xxx, AM_IMP, 0}, {I_sed, AM_IMP, 0}, {I_sbc, AM_ABY, 1}, {I_nop, AM_IMP, 0}, {I_xxx, AM_IMP, 0}, {I_nop, AM_IMP, 0}, {I_sbc, AM_ABX, 1}, {I_inc, AM_ABX, 1}, {I_xxx, AM_IMP, 0}
};

/*struct instr i_jtable[] = {
	{I_brk, AM_IMM}, {I_ora, AM_INX}, {I_xxx, AM_IMP}, {I_xxx, AM_IMP}, {I_nop, AM_IMP}, {I_ora, AM_ZP0}, {I_asl, AM_ZP0}, {I_xxx, AM_IMP}, {I_php, AM_IMP}, {I_ora, AM_IMM}, {I_asl, AM_IMP}, {I_xxx, AM_IMP}, {I_nop, AM_IMP}, {I_ora, AM_ABS}, {I_asl, AM_ABS}, {I_xxx, AM_IMP},
	{I_bpl, AM_IMM}, {I_ora, AM_INY}, {I_xxx, AM_IMP}, {I_xxx, AM_IMP}, {I_nop, AM_IMP}, {I_ora, AM_ZPX}, {I_asl, AM_ZPX}, {I_xxx, AM_IMP}, {I_clc, AM_IMP}, {I_ora, AM_ABY}, {I_nop, AM_IMP}, {I_xxx, AM_IMP}, {I_nop, AM_IMP}, {I_ora, AM_ABX}, {I_asl, AM_ABX}, {I_xxx, AM_IMP},
	{I_jsr, AM_ABS}, {I_and, AM_INX}, {I_xxx, AM_IMP}, {I_xxx, AM_IMP}, {I_bit, AM_ZP0}, {I_and, AM_ZP0}, {I_rol, AM_ZP0}, {I_xxx, AM_IMP}, {I_plp, AM_IMP}, {I_and, AM_IMM}, {I_rol, AM_IMP}, {I_xxx, AM_IMP}, {I_bit, AM_ABS}, {I_and, AM_ABS}, {I_rol, AM_ABS}, {I_xxx, AM_IMP},
	{I_bmi, AM_IMM}, {I_and, AM_INY}, {I_xxx, AM_IMP}, {I_xxx, AM_IMP}, {I_nop, AM_IMP}, {I_and, AM_ZPX}, {I_rol, AM_ZPX}, {I_xxx, AM_IMP}, {I_sec, AM_IMP}, {I_and, AM_ABY}, {I_nop, AM_IMP}, {I_xxx, AM_IMP}, {I_nop, AM_IMP}, {I_and, AM_ABX}, {I_rol, AM_ABX}, {I_xxx, AM_IMP},
	{I_rti, AM_IMP}, {I_eor, AM_INX}, {I_xxx, AM_IMP}, {I_xxx, AM_IMP}, {I_nop, AM_IMP}, {I_eor, AM_ZP0}, {I_lsr, AM_ZP0}, {I_xxx, AM_IMP}, {I_pha, AM_IMP}, {I_eor, AM_IMM}, {I_lsr, AM_IMP}, {I_xxx, AM_IMP}, {I_jmp, AM_ABS}, {I_eor, AM_ABS}, {I_lsr, AM_ABS}, {I_xxx, AM_IMP},
	{I_bvc, AM_IMM}, {I_eor, AM_INY}, {I_xxx, AM_IMP}, {I_xxx, AM_IMP}, {I_nop, AM_IMP}, {I_eor, AM_ZPX}, {I_lsr, AM_ZPX}, {I_xxx, AM_IMP}, {I_cli, AM_IMP}, {I_eor, AM_ABY}, {I_nop, AM_IMP}, {I_xxx, AM_IMP}, {I_nop, AM_IMP}, {I_eor, AM_ABX}, {I_lsr, AM_ABX}, {I_xxx, AM_IMP},
	{I_rts, AM_IMP}, {I_adc, AM_INX}, {I_xxx, AM_IMP}, {I_xxx, AM_IMP}, {I_nop, AM_IMP}, {I_adc, AM_ZP0}, {I_ror, AM_ZP0}, {I_xxx, AM_IMP}, {I_pla, AM_IMP}, {I_adc, AM_IMM}, {I_ror, AM_IMP}, {I_xxx, AM_IMP}, {I_jmp, AM_IND}, {I_adc, AM_ABS}, {I_ror, AM_ABS}, {I_xxx, AM_IMP},
	{I_bvs, AM_IMM}, {I_adc, AM_INY}, {I_xxx, AM_IMP}, {I_xxx, AM_IMP}, {I_nop, AM_IMP}, {I_adc, AM_ZPX}, {I_ror, AM_ZPX}, {I_xxx, AM_IMP}, {I_sei, AM_IMP}, {I_adc, AM_ABY}, {I_nop, AM_IMP}, {I_xxx, AM_IMP}, {I_nop, AM_IMP}, {I_adc, AM_ABX}, {I_ror, AM_ABX}, {I_xxx, AM_IMP},
	{I_nop, AM_IMP}, {I_sta, AM_INX}, {I_nop, AM_IMP}, {I_xxx, AM_IMP}, {I_sty, AM_ZP0}, {I_sta, AM_ZP0}, {I_stx, AM_ZP0}, {I_xxx, AM_IMP}, {I_dey, AM_IMP}, {I_nop, AM_IMP}, {I_txa, AM_IMP}, {I_xxx, AM_IMP}, {I_sty, AM_ABS}, {I_sta, AM_ABS}, {I_stx, AM_ABS}, {I_xxx, AM_IMP},
	{I_bcc, AM_IMM}, {I_sta, AM_INY}, {I_xxx, AM_IMP}, {I_xxx, AM_IMP}, {I_sty, AM_ZPX}, {I_sta, AM_ZPX}, {I_stx, AM_ZPY}, {I_xxx, AM_IMP}, {I_tya, AM_IMP}, {I_sta, AM_ABY}, {I_txs, AM_IMP}, {I_xxx, AM_IMP}, {I_nop, AM_IMP}, {I_sta, AM_ABX}, {I_xxx, AM_IMP}, {I_xxx, AM_IMP},
	{I_ldy, AM_IMM}, {I_lda, AM_INX}, {I_ldx, AM_IMM}, {I_xxx, AM_IMP}, {I_ldy, AM_ZP0}, {I_lda, AM_ZP0}, {I_ldx, AM_ZP0}, {I_xxx, AM_IMP}, {I_tay, AM_IMP}, {I_lda, AM_IMM}, {I_tax, AM_IMP}, {I_xxx, AM_IMP}, {I_ldy, AM_ABS}, {I_lda, AM_ABS}, {I_ldx, AM_ABS}, {I_xxx, AM_IMP},
	{I_bcs, AM_IMM}, {I_lda, AM_INY}, {I_xxx, AM_IMP}, {I_xxx, AM_IMP}, {I_ldy, AM_ZPX}, {I_lda, AM_ZPX}, {I_ldx, AM_ZPY}, {I_xxx, AM_IMP}, {I_clv, AM_IMP}, {I_lda, AM_ABY}, {I_tsx, AM_IMP}, {I_xxx, AM_IMP}, {I_ldy, AM_ABX}, {I_lda, AM_ABX}, {I_ldx, AM_ABY}, {I_xxx, AM_IMP},
	{I_cpy, AM_IMM}, {I_cmp, AM_INX}, {I_nop, AM_IMP}, {I_xxx, AM_IMP}, {I_cpy, AM_ZP0}, {I_cmp, AM_ZP0}, {I_dec, AM_ZP0}, {I_xxx, AM_IMP}, {I_iny, AM_IMP}, {I_cmp, AM_IMM}, {I_dex, AM_IMP}, {I_xxx, AM_IMP}, {I_cpy, AM_ABS}, {I_cmp, AM_ABS}, {I_dec, AM_ABS}, {I_xxx, AM_IMP},
	{I_bne, AM_IMM}, {I_cmp, AM_INY}, {I_xxx, AM_IMP}, {I_xxx, AM_IMP}, {I_nop, AM_IMP}, {I_cmp, AM_ZPX}, {I_dec, AM_ZPX}, {I_xxx, AM_IMP}, {I_cld, AM_IMP}, {I_cmp, AM_ABY}, {I_nop, AM_IMP}, {I_xxx, AM_IMP}, {I_nop, AM_IMP}, {I_cmp, AM_ABX}, {I_dec, AM_ABX}, {I_xxx, AM_IMP},
	{I_cpx, AM_IMM}, {I_sbc, AM_INX}, {I_nop, AM_IMP}, {I_xxx, AM_IMP}, {I_cpx, AM_ZP0}, {I_sbc, AM_ZP0}, {I_inc, AM_ZP0}, {I_xxx, AM_IMP}, {I_inx, AM_IMP}, {I_sbc, AM_IMM}, {I_nop, AM_IMP}, {I_sbc, AM_IMP}, {I_cpx, AM_ABS}, {I_sbc, AM_ABS}, {I_inc, AM_ABS}, {I_xxx, AM_IMP},
	{I_beq, AM_IMM}, {I_sbc, AM_INY}, {I_xxx, AM_IMP}, {I_xxx, AM_IMP}, {I_nop, AM_IMP}, {I_sbc, AM_ZPX}, {I_inc, AM_ZPX}, {I_xxx, AM_IMP}, {I_sed, AM_IMP}, {I_sbc, AM_ABY}, {I_nop, AM_IMP}, {I_xxx, AM_IMP}, {I_nop, AM_IMP}, {I_sbc, AM_ABX}, {I_inc, AM_ABX}, {I_xxx, AM_IMP}
};*/



void _6502_reset() {
	#if (_6502_RESET_ON_START) //set the program counter to the address taken from the reset vector
	_PC = get_w(__6502_RESET_V);
	#else
	_PC = _6502_START_ADDRESS;
	#endif

	_A = _X = _Y = 0;
	_SP = 0xff;
	_P._raw = 0x30; //0b00110000; //unused flag set
}

void _6502_interrupt() {
	if (!_P.flags.i)
		interr(__6502_IRQ_V, _P._raw & (~BIT_O(4)));
}

void _6502_nmi() {
	interr(__6502_NMI_V, _P._raw & (~BIT_O(4)));
}

void _6502_clock() {
	_IR = _6502_read(_PC++);

	//SDL_Log("%02x, %04x\n", _IR, _PC);
	//amode = i_jtable[_IR].A_func_i;
	A_funcs[i_jtable[_IR].A_func_i](); //call addressing-mode function
	fetch = i_jtable[_IR].fetch;

	if (fetch) data = _6502_read(addr);

	//SDL_Log("%04x\n", addr);

	//if the flag is true, do a fetch (data = _6502_read(addr))
	(i_jtable[_IR].I_func)(); //call operative function
}