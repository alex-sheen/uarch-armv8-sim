/*
 * CMSC 22200, Fall 2016
 *
 * ARM pipeline timing simulator
 *
 *  Collaborators: Caroline Apple (carolineapple) and
                    Alex Sheen (alexsheen)
 */

#include "cache.h"
#include <stdlib.h>
#include <stdio.h>

/* VARIABLES */


/* LRU FUNCTIONS */
node_t *newNode(uint64_t tag)
{
    node_t *tmp = (node_t*)malloc(sizeof(node_t));
    tmp->tag = tag;
    tmp->next = NULL;
    return tmp;
}

lru_list_t *makeLRU()
{
    lru_list_t *l = (lru_list_t*)malloc(sizeof(lru_list_t));
    l->head = NULL;
    l->tail = NULL;
    return l;
}

void enqueue(lru_list_t *l, uint64_t tag)
{
    node_t *tmp = newNode(tag);
	if(l == NULL)
	{
		fprintf(stderr, "ERROR, enqueue given null list\n");
	}
	if(tmp == NULL)
	{
		fprintf(stderr, "ERROR, enqueue couldn't allocate new lru node\n");
	}
    if (l->head == NULL) {
        l->head = tmp;
        l->tail = tmp;
        return;
    }
    l->tail->next = tmp;
	l->tail = tmp;
}

void dequeue(lru_list_t *l)
{
    if (l->head == NULL)
        return;

    node_t *tmp = l->head;

	if(l->tail == NULL)
	{
		return;
	}
    l->head = l->head->next;

    if (l->head == NULL)
        l->tail = NULL;

    free(tmp);
}

void deleteNode(lru_list_t *l, long tag)
{
    node_t *tmp = l->head;
    node_t *prev = NULL;

    if (tmp != NULL && tmp->tag == tag)
    {
        l->head = tmp->next;
		if(l->head == NULL)
		{
			l->tail = NULL;
		}
		free(tmp);
        return;
    }

    while (tmp != NULL && tmp->tag != tag)
    {
        prev = tmp;
        tmp = tmp->next;
    }

    if (tmp == NULL) return;

    prev->next = tmp->next;
    if(prev->next == NULL)
	{
		l->tail = prev;
    }
    free(tmp);
}

void printLru(lru_list_t *l)
{
    if(l == NULL)
    {
        printf("LRU is null\n");
        return;
    }
    if(l->head == NULL)
    {
        printf("LRU is empty\n");
        return;
    }
    node_t *tmp = l->head;
    int count = 0;
    while(tmp!=NULL)
    {
        printf("lru[%d] tag : %ld\n",count, tmp->tag);
        tmp = tmp->next;
        count++;
    }
}


/* CACHE FUNCTIONS */

void printSet(cache_t *c, uint32_t index){
    printf("  Printing Set\n");
    set_t s = c->sets[index];
    int i;
    for(i = 0; i < c->numEntries; i++){
        printf("    i: %i\n", i);
        printf("    valid: %i\n", s.entries[i].valid);
        printf("    tag: %u\n", s.entries[i].tag);
    }
}

bool isFull(cache_t *c, uint32_t index){
    set_t s = c->sets[index];
    int i;
    for(i = 0; i < c->numEntries; i++){
        if(s.entries[i].valid == 0){
            return false;
        }
    }
    return true;
}

cacheReturn_t cache_checkLoad(cache_t *c, uint64_t addy, bool instrC, int bytes, bool isStur, uint64_t data){
    uint32_t index;
    uint64_t tag;
    uint32_t offset;
    uint64_t dMask = 8191;
    index = (addy>>5)&255;
    tag = (addy>>13)&(~(dMask<<51));
    offset = addy&31;
    //init cacheReturn
    cacheReturn_t ret;

    // index into cache
    set_t s = c->sets[index];

    printf("\n DATA CACHE\n");
    printf("index: %u\n", index);
    printf("tag: %u\n", tag);
    printf("offset: %u\n", offset);
    printSet(c, index);
    printf("\n");

    int i;
    for(i = 0; i < c->numEntries; i++){

        printf("CACHE_D: i  = %i\n", i);
        printf("    valid = %i\n", s.entries[i].valid);
        printf("    tag = %i\n", s.entries[i].tag);

        if(s.entries[i].valid == 1 && s.entries[i].tag == tag)
        {
            printf("inside here\n");
            if(isStur){
                printf("CacheD hit -- stur (0x%X)\n", addy);
                printf("inside isStur\n");
                int j;
                for(j = 0; j < bytes; j++){
                    s.entries[i].data[offset+j] = (data&(255<<(j*8)))>>(j*8);
                }
                uint32_t LSB;
                uint64_t MSB;
                switch(bytes){
                    case 8:
                        LSB = data&1073741823;
                        MSB = data>>32;
                        mem_write_32(addy, LSB);
                        mem_write_32((addy+4), MSB);
                        break;
                    // STUR 32
                    case 4:
                        mem_write_32(addy, data&1073741823);
                        break;
                    // STURB
                    case 1:
                        mem_write_32(addy, data&255);
                        break;
                    // STURH
                    case 2:
                        mem_write_32(addy, data&65535);
                        break;
                }
            } else {
                printf("CacheD hit -- ldur (0x%X)\n", addy);
            }
            int j;
            ret.data = 0;
            for(j = 0; j < bytes; j++){
                ret.data += s.entries->data[offset+j]<<(j*8);
            }
            ret.hit = true;
            deleteNode(s.LRU, tag);
            enqueue(s.LRU, tag);
            return ret;
        }
    }

    // else it didn't find anything in cache
    ret.hit = false;
    printf("CacheD miss (0x%X)\n", addy);
    return ret;

}

void insertCacheD(cache_t *c, uint64_t addy, bool instrC, int bytes, bool isStur, uint64_t data){
    
    uint32_t index;
    uint64_t tag;
    uint32_t offset;
    uint64_t dMask = 8191;
    index = (addy>>5)&255;
    tag = (addy>>13)&(~(dMask<<51));
    offset = addy&31;
    set_t s = c->sets[index];
    if(!isFull(c, index)){
        //if set is not full
        int i;
        for(i = 0; i < c->numEntries; i++){
            if(s.entries[i].valid == 0){
                if(isStur){
                    printf("    CacheD Storing in Entry %u\n", i);
                    int j;
                    for(j = 0; j < bytes; j++){
                        s.entries[i].data[offset+j] = (data&(255<<(j*8)))>>(j*8);
                        printf("    byte %u = %u\n", j, s.entries[i].data[offset+j]);
                    }
                
                    enqueue(s.LRU, tag);
                    s.entries[i].valid = 1;
                    s.entries[i].tag = tag;
                    printf("    setting tag to %u\n", tag);
                    return; 
                }
                // load it into the cache
                int j;
                for(j = 0; j < 32; j = j+4){
                    uint32_t d = mem_read_32(addy&(~31)+j);
                    s.entries[i].data[j] = d&255;
                    s.entries[i].data[j+1] = (d&(255<<8))>>8;
                    s.entries[i].data[j+2] = (d&(255<<16))>>16;
                    s.entries[i].data[j+3] = (d&(255<<24))>>24;
                }
                enqueue(s.LRU, tag);
                s.entries[i].valid = 1;
                s.entries[i].tag = tag;
                return;
            }
        }
    }
    else{
        //if set is full, we take LRU out and put in new entry
        int lruTag = s.LRU->head->tag;
        dequeue(s.LRU);
        int i;
        for(i = 0; i < c->numEntries; i++){
            if(s.entries[i].tag == lruTag){
                if(isStur){
                    int j;
                    for(j = 0; j < bytes; j++){
                        s.entries[i].data[offset+j] = (data&(255<<(j*8)))>>(j*8);
                    }
                    enqueue(s.LRU, tag);
                    s.entries[i].valid = 1;
                    s.entries[i].tag = tag;
                    return;
                }
                int j;
                for(j = 0; j < 32; j = j+4){
                    uint32_t d = mem_read_32(addy&(~31)+j);
                    s.entries[i].data[j] = d&255;
                    s.entries[i].data[j+1] = (d&(255<<8))>>8;
                    s.entries[i].data[j+2] = (d&(255<<16))>>16;
                    s.entries[i].data[j+3] = (d&(255<<24))>>24;
                }
                enqueue(s.LRU, tag);
                s.entries[i].valid = 1;
                s.entries[i].tag = tag;
                return;
            }
        }
    }

}

void insertCacheI(cache_t *c, uint64_t addy){
    uint32_t index;
    uint64_t tag;
    uint32_t offset;
    uint64_t iMask = 2047;
    index = (addy>>5)&63;
    tag = (addy>>11)&(~(iMask<<53));
    offset = addy&31;
    set_t s = c->sets[index];
    if(!isFull(c, index)){
        int i;
        for(i = 0; i < c->numEntries; i++){
            if(s.entries[i].valid == 0){
                int j;
                for(j = 0; j < 32; j = j+4){
                    uint32_t d = mem_read_32((addy&(~31))+j);
                    //printf("  d = %u\n", d);
                    s.entries[i].data[j] = d&255;
                    //printf("  entry j = %u\n", s.entries[i].data[j]);
                    s.entries[i].data[j+1] = (d&(255<<8))>>8;
                    //printf("  entry j = %u\n", s.entries[i].data[j+1]);
                    s.entries[i].data[j+2] = (d&(255<<16))>>16;
                    //printf("  entry j = %u\n", s.entries[i].data[j+2]);
                    s.entries[i].data[j+3] = (d&(255<<24))>>24;
                    //printf("  entry j = %u\n", s.entries[i].data[j+3]);
                }
                enqueue(s.LRU, tag);
                s.entries[i].valid = 1;
                s.entries[i].tag = tag;
                return;
            }
        }
    }
    else{
        int lruTag = s.LRU->head->tag;
        dequeue(s.LRU);
        int i;
        for(i = 0; i < c->numEntries; i++){
            if(s.entries[i].tag == lruTag){
                int j;
                for(j = 0; j < 32; j = j+4){
                    uint32_t d = mem_read_32((addy&(~31))+j);
                    //printf("  d = %u\n", d);
                    s.entries[i].data[j] = d&255;
                    //printf("  entry j = %u\n", s.entries[i].data[j]);
                    s.entries[i].data[j+1] = (d&(255<<8))>>8;
                    //printf("  entry j = %u\n", s.entries[i].data[j+1]);
                    s.entries[i].data[j+2] = (d&(255<<16))>>16;
                    //printf("  entry j = %u\n", s.entries[i].data[j+2]);
                    s.entries[i].data[j+3] = (d&(255<<24))>>24;
                    //printf("  entry j = %u\n", s.entries[i].data[j+3]);
                }
                enqueue(s.LRU, tag);
                s.entries[i].valid = 1;
                s.entries[i].tag = tag;
                return;
            }
        }
    }

}

cache_t *cache_new(int sets, int ways, int block)
{
    cache_t *c = malloc(sizeof(cache_t));
    c->sets = malloc(sizeof(set_t)*sets);
    c->numSets = sets;
    c->numEntries = ways;
    for(int i = 0; i < sets; i++)
    {
        c->sets[i].entries = malloc(sizeof(entry_t)*ways);
        c->sets[i].LRU = makeLRU();
        for(int j = 0; j < ways; j++){
            c->sets[i].entries[j].valid = 0;
                c->sets[i].entries[j].data = malloc(sizeof(uint64_t)*(block));
        }
    }
    return c;
}

void freeLRU(node_t *h)
{
    if(h == NULL)
    {
        return;
    }
    node_t *tmp = h->next;
    free(h);
    freeLRU(tmp);
}

void cache_destroy(cache_t *c)
{
    for(int i = 0; i < c->numSets; i++)
    {
        for(int j = 0; j < c->numEntries; j++){
            free(c->sets[i].entries[j].data);
        }
        free(c->sets[i].entries);
        freeLRU(c->sets[i].LRU->head);
    }
    free(c->sets);
    free(c);
}

int cache_update(cache_t *c, uint64_t addr)
{
    uint32_t index;
    uint64_t tag;
    uint32_t offset;
    uint64_t iMask = 2047;
    index = (addr>>5)&63;
    tag = (addr>>11)&(~(iMask<<53));
    offset = addr&31;

    set_t s = c->sets[index];
/*
    printf("index: %u\n", index);
    printf("tag: %u\n", tag);
    printf("offset: %u\n", offset);
    printSet(c, index); */

    // check for a hit
    int ret = 0;
    int i;
    for(i = 0; i < c->numEntries; i++){
        if(s.entries[i].valid == 1 && s.entries[i].tag == tag){
            // printf("hit the cache\n");
            // printf("  entries:");
            int j;
            for(j = 0; j < 4; j++){
                //printf("  %i", s.entries->data[offset+j]<<(j*8));
                ret += s.entries->data[offset+j]<<(j*8);
            }
            //printf("\n");
            deleteNode(s.LRU, tag);
            enqueue(s.LRU, tag);
            //printf("    ret: %u\n", ret);
            printf("CacheI hit (0x%X)\n", addr);
            return ret;
        }
    }
    printf("CacheI miss (0x%X)\n", addr);
    //printf("returning ret = -1\n");
    return -1;
}

