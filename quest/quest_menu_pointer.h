#pragma once
#include "vr_math.h"
#include "quest_options.h"
/* Geometry shared by pointer hit tests and the visible options panel. */
static inline int qmenu_row_y(int page,int row){return 94+row*(qoptions_row_count(page)==7?30:36);}
typedef struct { bool armed;int hand; } QMenuClick;
static inline bool qmenu_finite(V3 p){return isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z);}
static inline bool qmenu_project(V3 head,Q4 rotation,V3 origin,V3 direction,float *x,float *y){
    if(!qmenu_finite(head)||!qmenu_finite(origin)||!qmenu_finite(direction)||
       !isfinite(rotation.x)||!isfinite(rotation.y)||!isfinite(rotation.z)||!isfinite(rotation.w))return false;
    float norm=rotation.x*rotation.x+rotation.y*rotation.y+rotation.z*rotation.z+rotation.w*rotation.w;
    if(!isfinite(norm)||norm<.99f||norm>1.01f)return false;
    V3 o=rotate(conjugate(rotation),sub(origin,head)),d=rotate(conjugate(rotation),direction);
    if(d.z>=-1e-5f)return false;
    float t=(-1.6f-o.z)/d.z;if(!isfinite(t)||t<=0)return false;
    V3 hit=add(o,mul(d,t));
    float px=(hit.x+.65f)*(640.f/1.3f),py=(.4875f-hit.y)*(480.f/.975f);
    if(!isfinite(px)||!isfinite(py)||px<0||px>=640||py<0||py>=480)return false;
    *x=px;*y=py;return true;
}
/* Consume a fresh trigger press even outside a button. No hold-to-repeat and
 * no press carried through opening, reconnecting, losing tracking or handoff. */
static inline bool qmenu_click(QMenuClick *state,bool active,int hand,float trigger){
    if(!active||!isfinite(trigger)){*state=(QMenuClick){.hand=-1};return false;}
    if(state->hand!=hand){state->armed=false;state->hand=hand;}
    if(trigger<.2f){state->armed=true;return false;}
    if(trigger>.55f&&state->armed){state->armed=false;return true;}
    return false;
}
/* -1 means panel background. +/- buttons have explicit hit boxes, so clicking
 * a label selects it without silently modifying the value. */
static inline int qmenu_row_at(int page,float x,float y,int *direction){
    *direction=0;if(x<8||x>=632)return -1;
    for(int row=0;row<qoptions_row_count(page);row++){
        int top=qmenu_row_y(page,row)-4;
        if(y>=top&&y<top+28){
            if(x>=528&&x<568)*direction=-1;
            else if(x>=584&&x<624)*direction=1;
            return row;
        }
    }
    return -1;
}
static inline int qmenu_page_at(float x,float y){
    if(y<42||y>=76)return 0;
    if(x>=16&&x<56)return -1;
    if(x>=584&&x<624)return 1;
    return 0;
}
