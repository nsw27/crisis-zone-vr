#include "quest_support_grip.h"
#include "quest_options.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static float length(V3 v){return sqrtf(dot(v,v));}
int main(void){
    for(int hand=-1;hand<=1;hand+=2){
        QSupportGrip s={0};Q4 q={0,0,0,1};V3 p=v3(hand*.25f,0,-.4f),other=add(p,v3(.03f,-.04f,-.3f));
        assert(!qsupport_update(&s,true,true,1,p,other,&q)); /* release first */
        assert(!qsupport_update(&s,true,true,0,p,other,&q));
        assert(qsupport_update(&s,true,true,1,p,other,&q));
        V3 wanted=mul(v3(.03f,0,-.3f),1.f/length(v3(.03f,0,-.3f)));
        assert(length(sub(rotate(q,v3(0,0,-1)),wanted))<1e-5f);
        q=(Q4){0,0,0,1};assert(!qsupport_update(&s,true,false,1,p,other,&q));
        assert(!qsupport_update(&s,true,true,1,p,other,&q));
        qsupport_update(&s,true,true,0,p,other,&q);assert(qsupport_update(&s,true,true,1,p,other,&q));
        assert(!qsupport_update(&s,true,true,1,p,add(p,v3(0,0,.3f)),&q));
        q=(Q4){0,0,0,1};qsupport_update(&s,true,true,0,p,other,&q);
        assert(!qsupport_update(&s,true,true,1,p,add(p,v3(0,0,-.01f)),&q));
        qsupport_update(&s,true,true,0,p,other,&q);assert(!qsupport_update(&s,true,true,1,p,add(p,v3(0,0,-2)),&q));
        qsupport_update(&s,true,true,0,p,other,&q);assert(!qsupport_update(&s,true,true,NAN,p,other,&q));
        qsupport_update(&s,true,true,0,p,other,&q);assert(!qsupport_update(&s,true,true,1,p,v3(INFINITY,0,0),&q));
        assert(!qsupport_update(&s,false,true,1,p,other,&q));
    }
    QCoverButton b={0};assert(qcover_button(&b,.8f,true,false)==.8f);
    assert(qcover_button(&b,1,true,true)==0);assert(qcover_button(&b,0,true,true)==0);
    assert(qcover_button(&b,1,true,true)==1);assert(qcover_button(&b,1,true,true)==1);
    assert(qcover_button(&b,0,true,true)==1);assert(qcover_button(&b,1,true,true)==0);
    qcover_button(&b,0,true,true);assert(qcover_button(&b,1,true,true)==1);
    assert(qcover_button(&b,1,false,true)==0);assert(qcover_button(&b,1,true,true)==0);
    assert(qcover_button(&b,NAN,true,true)==0);
    QOptions o;qoptions_load("missing.cfg",&o);assert(!o.two_hand&&!o.cover_toggle&&!o.depth_correction&&o.scene_density==0);
    o.two_hand=o.cover_toggle=o.depth_correction=true;o.scene_density=2;o.scene_trace=true;
    assert(qoptions_save("roundtrip.cfg",&o));qoptions_load("roundtrip.cfg",&o);
    assert(o.two_hand&&o.cover_toggle&&o.depth_correction&&o.scene_density==2&&!o.scene_trace);
    FILE *f=fopen("bad.cfg","w");assert(f);fputs("scene_density=-1\ntwo_hand=2\ncover_toggle=1junk\ndepth_correction=1\n",f);fclose(f);
    qoptions_load("bad.cfg",&o);assert(o.scene_density==0&&!o.two_hand&&!o.cover_toggle&&o.depth_correction);
    puts("PASS: two-hand alignment in both hands, acquisition/release/deadzone/crossing/invalid tracking; toggle-cover safety and option persistence");
}
