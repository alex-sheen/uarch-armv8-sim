/*
 * ARM pipeline timing simulator
 *
 * CMSC 22200, Fall 2016
 * Gushu Li and Reza Jokar

 COLLABORATORS:
 * Kiki Apple - carolineapple
 * Alex Sheen - alexsheen
 */

#include "bp.h"
#include "shell.h"
#include "pipe.h"
#include <stdlib.h>
#include <stdio.h>

/* VARIABLES */
bp_t bp;

/* HELPER FUNCTIONS */
int getPHT(uint64_t instrPC){
    //gets the entry in the PHT given GHR and PC
    instrPC = (instrPC&1020)>>2;
    int index = bp.Gshare.GHR ^ instrPC;
    return bp.Gshare.PHT[index];
}

void setPHT(uint64_t instrPC, bool taken){
    //updates the entry in the PHT
    instrPC = (instrPC&1020)>>2;
    int index = bp.Gshare.GHR ^ instrPC;
    int val = bp.Gshare.PHT[index];
    switch(val){
        //strongly not taken
        case 0:
            bp.Gshare.PHT[index] = taken? 1 : 0;
            break;
        case 1:
            bp.Gshare.PHT[index] = taken? 2 : 0;
            break;
        case 2:
            bp.Gshare.PHT[index] = taken? 3 : 1;
            break;
        case 3:
            bp.Gshare.PHT[index] = taken? 3 : 2;
            break;
        default:
            printf("Value in PHT not valid\n");
            break;
    }
}

void setGHR(bool taken){
    int take = taken? 1 : 0;
    bp.Gshare.GHR = ((bp.Gshare.GHR<<1) | take)&255;
}

void setBTB(uint64_t instrPC, uint64_t target, bool conditional){
    int index = (instrPC&4092)>>2;
    btbEntry_t entry = bp.BTB[index];
    entry.address = instrPC;
    entry.valid = 1;
    entry.conditional = conditional;
    entry.target = target;
    bp.BTB[index] = entry;
}

uint64_t getBranch(uint64_t instrPC){
    int index = (instrPC&4092)>>2;
    btbEntry_t entry = bp.BTB[index];
    if(entry.valid && entry.address == instrPC){
        if(!entry.conditional){
            return entry.target;
        }
        else{
            int taken = getPHT(entry.address);
            if(taken >= 2)
            {
                return entry.target;
            }
            else{
                return instrPC + 4;
            }
        }
    }
    else{
        //did not find in PC
        return instrPC + 4;
    }
}


/* MAIN FUNCTIONS */
void bp_predict()
{
    /* Predict next PC */
    //printf("  BP info\n");
    //printf("  GHR: %u\n", bp.Gshare.GHR);
    //printf("  Current_State.PC before: %u\n",CURRENT_STATE.PC );
    CURRENT_STATE.PC = getBranch(CURRENT_STATE.PC);
    //printf("  Current_State.PC after: %u\n",CURRENT_STATE.PC );
}

void bp_update(uint64_t instrPC, uint64_t targetPC, bool conditional, bool taken)
{
    /* Update BTB */
    setBTB(instrPC, targetPC, conditional);

   
    if(conditional)
    {
         /* Update gshare directional predictor */
         setPHT(instrPC, taken);

        /* Update global history register */
        setGHR(taken);
    }
    
}
