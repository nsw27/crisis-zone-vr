#pragma once
#include "vr_math.h"
/* Optional off-hand support. No temporal smoothing and no target snapping.
 * A fresh squeeze near the foregrip acquires support; release, crossed hands,
 * excessive reach, tracking/focus loss, pause or hand change release it. */
typedef struct { bool armed,held; } QSupportGrip;
static inline bool qsupport_finite(V3 p){return isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z);}
static inline bool qsupport_update(QSupportGrip *s,bool enabled,bool available,float grip,V3 primary,V3 other,Q4 *rotation){
    if(!enabled||!available||!isfinite(grip)||!qsupport_finite(primary)||!qsupport_finite(other)||
       !isfinite(rotation->x)||!isfinite(rotation->y)||!isfinite(rotation->z)||!isfinite(rotation->w)){
        *s=(QSupportGrip){0};return false;
    }
    if(grip<.2f){s->armed=true;s->held=false;return false;}
    float norm=sqrtf(rotation->x*rotation->x+rotation->y*rotation->y+rotation->z*rotation->z+rotation->w*rotation->w);
    if(!isfinite(norm)||norm<1e-6f){*s=(QSupportGrip){0};return false;}
    Q4 q={rotation->x/norm,rotation->y/norm,rotation->z/norm,rotation->w/norm};
    V3 forward=rotate(q,v3(0,0,-1));
    /* The support hand rests beneath the barrel, not on the optical axis. */
    V3 delta=sub(add(other,rotate(q,v3(0,.04f,0))),primary);
    float distance=sqrtf(dot(delta,delta));
    if(!isfinite(distance)||distance<.15f||distance>.75f){s->held=false;s->armed=false;return false;}
    V3 aim=mul(delta,1.f/distance);float alignment=dot(forward,aim);
    if(!s->held){
        if(grip<=.55f||!s->armed)return false;
        s->armed=false;
        /* Do not capture the gun when the off-hand is behind or far to its side. */
        if(alignment<.90f||distance>.50f)return false;
        s->held=true;
    }
    if(alignment<.2f){s->held=false;s->armed=false;return false;}
    V3 axis=cross(forward,aim);Q4 swing={axis.x,axis.y,axis.z,1.f+alignment};
    float length=sqrtf(dot(axis,axis)+swing.w*swing.w);
    if(!isfinite(length)||length<1e-6f){*s=(QSupportGrip){0};return false;}
    swing=(Q4){swing.x/length,swing.y/length,swing.z/length,swing.w/length};
    *rotation=product(swing,q);return true;
}
/* Grip-toggle accessibility retains the arcade pedal/reload mechanic.
 * Hold mode preserves the original analog value; toggle mode requires fresh
 * release/repress after invalid input and never carries exposure across pause. */
typedef struct { bool armed,exposed,toggle; } QCoverButton;
static inline float qcover_button(QCoverButton *s,float value,bool available,bool toggle){
    if(!available||!isfinite(value)){*s=(QCoverButton){.toggle=toggle};return 0;}
    if(s->toggle!=toggle)*s=(QCoverButton){.toggle=toggle};
    if(!toggle){s->exposed=false;s->armed=false;return fmaxf(0,fminf(1,value));}
    if(value<.2f)s->armed=true;
    else if(value>.55f&&s->armed){s->armed=false;s->exposed=!s->exposed;}
    return s->exposed?1.f:0.f;
}
