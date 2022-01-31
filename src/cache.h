/*
 * CMSC 22200, Fall 2016
 *
 * ARM pipeline timing simulator
 *
 */
#ifndef _CACHE_H_
#define _CACHE_H_

#include <stdint.h>
#include "stdbool.h"
#include "shell.h"

//for cache return
typedef struct cacheReturn_t
{
    uint64_t data;
    bool hit; //true if hit, false if miss
} cacheReturn_t;

// for LRU
typedef struct node_t
{
    int tag;
    struct node_t *next;
} node_t;

typedef struct lru_list_t
{
    node_t *head;
    node_t *tail;
} lru_list_t;

//for cache
typedef struct entry_t
{
    int valid;
    uint64_t tag;
    uint64_t *data;

} entry_t;

typedef struct set_t
{
    lru_list_t *LRU;
    entry_t *entries;

} set_t;

typedef struct
{
    int numSets;
    int numEntries;
    set_t *sets;

} cache_t;

void insertCacheD(cache_t *c, uint64_t addy, bool instrC, int bytes, bool isStur, uint64_t data);
void insertCacheI(cache_t *c, uint64_t addy);
cacheReturn_t cache_checkLoad(cache_t *c, uint64_t addy, bool instrC, int bytes, bool isStur, uint64_t data);
cache_t *cache_new(int sets, int ways, int block);
void cache_destroy(cache_t *c);
int cache_update(cache_t *c, uint64_t addr);

#endif
