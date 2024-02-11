#include <stddef.h>
#include <stdio.h>

#include "6502.h"



#define RAM_START					((uint16_t) (0x0000))
#define RAM_END						((uint16_t) (0xffff))
#define RAM_SIZE					((size_t) (RAM_END - RAM_START + 1))

#define PRG_START					((uint16_t) (0x000a))



static uint8_t ram[RAM_SIZE];

inline uint8_t _6502_read(uint16_t a) {return ram[a];}
inline void _6502_write(uint16_t a, uint8_t x) {ram[a] = x;}



static size_t load_prg(const char *name) {
	FILE *fp = fopen(name, "rb");
	if (fp == NULL)
		return -1;

	fseek(fp, 0L, SEEK_END);
	size_t s = ftell(fp);
	rewind(fp);

	fread(ram + PRG_START, s, 1, fp);
	fclose(fp);

	return s;
}

int main(int argc, char** argv) {
	if (argc < 2) return 1;

	size_t prg_size = load_prg(argv[1]);
	if (prg_size == (size_t) -1)
		return 1;

	printf("MEM_SIZE:\t%lu bytes\t(0x%lx)\nFILE_SIZE:\t%lu bytes\t(0x%lx)\n\n", RAM_SIZE, RAM_SIZE, prg_size, prg_size);

	_6502_reset();

	//basically, loop till you get stuck. (not actually accurate, but works fine in this case)
	uint16_t old_pc;

	do {
		old_pc = _PC;
		_6502_clock();
	} while (_PC != old_pc);

	printf("stuck at:\t0x%04x\n", _PC);

	return 0;
}