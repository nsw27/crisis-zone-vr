#pragma once
/* Three normal hit rays per accepted shell, one per distinct gameplay update.
 * This policy has no game-memory writes, trigger injection or damage multiplier.
 * Off-screen points are discarded, never clamped into extra edge hits. */
#include <stdbool.h>
#include <stdint.h>
enum { QPELLET_COUNT=3, QPELLET_INTERVAL=2, QPELLET_LIFETIME=8 };
typedef struct {
    bool pending;
    int next;
    int32_t x,y;
    uint64_t started,due;
} QPelletBurst;
static inline void qpellets_cancel(QPelletBurst *b){*b=(QPelletBurst){0};}
static inline bool qpellets_inside(int32_t x,int32_t y){
    return x>=-320&&x<=319&&y>=-239&&y<=240;
}
static inline void qpellets_start(QPelletBurst *b,uint64_t tick,int32_t x,int32_t y){
    qpellets_cancel(b);
    if(tick<=UINT64_MAX-QPELLET_LIFETIME&&qpellets_inside(x,y))*b=(QPelletBurst){.pending=true,.x=x,.y=y,.started=tick,.due=tick};
}
static inline bool qpellets_point(QPelletBurst *b,uint64_t tick,int32_t *x,int32_t *y){
    static const int dx[3]={0,-9,9},dy[3]={10,-5,-5};
    if(!b->pending)return false;
    if(tick<b->started||tick-b->started>=QPELLET_LIFETIME||b->next<0||b->next>=3){
        qpellets_cancel(b);return false;
    }
    if(tick<b->due)return false;
    *x=b->x+dx[b->next];*y=b->y+dy[b->next];
    return true;
}
static inline void qpellets_advance(QPelletBurst *b,uint64_t tick){
    if(b->pending){if(++b->next>=QPELLET_COUNT)b->pending=false;else b->due=tick+QPELLET_INTERVAL;}
}
