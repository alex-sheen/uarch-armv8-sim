/*
 * CMSC 22200
 *
 * ARM pipeline timing simulator
 *

    COLLABORATORS:
 * Kiki Apple - carolineapple
 * Alex Sheen - alexsheen
 */

#include "pipe.h"
#include "shell.h"
#include "bp.h"
#include "cache.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

/* cache variables */
cache_t *cacheI;
cache_t *cacheD;
bool missI;
uint32_t pendingMemInstr;
bool missD;
bool loadUse;
bool justCopied = false;


/* global pipeline state */
CPU_State CURRENT_STATE;
IF2ID_t IF2ID;
ID2EX_t ID2EX;
EX2MEM_t EX2MEM;
MEM2WB_t MEM2WB;
PendMem_t PendMem;

char format = 'a';
int IF_Stall;
uint64_t branchTo;
uint32_t use_A = 0;
uint32_t use_B = 0;
uint64_t bufferPC = 0;
int bufferCount = 0;

/* HELPER FUNCTIONS */
void stallPipeline(){
    IF2ID.stall = 50;
    ID2EX.stall = 50;
    EX2MEM.stall = 49;
    // IF_Stall = 49;
    //IF_Stall = IF_Stall > 0? IF_Stall + 1 : 50;

    IF_Stall = IF_Stall > 0? IF_Stall + (EX2MEM.stall - IF_Stall) : 50;
    MEM2WB.stall = 49;
}

void stallFetch(){
    IF_Stall = 49;
    IF2ID.instr = 0;
}

void killFetch(){

    //kill instr in IF2ID
    IF2ID.instr = 0;
}

void Bcond(bool taken)
{
    //printf("  taking bcond\n");
    int tmp = (int) ID2EX.imm; 
    if (tmp >> 18 == 1) {
        tmp = tmp | -524288; 
    } 

    if (tmp > 0) {
       bufferPC = ID2EX.PC + (tmp * 4);
    }

    else {
        tmp = abs(tmp);
        bufferPC = ID2EX.PC - (tmp * 4);
    }
    if(taken)
    {
        if(bufferPC == IF2ID.PC)
        {
            //printf("   taken branch, predicted right\n");
            bp_update(ID2EX.PC, bufferPC, true, taken);
            
            //printf("        bufferPC = %u\n", bufferPC);
            //printf("        ID2EX.PC = %u\n", ID2EX.PC);
            bufferPC = 0;
        }
        else{
            printf("   taken branch, predicted incorrectly BCond\n");
            if((CURRENT_STATE.PC&(~31))!= (bufferPC&(~31))){
                printf("into the thing\n");
                IF_Stall = 0;
                bufferCount = 1;
                IF2ID.writeEnable = 0;
            }
            killFetch();
            CURRENT_STATE.PC = bufferPC;
            bp_update(ID2EX.PC, bufferPC, true, taken);
            //printf("        bufferPC = %u\n", bufferPC);
            //printf("        ID2EX.PC = %u\n", ID2EX.PC);
        }  
    }
    else{
        //printf("        ID2EX.PC = %u\n", ID2EX.PC);
        //printf("        IF2ID.PC = %u\n", IF2ID.PC);

        if(ID2EX.PC == IF2ID.PC - 4)
        {
            //printf("   not taken branch, predicted not taken\n");
            bp_update(ID2EX.PC, IF2ID.PC, true, taken);
            //printf("        bufferPC = %u\n", bufferPC);
            //printf("        ID2EX.PC = %u\n", ID2EX.PC);
            bufferPC = 0;
        }
        else{
            printf("   not taken branch, but predicted taken BCond\n");
            bufferPC = ID2EX.PC + 4;
            if((CURRENT_STATE.PC&(~31))!= (bufferPC&(~31))){
                IF_Stall = 0;
                bufferCount = 1;
                IF2ID.writeEnable = 0;
            }
            killFetch();
            CURRENT_STATE.PC = ID2EX.PC + 4;
            bp_update(ID2EX.PC, bufferPC, true, taken);
            //printf("        bufferPC = %u\n", bufferPC);
            //printf("        ID2EX.PC = %u\n", ID2EX.PC);
        }
    }
    
}

void getFormat(int shift) {

    uint32_t temp = IF2ID.instr>>(32-shift);

    if(temp == 0) {
        ID2EX.opcode = 0;
        ID2EX.MemtoReg = 0;
        ID2EX.MemRead = 0;
        ID2EX.MemWrite = 0;
        format = 'a';
        use_A = 0;
        use_B = 0;
    }
    
    //ADD ext
    if (temp == 1160 || temp == 1112) {
        ID2EX.opcode = 1160;
        ID2EX.MemtoReg = 1;
        ID2EX.MemRead = 0;
        ID2EX.MemWrite = 0;
        format = 'R';
        use_A = 1;
        use_B = 1;
    }

    //ADD imm
    else if (temp == 580) {
        ID2EX.opcode = temp;
        ID2EX.MemtoReg = 1;
        ID2EX.MemRead = 0;
        ID2EX.MemWrite = 0;
        format = 'I';
        use_A = 1;
        use_B = 0;
    }

    //ADDS ext
    else if (temp == 1369 || temp == 1368) {
        ID2EX.opcode = 1369;
        ID2EX.MemtoReg = 1;
        ID2EX.MemRead = 0;
        ID2EX.MemWrite = 0;
        format = 'R';
        use_A = 1;
        use_B = 1;
    }

    //ADDS imm
    else if (temp == 708){
        ID2EX.opcode = temp;
        ID2EX.MemtoReg = 1;
        ID2EX.MemRead = 0;
        ID2EX.MemWrite = 0;
        format = 'I';
        use_A = 1;
        use_B = 0;
    }

    //CBNZ
    else if (temp == 181) {
        ID2EX.opcode = temp;
        ID2EX.MemtoReg = 0;
        ID2EX.MemRead = 0;
        ID2EX.MemWrite = 0;
        format = 'C';
        use_A = 0;
        use_B = 0;
    }

    //CBZ
    else if (temp == 180) {
        ID2EX.opcode = temp;
        ID2EX.MemtoReg = 0;
        ID2EX.MemRead = 0;
        ID2EX.MemWrite = 0;
        format = 'C';
        use_A = 0;
        use_B = 0;
    }

    //AND
    else if (temp == 1104) {
        ID2EX.opcode = temp;
        ID2EX.MemtoReg = 1;
        ID2EX.MemRead = 0;
        ID2EX.MemWrite = 0;
        format = 'R';
        use_A = 1;
        use_B = 1;
    }

    //ANDS
    else if (temp == 1872) {
        ID2EX.opcode = temp;
        ID2EX.MemtoReg = 1;
        ID2EX.MemRead = 0;
        ID2EX.MemWrite = 0;
        format = 'R';
        use_A = 1;
        use_B = 1;
    }

    //EOR
    else if (temp == 1616) {
        ID2EX.opcode = temp;
        ID2EX.MemtoReg = 1;
        ID2EX.MemRead = 0;
        ID2EX.MemWrite = 0;
        format = 'R';
        use_A = 1;
        use_B = 1;
    }

    //ORR
    else if (temp == 1360) {
        ID2EX.opcode = temp;
        ID2EX.MemtoReg = 1;
        ID2EX.MemRead = 0;
        ID2EX.MemWrite = 0;
        format = 'R';
        use_A = 1;
        use_B = 1;
    }

    //LDUR
    else if (temp == 1474 || temp == 1986) {
        ID2EX.opcode = temp;
        ID2EX.MemtoReg = 1;
        ID2EX.MemRead = 1;
        ID2EX.MemWrite = 0;
        format = 'D';
        use_A = 1;
        use_B = 0;
    }

    //LDURB
    else if (temp == 450) {
        ID2EX.opcode = temp;
        ID2EX.MemtoReg = 1;
        ID2EX.MemRead = 1;
        ID2EX.MemWrite = 0;
        format = 'D';
        use_A = 1;
        use_B = 0;
    }

    //LDURH
    else if (temp == 962) {
        ID2EX.opcode = temp;
        ID2EX.MemtoReg = 1;
        ID2EX.MemRead = 1;
        ID2EX.MemWrite = 0;
        format = 'D';
        use_A = 1;
        use_B = 0;
    }

    //LSL and LSR
    else if (temp == 845) {
        ID2EX.opcode = temp;
        ID2EX.MemtoReg = 1;
        ID2EX.MemRead = 0;
        ID2EX.MemWrite = 0;
        format = 'R';
        use_A = 1;
        use_B = 0;
    }

    //MOVZ
    else if (temp == 1685 || temp == 1684 || temp == 1687 || temp == 1686 || temp == 421) {
        //hard coding this to make it easier in execute switch
        ID2EX.opcode = 1684;
        ID2EX.MemtoReg = 1;
        ID2EX.MemRead = 0;
        ID2EX.MemWrite = 0;
        format = 'W';
        use_A = 0;
        use_B = 0;
    }

    //STUR
    else if (temp == 1984 || temp == 1472) {
        ID2EX.opcode = temp;
        ID2EX.MemtoReg = 0;
        ID2EX.MemRead = 0;
        ID2EX.MemWrite = 1;
        format = 'D';
        use_A = 1;
        use_B = 0;
    }

    //STURB
    else if (temp == 448) {
        ID2EX.opcode = temp;
        ID2EX.MemtoReg = 0;
        ID2EX.MemRead = 0;
        ID2EX.MemWrite = 1;
        format = 'D';
        use_A = 1;
        use_B = 0;
    }

    //STURH
    else if (temp == 960) {
        ID2EX.opcode = temp;
        ID2EX.MemtoReg = 0;
        ID2EX.MemRead = 0;
        ID2EX.MemWrite = 1;
        format = 'D';
        use_A = 1;
        use_B = 0;
    }

    //SUB ext
    else if (temp == 1625 || temp == 1624) {
        ID2EX.opcode = 1625;
        ID2EX.MemtoReg = 1;
        ID2EX.MemRead = 0;
        ID2EX.MemWrite = 0;
        format = 'R';
        use_A = 1;
        use_B = 1;
    }

    //SUB imm
    else if (temp == 836) {
        ID2EX.opcode = temp;
        ID2EX.MemtoReg = 1;
        ID2EX.MemRead = 0;
        ID2EX.MemWrite = 0;
        format = 'I';
        use_A = 1;
        use_B = 0;
    }

    //SUBS ext
    else if (temp == 1881 || temp == 1880) {
        ID2EX.opcode = 1881;
        ID2EX.MemtoReg = 1;
        ID2EX.MemRead = 0;
        ID2EX.MemWrite = 0;
        format = 'R';
        use_A = 1;
        use_B = 1;
    }

    //SUBS imm
    else if (temp == 964) {
        ID2EX.opcode = temp;
        ID2EX.MemtoReg = 1;
        ID2EX.MemRead = 0;
        ID2EX.MemWrite = 0;
        format = 'I';
        use_A = 1;
        use_B = 0;
    }

    //MUL
    else if (temp == 1240) {
        //0Q001111xxLM page 2001
        ID2EX.opcode = temp;
        ID2EX.MemtoReg = 1;
        ID2EX.MemRead = 0;
        ID2EX.MemWrite = 0;
        format = 'R';
        use_A = 1;
        use_B = 1;
    }

    //HLT
    else if (temp == 1698) {
        ID2EX.opcode = temp;
        ID2EX.MemtoReg = 0;
        ID2EX.MemRead = 0;
        ID2EX.MemWrite = 0;
        format = 'W';
        use_A = 0;
        use_B = 0;
    }

    //BR
    else if (temp == 1712) {
        ID2EX.opcode = temp;
        ID2EX.MemtoReg = 0;
        ID2EX.MemRead = 0;
        ID2EX.MemWrite = 0;
        format = 'R';
        use_A = 1;
        use_B = 0;
    }

    //B
    else if (temp == 5) {
        ID2EX.opcode = temp;
        ID2EX.MemtoReg = 0;
        ID2EX.MemRead = 0;
        ID2EX.MemWrite = 0;
        format = 'B';
        use_A = 0;
        use_B = 0;
    }

    //B.cond (which includes BEQ, BNE, BGT, BLT, BGE, BLE)
    else if (temp == 84) {
        ID2EX.opcode = temp;
        ID2EX.MemtoReg = 0;
        ID2EX.MemRead = 0;
        ID2EX.MemWrite = 0;
        format = 'C';
        use_A = 0;
        use_B = 0;
    }
}

void setFlags(int result) {
    if (result == 0) {
        EX2MEM.FLAG_Z = 1;
        ID2EX.FLAG_Z = 1;
    } else {
        EX2MEM.FLAG_Z = 0;
        ID2EX.FLAG_Z = 0;
    }
    if (result < 0) {
        EX2MEM.FLAG_N = 1;
        ID2EX.FLAG_N = 1;
    } else {
        EX2MEM.FLAG_N = 0;
        ID2EX.FLAG_N = 0;
    }
}

void printInstr(uint32_t opcode){

    uint32_t temp = opcode;
    //LDUR
    if (temp == 1474 || temp == 1986) {
        printf("    Load\n");
    }

    //LDURB
    else if (temp == 450) {
        printf("    Load\n");
    }

    //LDURH
    else if (temp == 962) {
        printf("    Load\n");
    }
    //STUR
    else if (temp == 1984 || temp == 1472) {
        printf("    Store\n");
    }

    //STURB
    else if (temp == 448) {
        printf("    Store\n");
    }

    //STURH
    else if (temp == 960) {
       printf("    Store\n");
    }
    printf("\n");
}

void pipe_init()
{
    memset(&CURRENT_STATE, 0, sizeof(CPU_State));
    CURRENT_STATE.PC = 0x00400000;
    IF2ID.stall = 0;
    ID2EX.stall = 0;
    EX2MEM.stall = 0;
    MEM2WB.stall = 0;
    IF_Stall = 0;
    branchTo = 0;
    IF2ID.writeEnable = 1;
	cacheI = cache_new(64, 4, 32);
	cacheD = cache_new(256, 8, 32);

    missI = false;
    missD = false;
    loadUse = false;

    printf("\n");
    printf("first PC read: %u\n", mem_read_32(CURRENT_STATE.PC));   
    printf("\n");
}

void pipe_cycle()
{
    printf("\n\n CYCLE: %u\n", stat_cycles+1);
	pipe_stage_wb();
	pipe_stage_mem();
	pipe_stage_execute();
	pipe_stage_decode();
	pipe_stage_fetch();
}


void pipe_stage_wb()
{
    //printf("WB: ");
    //printInstr(MEM2WB.opcode);
    printf(" WB opcode = %u\n",MEM2WB.opcode);
    if(MEM2WB.opcode == 1698)
    {
        //printf("WB: HLT\n");
        cache_destroy(cacheD);
        cache_destroy(cacheI);
        RUN_BIT = 0;
        stat_inst_retire++;
    }
	else if(MEM2WB.stall == 0 && MEM2WB.opcode != 0)
	{
        
        if(MEM2WB.MemtoReg == 1){
            printf("WB: writing %lu to register %lu\n", MEM2WB.result, MEM2WB.dest);
            CURRENT_STATE.REGS[MEM2WB.dest] = MEM2WB.result;
        }
        stat_inst_retire++;
        CURRENT_STATE.FLAG_N = MEM2WB.FLAG_N;
        CURRENT_STATE.FLAG_Z = MEM2WB.FLAG_Z;
    }
    else if(MEM2WB.stall > 0){
        //printf("WB stall = %u\n", MEM2WB.stall);
        MEM2WB.stall--;
    }
    
}

void copyToPend()
{
    PendMem.opcode = EX2MEM.opcode; 
    PendMem.result = EX2MEM.result;
    PendMem.dest = EX2MEM.dest;
    PendMem.imm = EX2MEM.imm;
    PendMem.addy = EX2MEM.addy;
    PendMem.MemtoReg = EX2MEM.MemtoReg;
    PendMem.MemRead = EX2MEM.MemRead;
    PendMem.MemRead = EX2MEM.MemRead;
    PendMem.MemWrite = EX2MEM.MemWrite;
    PendMem.FLAG_Z = EX2MEM.FLAG_Z;
    PendMem.FLAG_N = EX2MEM.FLAG_N;
}

void cacheDHit(cacheReturn_t ret) {
    if(ret.hit == false){
        missD = true;
        copyToPend();
        justCopied = true;
    }
}
void pipe_stage_mem()
{
    //printf("MEM: ");
    //printInstr(EX2MEM.opcode);
    cacheReturn_t ret;
    uint32_t LSB;
    uint64_t MSB;
    uint64_t addy;
    uint64_t result;
    uint32_t opcode;
    
    if(PendMem.opcode != 0){
        printf("    using the pendMem\n");
        opcode = PendMem.opcode;
        addy = PendMem.addy;
        result = PendMem.result;
    }
    else{
        printf("    using the normal stuff\n");
        opcode = EX2MEM.opcode;
        addy = EX2MEM.addy;
        result = EX2MEM.result;
    }
    if(EX2MEM.stall == 0){
        printf("MEM: opcode = %d\n", EX2MEM.opcode);
        
        switch(opcode){
            // LDUR 64
            case 1986:
                ret = cache_checkLoad(cacheD, addy, false, 8, false, 0);
                cacheDHit(ret);
                MEM2WB.result = ret.data;
                break;
            // LDUR 32
            case 1474:
                ret = cache_checkLoad(cacheD, addy, false, 4, false, 0);
                cacheDHit(ret);
                MEM2WB.result = ret.data;
                break;
            // LDURB
            case 450:
                ret = cache_checkLoad(cacheD, addy, false, 2, false, 0);
                cacheDHit(ret);
                MEM2WB.result = ret.data;
                break;
            // LDURH
            case 962:
                ret = cache_checkLoad(cacheD, addy, false, 1, false, 0);
                cacheDHit(ret);
                MEM2WB.result = ret.data;
                break;
            // STUR 64
            case 1984:
                ret = cache_checkLoad(cacheD, addy, false, 8, true, result);
                cacheDHit(ret);
                break;
            // STUR 32
            case 1472:
                ret = cache_checkLoad(cacheD, addy, false, 4, true, result);
                cacheDHit(ret);
                break;
            // STURB
            case 448:
                ret = cache_checkLoad(cacheD, addy, false, 2, true, result);
                cacheDHit(ret);
                break;
            // STURH
            case 960:
                ret = cache_checkLoad(cacheD, addy, false, 1, true, result);
                cacheDHit(ret);
                break;
            // HLT
            case 1698:
                //make a variable that says if it is halted or not
                //in WB stage we can turn runbit to 0
                break;
            default:
                MEM2WB.result = EX2MEM.result;
                break;
        }
        if(PendMem.opcode != 0 && !justCopied){
            printf("    copying over PendMem to WB\n");
            MEM2WB.dest = PendMem.dest;
            MEM2WB.MemtoReg = PendMem.MemtoReg;
            MEM2WB.opcode = PendMem.opcode;
            MEM2WB.FLAG_N = PendMem.FLAG_N;
            MEM2WB.FLAG_Z = PendMem.FLAG_Z;
            PendMem.opcode = 0;
            PendMem.addy = 0;
            PendMem.result = 0;
            justCopied = false;
        }
        else{
            printf("    copying over normal stuff to WB\n");
            printf(" MEM opcode = %u\n",EX2MEM.opcode);
            MEM2WB.dest = EX2MEM.dest;
            MEM2WB.MemtoReg = EX2MEM.MemtoReg;
            MEM2WB.opcode = EX2MEM.opcode;
            MEM2WB.FLAG_N = EX2MEM.FLAG_N;
            MEM2WB.FLAG_Z = EX2MEM.FLAG_Z;
            justCopied = false;
        }
    }
    else if(EX2MEM.stall == 1 && missD){
        printf("CacheD fill\n");
        missD = false;
        EX2MEM.stall--;
        switch(opcode){
            // LDUR 64
            case 1986:
                insertCacheD(cacheD, addy, false, 8, false, 0);
                MEM2WB.result = ret.data;
                break;
            // LDUR 32
            case 1474:
                insertCacheD(cacheD, addy, false, 4, false, 0);
                MEM2WB.result = ret.data;
                break;
            // LDURB
            case 450:
                insertCacheD(cacheD, addy, false, 2, false, 0);
                MEM2WB.result = ret.data;
                break;
            // LDURH
            case 962:
                insertCacheD(cacheD, addy, false, 1, false, 0);
                MEM2WB.result = ret.data;
                break;
            // STUR 64
            case 1984:
                insertCacheD(cacheD, addy, false, 8, true, result);
                break;
            // STUR 32
            case 1472:
                insertCacheD(cacheD, addy, false, 4, true, result);
                break;
            // STURB
            case 448:
                insertCacheD(cacheD, addy, false, 2, true, result);
                break;
            // STURH
            case 960:
                insertCacheD(cacheD, addy, false, 1, true, result);
                break;
            // HLT
            case 1698:
                //make a variable that says if it is halted or not
                //in WB stage we can turn runbit to 0
                break;
            default:
                MEM2WB.result = EX2MEM.result;
                break;
        }
    }
    else{
        //printf("MEM: stalled\n");
        if (missD) {
            printf("CacheD stall (%u)\n", EX2MEM.stall);
        }
        EX2MEM.stall--;
    }
}

void pipe_stage_execute()
{
    //printf("EXE: ");
    //printInstr(ID2EX.opcode);
     if(loadUse) {
        printf("EXE: cycle after loadUse, stall FE/DE/EXE, bubble EXE\n");
        IF_Stall += 1;
        IF2ID.stall += 1;
        ID2EX.stall += 1;
        loadUse = false;

        ID2EX.forwardResult1 = MEM2WB.result;
        printf("updating forwardresult1 = %u\n", MEM2WB.result);

        EX2MEM.MemtoReg = 0;
        EX2MEM.MemRead = 0;
        EX2MEM.MemWrite = 0;
        EX2MEM.opcode = 0;
    }

    if(ID2EX.stall == 0){ 
        EX2MEM.MemtoReg = ID2EX.MemtoReg;
        EX2MEM.MemRead = ID2EX.MemRead;
        EX2MEM.MemWrite = ID2EX.MemWrite;
        EX2MEM.opcode = ID2EX.opcode;

       

        if(ID2EX.dependDistA == 1 && ID2EX.opcode != 0)
        {
            //printf("EX dependence, DistA 1, replace %u with %d\n", ID2EX.A, ID2EX.forwardResult1);
            ID2EX.A = ID2EX.forwardResult1;
        }
        else if(ID2EX.dependDistA == 2 && ID2EX.opcode != 0)
        {
            //printf("EX dependence, DistA 2, replace %u with %d\n", ID2EX.A, ID2EX.forwardResult2);
            ID2EX.A = ID2EX.forwardResult2;
        }
        if(ID2EX.dependDistB == 1 && ID2EX.opcode != 0)
        {
            if(ID2EX.opcode == 1984 || ID2EX.opcode == 1472 || ID2EX.opcode == 448 || ID2EX.opcode == 960){
                //printf("EX dependence, STUR dest, replace %u with %d\n", ID2EX.dest, ID2EX.forwardResult1);
                ID2EX.destVal = ID2EX.forwardResult1;
            }
            else if(ID2EX.opcode == 181 || ID2EX.opcode == 180) {
                //printf("EX dependence, CBZ dest, replace %u with %d\n", ID2EX.dest, ID2EX.forwardResult1);
                ID2EX.destVal = ID2EX.forwardResult1;
            }
            else{
                //printf("EX dependence, DistB 1, replace %u with %d\n", ID2EX.B, ID2EX.forwardResult1);
                ID2EX.B = ID2EX.forwardResult1;
            }
            
        }
        else if(ID2EX.dependDistB == 2 && ID2EX.opcode != 0)
        {
            if(ID2EX.opcode == 1984 || ID2EX.opcode == 1472 || ID2EX.opcode == 448 || ID2EX.opcode == 960){
                //printf("EX dependence, STUR dest, replace %u with %d\n", ID2EX.dest, ID2EX.forwardResult2);
                ID2EX.destVal = ID2EX.forwardResult2;
            }
            else if(ID2EX.opcode == 181 || ID2EX.opcode == 180) {
                //printf("EX dependence, CBZ dest, replace %u with %d\n", ID2EX.dest, ID2EX.forwardResult2);
                ID2EX.destVal = ID2EX.forwardResult2;
            }
            else{
                //printf("EX dependence, DistB 2, replace %u with %d\n", ID2EX.B, ID2EX.forwardResult2);
                ID2EX.B = ID2EX.forwardResult2;
            }
        }

        if(ID2EX.opcode == 84 && ID2EX.dependDistB == 1)
        {
            //printf("EX cmp and bcond depedency\n");
        }

        //printf("EXE: opcode = %d\n", ID2EX.opcode);
        int tmp;
        switch (ID2EX.opcode) {
            // ADD_ex
            case 1160:
                EX2MEM.result = ID2EX.A + ID2EX.B;
                EX2MEM.dest = ID2EX.dest;
                break;
            // ADDS_ex
            case 1369:
                EX2MEM.result = ID2EX.A + ID2EX.B;
                setFlags(EX2MEM.result);
                EX2MEM.dest = ID2EX.dest;
                break;
            // ADD_imm
            case 580:
                EX2MEM.result = ID2EX.imm + ID2EX.A;
                EX2MEM.dest = ID2EX.dest;
                break;
            // ADDS_imm
            case 708:
                EX2MEM.result = ID2EX.imm + ID2EX.A;
                //printf("adds result: %u\n", EX2MEM.result);
                //printf("adds imm: %u\n", ID2EX.imm);
                //printf("adds A: %u\n", ID2EX.A);
                setFlags(EX2MEM.result);
                EX2MEM.dest = ID2EX.dest;
                break;
            // AND
            case 1104:
                EX2MEM.result = ID2EX.A & ID2EX.B;
                EX2MEM.dest = ID2EX.dest;
                break;
            // ANDS
            case 1872:
                EX2MEM.result = ID2EX.A & ID2EX.B;
                setFlags(EX2MEM.result);
                EX2MEM.dest = ID2EX.dest;
                break;
            // EOR
            case 1616:
                EX2MEM.result = ID2EX.A ^ ID2EX.B;
                EX2MEM.dest = ID2EX.dest;
                break;
            // ORR
            case 1360:
                EX2MEM.result = ID2EX.A | ID2EX.B;
                EX2MEM.dest = ID2EX.dest;
                break;
            // MUL
            case 1240:
                EX2MEM.result = ID2EX.A * ID2EX.B;
                EX2MEM.dest = ID2EX.dest;
                break;
            // SUB_ex
            case 1625:
                EX2MEM.result = ID2EX.A - ID2EX.B;
                EX2MEM.dest = ID2EX.dest;
                break;
            // SUBS_ex
            case 1881:
                EX2MEM.result = ID2EX.A - ID2EX.B;
                setFlags(EX2MEM.result);
                if(ID2EX.dest == 31){
                    // check dest = 31, for CMP
                    EX2MEM.MemtoReg = 0;
                }
                EX2MEM.dest = ID2EX.dest;
                break;
            // SUB_imm
            case 836:
                EX2MEM.result = ID2EX.A - ID2EX.imm;
                EX2MEM.dest = ID2EX.dest;
                break;
            // SUBS_imm
            case 964:
                EX2MEM.result = ID2EX.A - ID2EX.imm;
                setFlags(EX2MEM.result);
                if(ID2EX.dest == 31){
                    //look at SUBS_ex
                    EX2MEM.MemtoReg = 0;
                }
                EX2MEM.dest = ID2EX.dest;
                break;
            // HLT
            case 1698:
                //make a variable that says if it is halted or not
                //in WB stage we can turn runbit to 0
                ID2EX.opcode = 0;
                IF2ID.instr = 0;
                IF2ID.writeEnable = 0;
                break;
            // LDUR
            case 1986:
                EX2MEM.addy = ID2EX.A + ID2EX.imm;
                EX2MEM.dest = ID2EX.dest;
                break;
            // LDUR
            case 1474:
                EX2MEM.addy = ID2EX.A+ID2EX.imm;
                EX2MEM.dest = ID2EX.dest;
                break;
            // LDURB
            case 450:
                EX2MEM.addy = ID2EX.A+ID2EX.imm;
                EX2MEM.dest = ID2EX.dest;
                break;
            // LDURH:
            case 962:
                EX2MEM.addy = ID2EX.A+ID2EX.imm;
                EX2MEM.dest = ID2EX.dest;
                break;
            // CBNZ:
            case 181:
                Bcond(ID2EX.destVal != 0);
                break;
            // CBZ
            case 180:
                Bcond(ID2EX.destVal == 0);
                break;
            // LSR / LSL
            case 845:
                if (ID2EX.imm == 63)
                {
                    //LSR
                    EX2MEM.result = ID2EX.A >> ID2EX.Bimm;
                }
                else {
                    //LSL
                    EX2MEM.result = ID2EX.A << (63-ID2EX.imm);
                }
                EX2MEM.dest = ID2EX.dest;
                break;
            // MOVZ
            case 1684:
                EX2MEM.result = ID2EX.imm;
                EX2MEM.dest = ID2EX.dest;
                break;
            // STUR
            case 1984:
                EX2MEM.addy = ID2EX.A+ID2EX.imm;
                EX2MEM.result = ID2EX.destVal;
                break;
            // STUR
            case 1472:
                EX2MEM.addy = ID2EX.A+ID2EX.imm;
                EX2MEM.result = ID2EX.destVal;
                break;
            // STURB
            case 448:
                EX2MEM.addy = ID2EX.A+ID2EX.imm;
                EX2MEM.result = ID2EX.destVal;
                //printf("STURB addy: %u\n", EX2MEM.addy);
                //printf("STURB result: %u\n", EX2MEM.result);
                break;
            // STURH
            case 960:
                EX2MEM.addy = ID2EX.A+ID2EX.imm;
                EX2MEM.result = ID2EX.destVal;
                break;
            // BR
            case 1712:
                bufferPC = ID2EX.A;
                if(bufferPC == IF2ID.PC)
                {
                    //printf("   taken branch, predicted right\n");
                    bp_update(ID2EX.PC, bufferPC, false, true);
                    bufferPC = 0;
                }
                else{
                    printf("   taken branch, predicted incorrectly BR\n");
                    if((CURRENT_STATE.PC&(~31))!= (bufferPC&(~31))){
                        printf("into the thing\n");
                        IF_Stall = 0;
                        bufferCount = 1;
                        IF2ID.writeEnable = 0;
                    }
                    killFetch();
                    CURRENT_STATE.PC = bufferPC;
                    bp_update(ID2EX.PC, bufferPC, false, true);
                    IF_Stall = 0;
                }
                break;
            // B
            case 5:
                tmp = (int) ID2EX.imm; 
                if (tmp >> 25 == 1) {
                    tmp = tmp | -67108864; 
                } 

                if (tmp > 0) { 
                    bufferPC = ID2EX.PC + (tmp * 4);
                }

                else {
                    tmp = abs(tmp);
                    bufferPC = ID2EX.PC - (tmp * 4);
                }
                if(bufferPC == IF2ID.PC)
                {
                    //printf("   taken branch, predicted right\n");
                    bp_update(ID2EX.PC, bufferPC, false, true);
                    bufferPC = 0;
                }
                else{
                    printf("   taken branch, predicted incorrectly B\n");
                    if((CURRENT_STATE.PC&(~31))!= (bufferPC&(~31))){
                        printf("into the thing\n");
                        IF_Stall = 0;
                        bufferCount = 1;
                        IF2ID.writeEnable = 0;
                    }
                    killFetch();
                    CURRENT_STATE.PC = bufferPC;
                    bp_update(ID2EX.PC, bufferPC, false, true);
                    IF_Stall = 0;
                }
                break;
            // B_cond
            case 84:
                switch(ID2EX.dest){
                    case 0:
                        Bcond(ID2EX.FLAG_Z != 0);
                        break;
                    case 1:
                        Bcond(ID2EX.FLAG_Z == 0);
                        break;
                    case 12:
                        Bcond((ID2EX.FLAG_Z == 0) && (ID2EX.FLAG_N == 0));
                        break;
                    case 11:
                        Bcond((ID2EX.FLAG_Z == 0) && (ID2EX.FLAG_N != 0));
                        break;
                    case 10:
                        Bcond((ID2EX.FLAG_Z != 0) || (ID2EX.FLAG_N == 0));
                        break;
                    case 13:
                        Bcond((ID2EX.FLAG_Z != 0) || (ID2EX.FLAG_N != 0));
                        break;
                    default:
                        printf("execute (B.cond): Rt not found, Rt: %u \n", ID2EX.dest);
                }
                break;
            default:
                printf("execute: opcode not found, opcode %u\n", ID2EX.opcode);
        }

        //forward the data
        ID2EX.forwardResult2 = MEM2WB.result;
        ID2EX.forwardResult1 = EX2MEM.result;
        printf("forwardResult1 = %u\n", EX2MEM.result);
        printf("forwardResult2 = %u\n", MEM2WB.result);
    }
    else {
        //printf("EXE: stalled\n");
        ID2EX.stall--;
    }
}

void pipe_stage_decode()
{

    if(IF2ID.instr == 0)
    {
        //printf("decode bubble\n");
        ID2EX.opcode == 0;
        ID2EX.MemtoReg = 0;
        ID2EX.MemRead = 0;
        ID2EX.MemWrite = 0;
    }

    if(IF2ID.stall == 0) {
        //printf("DE: %u\n", IF2ID.instr);
        /*reset all controls*/
        ID2EX.MemtoReg = 0;
        ID2EX.MemRead = 0;
        ID2EX.MemWrite = 0;

        /* shift to shrink to ID2EX.opcode
        saves format*/
        // 11 : 21, 10 : 22, 8: 24, 6 :26
        format = 'a'; //reset

        getFormat(11);
        getFormat(10);
        getFormat(8);
        getFormat(6);

        int mask2 = 3;
        int mask5 = 31;
        int mask6 = 63;
        int mask9 = 511;
        int mask12 = 4095;
        int mask16 = 65535;
        int mask19 = 524287;
        int mask26 = 67108863;

        int regA;
	    int regB;
        
        int branch = 0;

        if (format == 'a')
        {
            //printf("ERROR instr not found: %u\n", IF2ID.instr);
        }

        switch(format){
            case 'R':
                ID2EX.dest = IF2ID.instr  & mask5;
                ID2EX.A = CURRENT_STATE.REGS[(IF2ID.instr >>5) & mask5];
                ID2EX.B = CURRENT_STATE.REGS[(IF2ID.instr >>16) & mask5]; 
                ID2EX.Bimm = (IF2ID.instr >>16) & mask6;
                ID2EX.imm = (IF2ID.instr >>10) & mask6;  
                regA = (IF2ID.instr >>5) & mask5;
                regB = (IF2ID.instr >>16) & mask5;
                break;
            case 'I':
                ID2EX.dest = IF2ID.instr  & mask5;   
                ID2EX.A = CURRENT_STATE.REGS[(IF2ID.instr >>5) & mask5];  
                ID2EX.imm = (IF2ID.instr >>10) & mask12;
                regA = (IF2ID.instr >>5) & mask5;
                break;
            case 'D':
                ID2EX.destVal = CURRENT_STATE.REGS[IF2ID.instr  & mask5];
                ID2EX.dest = IF2ID.instr  & mask5;
                ID2EX.A = CURRENT_STATE.REGS[(IF2ID.instr >>5) & mask5];
                ID2EX.imm = (IF2ID.instr >>12) & mask9;
                regA = (IF2ID.instr >>5) & mask5;
                break;
            case 'B':
                ID2EX.imm = IF2ID.instr & mask26;
                branch = 1;
                break;
            case 'C':
                ID2EX.dest = IF2ID.instr & mask5;
                ID2EX.destVal = CURRENT_STATE.REGS[IF2ID.instr & mask5];
                ID2EX.imm = (IF2ID.instr >>5) & mask19;
                branch = 1;
                break;
            case 'W':
                ID2EX.dest = IF2ID.instr  & mask5;
                ID2EX.imm = (IF2ID.instr >>5) & mask16;
                break;
            default:
                printf("decode: no match, instr: %u\n", IF2ID.instr);
                break;
        } 
        
        //check for a dependency
        if((regA == EX2MEM.dest) && (use_A == 1) && (EX2MEM.MemtoReg == 1)) {
            //opA dependency, dist of 1
            ID2EX.dependDistA = 1;
        }
        else if((regA == MEM2WB.dest) && (use_A == 1) && (MEM2WB.MemtoReg == 1)) {
            //opA dependency, dist of 2
            ID2EX.dependDistA = 2;
        }
        else {
            //no dependency
            ID2EX.dependDistA = 0;
        }
        if((regB == EX2MEM.dest) && (use_B == 1) && (EX2MEM.MemtoReg == 1)) {
            //opB dependency, dist of 1
            ID2EX.dependDistB = 1;
        }
        else if((regB == MEM2WB.dest) && (use_B == 1) && (MEM2WB.MemtoReg == 1)) {
            //opB dependency, dist of 2
            ID2EX.dependDistB = 2;
        }
        else {
            //no dependency
            ID2EX.dependDistB = 0;
        }
        //special for STUR
        if(ID2EX.opcode == 1984 || ID2EX.opcode == 1472 || ID2EX.opcode == 448 || ID2EX.opcode == 960){
            //printf("in decode thing \n");
        
            if((ID2EX.dest == EX2MEM.dest) && (EX2MEM.MemtoReg == 1)) {
                //dest dependency, dist of 1
                ID2EX.dependDistB = 1;
            }
            else if((ID2EX.dest == MEM2WB.dest) && (MEM2WB.MemtoReg == 1)) {
                //dest dependency, dist of 2
                ID2EX.dependDistB = 2;
            }
        }

        //special for CBNZ and CBZ
        if(ID2EX.opcode == 181 || ID2EX.opcode == 180){
            //printf("in decode thing \n");
        
            if((ID2EX.dest == EX2MEM.dest) && (EX2MEM.MemtoReg == 1)) {
                //dest dependency, dist of 1
                ID2EX.dependDistB = 1;
            }
            else if((ID2EX.dest == MEM2WB.dest) && (MEM2WB.MemtoReg == 1)) {
                //dest dependency, dist of 2
                ID2EX.dependDistB = 2;
            }
        }

        //special for LDUR
        if(EX2MEM.opcode == 1474 || EX2MEM.opcode == 1986 || EX2MEM.opcode == 450 || EX2MEM.opcode == 962) {

            if((regA == EX2MEM.dest) && (use_A == 1))
            {
                ID2EX.dependDistA = 1;
                loadUse = true;

            }
            if((regB == EX2MEM.dest) && (use_B == 1))
            {

                ID2EX.dependDistB = 1;
                loadUse = true;
            }
        }
    
        ID2EX.PC = IF2ID.PC;

    }
    else {
        //printf("DE: stalled\n");
        IF2ID.stall--;
    }
    
}

void pipe_stage_fetch()
{
    printf("IF_Stall = %i\n", IF_Stall);
    if(IF_Stall == 0) {
        int retI;
        if(bufferPC != 0 && bufferCount == 0)
        {
            //printf("   bufferPC = %u\n", bufferPC);
            CURRENT_STATE.PC = bufferPC;
            bufferPC = 0;
            IF2ID.writeEnable = 1;
        }
        else if(bufferCount > 0){
            //printf("bufferCount--\n");
            bufferCount--;
        }
        if(IF2ID.writeEnable == 1){
            retI = cache_update(cacheI, CURRENT_STATE.PC);
            if(retI != -1){
                IF2ID.instr = retI;
                printf("CacheI hit -- (%u) = %u\n", CURRENT_STATE.PC, retI);
                IF2ID.PC = CURRENT_STATE.PC;
                bp_predict();
            }
            else{
                printf("CacheI missed --> stalling fetch for 49\n");
                stallFetch();
                missI = true;
            } 
        }
    }
    else if(IF_Stall == 1 && missI == true){
        missI = false;
        IF_Stall--;
        printf("CacheI fill\n");
        insertCacheI(cacheI, CURRENT_STATE.PC);
    }
    else{
        if (missI || missD) {
            printf("CacheI bubble (%u)\n", IF_Stall);
        }
        IF_Stall--;
    }

    if(missD && EX2MEM.stall == 0){
        printf("CacheD missed --> stalling pipeline for 49\n");
        stallPipeline();
        MEM2WB.opcode = 0;
    } 
}
