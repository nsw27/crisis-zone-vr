#include "quest_weapon_stick.h"
#include "quest_options.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
int main(void){
    QWeaponStick s={0};
    const float directions[8][2]={{1,0},{0,1},{-1,0},{0,-1},{.8f,.6f},{-.8f,.6f},{.6f,-.8f},{-.6f,.8f}};
    const int expected[]={1,1,-1,-1,1,-1,-1,1};
    for(int i=0;i<8;i++){
        s=(QWeaponStick){0};float x=directions[i][0],y=directions[i][1];
        assert(qweapon_stick_step(&s,true,x,y)==0);
        assert(qweapon_stick_step(&s,true,0,0)==0);
        assert(qweapon_stick_step(&s,true,x,y)==expected[i]);
        for(int n=0;n<120;n++)assert(qweapon_stick_step(&s,true,x,y)==0);
        /* Crossing to the opposite direction without neutral cannot repeat. */
        assert(qweapon_stick_step(&s,true,-x,-y)==0);
        qweapon_stick_step(&s,true,0,0);assert(qweapon_stick_step(&s,true,-x,-y)==-expected[i]);
    }
    qweapon_stick_step(&s,true,0,0);assert(!qweapon_stick_step(&s,true,.64f,0));
    assert(qweapon_stick_step(&s,true,.65f,0)==1);
    assert(!qweapon_stick_step(&s,true,.25f,0));assert(!qweapon_stick_step(&s,true,1,0));
    qweapon_stick_step(&s,true,.24f,0);assert(qweapon_stick_step(&s,true,1,0)==1);
    const float bad[]={NAN,INFINITY,-INFINITY,1.01f,-1.01f};
    for(unsigned i=0;i<sizeof bad/sizeof *bad;i++){
        qweapon_stick_step(&s,true,0,0);assert(!qweapon_stick_step(&s,true,bad[i],0));
        assert(!qweapon_stick_step(&s,true,1,0));
    }
    qweapon_stick_step(&s,true,0,0);assert(!qweapon_stick_step(&s,false,0,0));
    assert(!qweapon_stick_step(&s,true,1,0));
    for(int count=1;count<=3;count++)for(int n=0;n<count;n++){
        assert(qweapon_cycle_index(qweapon_cycle_index(n,1,count),-1,count)==n);
    }
    QOptions o;qoptions_load("nonexistent-settings.cfg",&o);assert(o.expanded_weapons);
    FILE *f=fopen("older.cfg","w");assert(f);fputs("machine_gun=1\n",f);fclose(f);
    qoptions_load("older.cfg",&o);assert(o.expanded_weapons);
    o.expanded_weapons=false;assert(qoptions_save("saved.cfg",&o));
    qoptions_load("saved.cfg",&o);assert(!o.expanded_weapons);
    o.expanded_weapons=true;assert(qoptions_save("saved.cfg",&o));qoptions_load("saved.cfg",&o);assert(o.expanded_weapons);
    puts("PASS: four-way neutral/rearm selection, held/diagonal/deadzone/invalid input, cycle wrapping, defaults and saved OFF");
}
