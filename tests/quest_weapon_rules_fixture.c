/* Deterministic policy and guarded RAM bridge tests, not a headset playtest. */
#include "crisis_zone_weapons.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
unsigned char tc2_ram[0x1000000];
static void w32(unsigned a,unsigned v){a&=0xffffff;for(int i=0;i<4;i++)tc2_ram[a+i]=(unsigned char)(v>>(24-i*8));}
static unsigned r32(unsigned a){a&=0xffffff;return (unsigned)tc2_ram[a]<<24|(unsigned)tc2_ram[a+1]<<16|(unsigned)tc2_ram[a+2]<<8|tc2_ram[a+3];}
static QWeapons pistol(void){
    QWeapons s={0};qweapons_configure(&s,true,QWEAPON_HANDGUN);qweapons_new_game(&s);
    int n=0;assert(qweapons_switch(&s,40,&n)&&n==12);return s;
}
static void ready_press(QWeapons *s){qweapons_sample(s,true,true,0);qweapons_sample(s,true,true,1);}
int main(void){
    QWeapons s=pistol();assert(s.ammo[0]==40&&s.ammo[1]==12);
    qweapons_sample(&s,true,true,1);assert(!s.pending); /* release after switch */
    for(int i=0;i<12;i++){
        ready_press(&s);assert(s.pending&&qweapons_can_fire(&s));
        assert(qweapons_accepted(&s,11-i));
        assert(s.ammo[1]==11-i&&!s.pending&&s.shot_serial==(unsigned)i+1);
        for(int j=0;j<30;j++){qweapons_game_tick(&s);qweapons_sample(&s,true,true,1);assert(!s.pending);}
    }
    ready_press(&s);assert(!s.pending&&!qweapons_can_fire(&s));
    assert(qweapons_reload(&s)==12&&s.ammo[0]==40);qweapons_cancel(&s);
    /* Rejected/expired attempts do not consume ammo or produce feedback. */
    ready_press(&s);assert(s.pending);
    for(int i=0;i<QWEAPON_REQUEST_LIFETIME;i++)qweapons_game_tick(&s);
    assert(!s.pending&&s.ammo[1]==12&&s.shot_serial==12);
    ready_press(&s);assert(s.pending);qweapons_sample(&s,true,false,1);assert(!s.pending&&!s.active);
    qweapons_sample(&s,true,true,1);assert(!s.pending);
    ready_press(&s);assert(qweapons_accepted(&s,11));
    uint64_t deadline=s.next_shot[1];int n;
    qweapons_configure(&s,true,QWEAPON_MACHINE_GUN);assert(qweapons_switch(&s,11,&n)&&n==40);
    assert(s.ammo[1]==11&&s.next_shot[1]==deadline);
    qweapons_configure(&s,true,QWEAPON_HANDGUN);assert(qweapons_switch(&s,40,&n)&&n==11);
    ready_press(&s);assert(!s.pending); /* switching cannot bypass cooldown */
    while(s.tick<deadline)qweapons_game_tick(&s);
    qweapons_sample(&s,true,true,1);assert(!s.pending);ready_press(&s);assert(s.pending);
    float bad[]={NAN,INFINITY,-INFINITY,-1,2};
    for(unsigned i=0;i<sizeof bad/sizeof *bad;i++){
        ready_press(&s);qweapons_sample(&s,true,true,bad[i]);assert(!s.active&&!s.pending&&!s.armed);
        qweapons_sample(&s,true,true,1);assert(!s.pending);
    }
    ready_press(&s);qweapons_sample(&s,false,true,1);assert(!s.pending);
    qweapons_sample(&s,true,true,1);assert(!s.pending);
    ready_press(&s);qweapons_configure(&s,false,QWEAPON_HANDGUN);assert(!s.pending);
    assert(qweapons_switch(&s,11,&n)&&n==40);
    s=pistol();ready_press(&s);assert(!qweapons_accepted(&s,7)&&s.fault&&!s.pending);
    s=pistol();s.tick=UINT64_MAX;qweapons_game_tick(&s);assert(s.fault);

    /* Fixed ABI bridge: writes are restricted to the actual game's ammo word. */
    memset(tc2_ram,0,sizeof tc2_ram);w32(0x803ba3f4,0x80549844);w32(0x803ba3f8,0x805495a0);
    w32(0x80549848,40);w32(0x80549844,3);w32(0x8054984c,0xabcdef);
    csz_weapons_configure(false,false);csz_weapons_init(0x80549844);csz_weapons_begin(0x803b4e00);
    assert(csz_weapons_filter(15)==15&&r32(0x80549848)==40);
    csz_weapons_configure(true,true);csz_weapons_begin(0x803b4e00);assert(r32(0x80549848)==12);
    assert(r32(0x80549844)==3&&r32(0x8054984c)==0xabcdef); /* health/other stats untouched */
    assert(csz_weapons_handgun_active()&&csz_weapons_filter(15)==14);
    csz_weapons_sample(true,true,1);assert(!csz_weapons_pending());
    csz_weapons_sample(true,true,0);csz_weapons_sample(true,true,1);
    assert(csz_weapons_pending()&&csz_weapons_filter(15)==15);
    w32(0x80549848,11);csz_weapons_accepted(0x80549844);
    assert(csz_weapons_state()->ammo[1]==11&&csz_weapons_feedback()&&!csz_weapons_feedback());
    assert(csz_weapons_filter(15)==14&&r32(0x80549848)==11);
    csz_weapons_configure(true,false);csz_weapons_begin(0x803b4e00);assert(r32(0x80549848)==40);
    w32(0x80549848,39);csz_weapons_accepted(0x80549844);assert(csz_weapons_state()->ammo[0]==39);
    csz_weapons_configure(true,true);csz_weapons_begin(0x803b4e00);assert(r32(0x80549848)==11);
    /* The original reload write is intercepted, not arbitrary shield visibility. */
    w32(0x80549848,40);csz_weapons_reload(0x80549844);assert(r32(0x80549848)==12);
    csz_weapons_configure(true,false);csz_weapons_begin(0x803b4e00);assert(r32(0x80549848)==39);
    csz_weapons_configure(false,false);csz_weapons_begin(0x803b4e00);assert(r32(0x80549848)==39);
    assert(csz_weapons_filter(15)==15);
    /* No writes to unknown players or during a mismatched game context. */
    csz_weapons_configure(true,true);w32(0x803ba3f4,0);csz_weapons_begin(0x803b4e00);
    csz_weapons_reload(0x80549844);assert(r32(0x80549848)==39&&csz_weapons_filter(15)==15);
    csz_weapons_init(0x81234567);assert(!csz_weapons_handgun_active());
    /* A mismatched count stops custom firing; turning the feature off restores
     * the known machine-gun magazine instead of trapping the player in that mode. */
    w32(0x803ba3f4,0x80549844);csz_weapons_begin(0x803b4e00);
    assert(r32(0x80549848)==12);
    w32(0x80549848,8);csz_weapons_begin(0x803b4e00);assert(csz_weapons_state()->fault);
    csz_weapons_configure(false,false);csz_weapons_begin(0x803b4e00);
    assert(r32(0x80549848)==39&&!csz_weapons_state()->fault&&csz_weapons_filter(15)==15);
    puts("PASS: 12 confirmed shots, no held auto-fire, accepted-event accounting, separate magazines/cooldowns, guarded reload/reset, invalid-input recovery; no health/aim/cover writes");
}
