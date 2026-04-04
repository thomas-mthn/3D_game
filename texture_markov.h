#ifndef MARKOV_H
#define MARKOV_H

#include "langext.h"

structure(Texture);

static int offsets[][2] = {
    {-1, 0},
    { 1, 0},
    { 0,-1},
    { 0, 1},
    
    {-1,-1},    
    {-1, 1},
    { 1,-1},
    { 1, 1},
};

static int offsets_up[][2] = {
    { 0, -1},
    { 1, 0},
    { 0, 1},
    { -1, 0},

    {-1, 0},
    { 1, 0},
    { 0,-1},
    { 0, 1},
};

#define DIMENSIONS ((((countof(offsets) + countof(offsets_up)) * 3) + 3) & ~3)

structure(GraphNode){
    uint8 position[DIMENSIONS];
    int neighbours[0x10];
    int next;
    struct{
        int n_sample;
        int color;
    } sample[8];
    unsigned hash;
};

structure(Graph){
    int n_search_inference;
    int n_candidates_inference;
    int n_neighbours;
    int n_difference_threshold;

    int n_nodes;

    int weights[DIMENSIONS];
    int weights_inverse[DIMENSIONS];
    GraphNode* entry;

    int color_randindex[4096];
    int n_color_distribution[4096];
    int n_color_randrange;
};

void markovTextureTrain(Texture texture);
void markovTextureRemix(Texture texture,int remix_mipmap);
void markovTextureGenerate(Texture texture,Texture sampler);
void markovInit(void);
void markovFree(void);

#endif
