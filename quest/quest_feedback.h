#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
typedef struct {bool active;uint64_t last;} QFeedback;
/* Zero: idle; one: kick; minus one: cancel. Sustained bursts retain feedback,
 * but no timer may create a shot while the permitted firing state is false. */
static inline int qfeedback_tick(QFeedback *s,bool allowed,uint64_t now){
    if(!allowed){bool was=s->active;*s=(QFeedback){0};return was?-1:0;}
    if(!s->active||now<s->last||now-s->last>=100){s->active=true;s->last=now;return 1;}
    return 0;
}
static inline uint16_t qgun_register(float value,unsigned base,unsigned span){
    if(!isfinite(value))value=.5f;
    value=fmaxf(0,fminf(1,value));
    double result=base+(double)value*span;
    if(result>65535)result=65535;
    return (uint16_t)result;
}
