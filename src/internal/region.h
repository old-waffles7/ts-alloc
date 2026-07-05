
#pragma once
#ifndef REGION_H
#define REGION_H


#include    "mutex.h"


struct region_node 
{
    struct region_node *lchild;
    struct region_node *next;
    struct region_node *prev;
};

typedef struct region_node  reg_node_t;


struct region_heap
{
    void   *root;
    mutex_t mutex;
};

typedef struct region_heap  reg_heap_t;


struct region
{
    reg_node_t  heap_node;
    size_t      nbytes;
};

typedef struct region   region_t;


#endif  //REGION_H