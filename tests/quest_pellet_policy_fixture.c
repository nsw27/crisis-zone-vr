#include "quest_pellet_burst.h"
#include "quest_weapon_rules.h"
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
int main(void){
    QPelletBurst p={0};int32_t x=99,y=99;
    assert(!qpellets_point(&p,0,&x,&y));
    qpellets_start(&p,10,0,0);assert(qpellets_point(&p,10,&x,&y)&&x==0&&y==10);
    qpellets_advance(&p,10);assert(!qpellets_point(&p,10,&x,&y)&&!qpellets_point(&p,11,&x,&y));
    assert(qpellets_point(&p,12,&x,&y)&&x==-9&&y==-5);qpellets_advance(&p,12);
    assert(!qpellets_point(&p,13,&x,&y));assert(qpellets_point(&p,14,&x,&y)&&x==9&&y==-5);
    qpellets_advance(&p,14);assert(!qpellets_point(&p,15,&x,&y)&&!p.pending);
    /* Late dispatch must not catch up with multiple pellets on one update. */
    qpellets_start(&p,20,1,2);assert(qpellets_point(&p,23,&x,&y));qpellets_advance(&p,23);
    assert(!qpellets_point(&p,23,&x,&y)&&!qpellets_point(&p,24,&x,&y));
    assert(qpellets_point(&p,25,&x,&y));qpellets_advance(&p,25);
    assert(qpellets_point(&p,27,&x,&y));qpellets_advance(&p,27);assert(!p.pending);
    qpellets_start(&p,20,0,0);assert(!qpellets_point(&p,28,&x,&y)&&!p.pending);
    qpellets_start(&p,20,0,0);assert(!qpellets_point(&p,19,&x,&y)&&!p.pending);
    qpellets_start(&p,UINT64_MAX,0,0);assert(!p.pending);
    qpellets_start(&p,20,INT32_MAX,0);assert(!p.pending);
    qpellets_start(&p,20,0,INT32_MIN);assert(!p.pending);
    qpellets_start(&p,20,0,0);p.next=3;assert(!qpellets_point(&p,20,&x,&y));
    assert(qpellets_inside(-320,-239)&&qpellets_inside(319,240));
    assert(!qpellets_inside(-321,0)&&!qpellets_inside(320,0));
    /* Three independent magazines/cooldowns; the shotgun cannot be rearmed by
     * switching away/back, waiting while held, pausing or a rejected request. */
    QWeapons s={0};int ammo;
    qweapons_configure(&s,true,QWEAPON_SHOTGUN);qweapons_new_game(&s);
    assert(qweapons_switch(&s,40,&ammo)&&ammo==3);
    qweapons_sample(&s,true,true,0);qweapons_sample(&s,true,true,1);
    assert(qweapons_can_fire(&s)&&qweapons_accepted(&s,2)&&s.shot_serial==1);
    qweapons_configure(&s,true,QWEAPON_HANDGUN);assert(qweapons_switch(&s,2,&ammo)&&ammo==12);
    qweapons_configure(&s,true,QWEAPON_SHOTGUN);assert(qweapons_switch(&s,12,&ammo)&&ammo==2);
    qweapons_sample(&s,true,true,0);qweapons_sample(&s,true,true,1);assert(!s.pending);
    for(int i=0;i<30;i++){qweapons_game_tick(&s);qweapons_sample(&s,true,true,1);assert(!s.pending);}
    qweapons_sample(&s,true,true,0);qweapons_sample(&s,true,true,1);
    assert(qweapons_can_fire(&s)&&qweapons_accepted(&s,1));
    for(int i=0;i<30;i++)qweapons_game_tick(&s);
    qweapons_sample(&s,true,true,0);qweapons_sample(&s,true,true,1);
    assert(qweapons_accepted(&s,0)&&s.shot_serial==3);
    for(int i=0;i<30;i++)qweapons_game_tick(&s);
    qweapons_sample(&s,true,true,0);qweapons_sample(&s,true,true,1);assert(!qweapons_can_fire(&s));
    assert(qweapons_reload(&s)==3&&s.ammo[0]==40&&s.ammo[1]==12);
    qweapons_sample(&s,true,true,0);qweapons_sample(&s,true,true,1);assert(s.pending);
    qweapons_sample(&s,true,false,1);assert(!s.pending);
    qweapons_sample(&s,true,true,1);assert(!s.pending);
    qweapons_sample(&s,true,true,NAN);assert(!s.active&&!s.armed);
    puts("PASS: triangle pattern, delayed dispatch spacing, deadline/overflow/edge rejection, three-shell independent magazine/cooldown, no switch refill or held-trigger recovery");
}
