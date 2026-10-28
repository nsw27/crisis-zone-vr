#pragma once
/* Gameplay-only directional selection. No repeat until a fresh neutral sample.
 * Context loss invalidates arming; never queue a switch across menus/reconnects. */
#include <stdbool.h>
#include <math.h>
typedef struct { bool armed; } QWeaponStick;
static inline int qweapon_stick_step(QWeaponStick *s, bool active, float x, float y) {
    if(!active || !isfinite(x) || !isfinite(y) || fabsf(x)>1.f || fabsf(y)>1.f) {
        s->armed=false;return 0;
    }
    float ax=fabsf(x),ay=fabsf(y),m=fmaxf(ax,ay);
    if(m<.25f){s->armed=true;return 0;}
    if(!s->armed || m<.65f)return 0;
    s->armed=false;
    /* Dominant axis; an exact diagonal tie uses X. */
    return (ax>=ay?x:y)>0.f?1:-1;
}
static inline int qweapon_cycle_index(int current,int step,int count) {
    if(count<1)return 0;
    if(current<0||current>=count)current=0;
    if(step>0)return current==count-1?0:current+1;
    if(step<0)return current?current-1:count-1;
    return current;
}
