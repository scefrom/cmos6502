#pragma once



#include <stdint.h>



#define __6502_STACK_BOTTOM			0x0100
#define __6502_STACK_TOP			0x01ff

#define __6502_NMI_V				0xfffa
#define __6502_RESET_V				0xfffc
#define __6502_IRQ_V				0xfffe
#define __6502_BRK_V				0xfffe

//some emulating debug flags, dont mind them
//#define _6502_DEBUG 				1
//#define _6502_PRINT_STATUS 			1
//#define _6502_TRACK_ADDR1			0x000c
//#define _6502_TRACK_ADDR2			0x000d
//#define _6502_STOP_ENABLED			1
#define _6502_RESET_ON_START		0
//#define _6502_GET_I_TIME			1

//#if (_6502_STOP_ENABLED)
//#define _6502_STOP_AT				0x336d
//#endif

#if (!_6502_RESET_ON_START)
#define _6502_START_ADDRESS			0x0400
#endif

//#if (_6502_PRINT_STATUS)
//#define _6502_PRINT_ONLY_ON_STOP	0
//#endif



//extern uint8_t __debug;

typedef union {
	struct {
		uint8_t c : 1;
		uint8_t z : 1;
		uint8_t i : 1;
		uint8_t d : 1;
		uint8_t b : 1;
		uint8_t u : 1;
		uint8_t v : 1;
		uint8_t n : 1;
	} flags;
	uint8_t _raw;
} cpu_s_t;



/*
	Yes, i know, global variables. Forgive me.
	Just didn't want to pass a struct pointer to each static function / use C++ and classes
*/

extern uint16_t _PC;
extern uint8_t _A, _X, _Y, _SP, _IR;
extern cpu_s_t _P;



extern uint8_t _6502_read(uint16_t);
extern void _6502_write(uint16_t, uint8_t);



void _6502_reset();
void _6502_interrupt();
void _6502_nmi();
void _6502_clock();