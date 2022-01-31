/*
 * ARM pipeline timing simulator
 *
 * CMSC 22200, Fall 2016
 */

#ifndef _BP_H_
#define _BP_H_

#include "shell.h"
#include "stdbool.h"
#include <limits.h>

typedef struct gshare_t {
	uint32_t GHR;
    int64_t PHT[256];
} gshare_t;

typedef struct btbEntry_t{
    uint64_t address;
    bool valid;
    bool conditional;
    uint64_t target;
} btbEntry_t;


typedef struct
{
    /* gshare */
    gshare_t Gshare;

    /* BTB */
    btbEntry_t BTB[1024];

} bp_t;



void bp_predict();
void bp_update(uint64_t instrPC, uint64_t targetPC, bool conditional, bool taken);

#endif
