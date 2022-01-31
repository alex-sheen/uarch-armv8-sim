/*
 * CMSC 22200
 *
 * ARM pipeline timing simulator
 */

#ifndef _PIPE_H_
#define _PIPE_H_

#include "shell.h"
#include "stdbool.h"
#include <limits.h>


typedef struct CPU_State {
	/* register file state */
	int64_t REGS[ARM_REGS];
	int FLAG_N;        /* flag N */
	int FLAG_Z;        /* flag Z */

	/* program counter in fetch stage */
	uint64_t PC;
	
} CPU_State;

/* VARIABLES */
typedef struct IF2ID_t {
	uint32_t instr;
	uint64_t PC;
	int stall;
	int writeEnable;
} IF2ID_t;

typedef struct ID2EX_t {
	uint64_t PC;
	uint32_t opcode;
	uint64_t A;
	uint64_t B;
	uint32_t imm;
	uint32_t dest;
	uint64_t destVal;
	uint32_t Bimm;

	int MemtoReg;
	int MemRead;
	int MemWrite;
	int FLAG_N;
	int FLAG_Z;

	int dependDistA;
	int dependDistB;
	uint64_t forwardResult1;
	uint64_t forwardResult2;

	int stall;
} ID2EX_t;

typedef struct EX2MEM_t {
	uint32_t opcode;
	uint64_t result;
	uint32_t dest;
	uint64_t imm;
	uint64_t addy;

	int MemtoReg;
	int MemRead;
	int MemWrite;
	int FLAG_Z;
	int FLAG_N;

	int stall;
} EX2MEM_t;

typedef struct MEM2WB_t {
	uint32_t opcode;
	uint64_t result;
	uint32_t dest;

	int MemtoReg;
	int FLAG_Z;
	int FLAG_N;

	int stall;
} MEM2WB_t;

typedef struct PendMem_t {
	uint32_t opcode;
	uint64_t result;
	uint32_t dest;
	uint64_t imm;
	uint64_t addy;

	int MemtoReg;
	int MemRead;
	int MemWrite;
	int FLAG_Z;
	int FLAG_N;

	int stall;
} PendMem_t;

int RUN_BIT;

/* global variable -- pipeline state */
extern CPU_State CURRENT_STATE;


/* called during simulator startup */
void pipe_init();

/* this function calls the others */
void pipe_cycle();

/* each of these functions implements one stage of the pipeline */
void pipe_stage_fetch();
void pipe_stage_decode();
void pipe_stage_execute();
void pipe_stage_mem();
void pipe_stage_wb();

#endif
