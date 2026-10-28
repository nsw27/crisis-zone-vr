/* Version-specific bridge for Crisis Zone World CSZO4 Ver. B.
 * See docs/EXPANDED_WEAPONS_RESEARCH.md for verified instruction sites.
 * Shotgun offsets only the transient native hit rays; original aim, shield,
 * enemy damage handlers and CPU scheduling remain authoritative. */
#include "crisis_zone_weapons.h"
#include <stdio.h>
#include "quest_pellet_burst.h"
extern unsigned char tc2_ram[0x1000000];
/* Physical addresses; restrict all writes to the single documented ammo word. */
enum { PLAYER=0x549844, PLAYER_OWNER=0x5495a0, GP=0x3b4e00,
       PLAYER_POINTER=GP+0x55f4, OWNER_POINTER=GP+0x55f8 };
static QWeapons weapons;
static bool context_valid,enabled_previous;
static uint64_t feedback_serial;
static QPelletBurst pellets;
static bool hit_owned,hit_active;
static int32_t hit_x,hit_y;
static uint32_t read32(uint32_t p){
    return (uint32_t)tc2_ram[p]<<24|(uint32_t)tc2_ram[p+1]<<16|
           (uint32_t)tc2_ram[p+2]<<8|tc2_ram[p+3];
}
static void ammo_write(uint32_t value){
    const uint32_t p=PLAYER+4;
    tc2_ram[p]=(unsigned char)(value>>24);tc2_ram[p+1]=(unsigned char)(value>>16);
    tc2_ram[p+2]=(unsigned char)(value>>8);tc2_ram[p+3]=(unsigned char)value;
}
static bool real_player(uint32_t p){return p==0x80549844u;}
static bool live_context(void){
    /* 0x40 selects the alternate recorded/demo path; 0x02 bypasses live fire.
     * Neither should consume a custom weapon's queued press or magazine. */
    return read32(PLAYER_POINTER)==0x80549844u&&read32(OWNER_POINTER)==0x805495a0u&&
           !(read32(PLAYER+0x30)&0x42);
}
void csz_weapons_select(bool enabled,QWeapon weapon){
    if(weapons.enabled!=enabled||weapons.requested!=weapon)qpellets_cancel(&pellets);
    qweapons_configure(&weapons,enabled,weapon);
}
void csz_weapons_configure(bool enabled,bool handgun){
    csz_weapons_select(enabled,handgun?QWEAPON_HANDGUN:QWEAPON_MACHINE_GUN);
}
void csz_weapons_sample(bool active,bool exposed,float trigger){
    qweapons_sample(&weapons,active,exposed,trigger);
    if(!weapons.active||!weapons.exposed)qpellets_cancel(&pellets);
}
void csz_weapons_cancel(void){qweapons_cancel(&weapons);qpellets_cancel(&pellets);hit_owned=hit_active=false;feedback_serial=weapons.shot_serial;}
bool csz_weapons_selection_ready(void){
    return weapons.enabled&&weapons.ready&&!weapons.fault&&context_valid&&live_context();
}
bool csz_weapons_handgun_active(void){
    return weapons.enabled&&weapons.ready&&context_valid&&live_context()&&weapons.selected==QWEAPON_HANDGUN;
}
bool csz_weapons_single_active(void){
    return weapons.enabled&&weapons.ready&&context_valid&&live_context()&&weapons.selected!=QWEAPON_MACHINE_GUN;
}
bool csz_weapons_pending(void){return csz_weapons_single_active()&&qweapons_can_fire(&weapons);}
const QWeapons *csz_weapons_state(void){return &weapons;}
bool csz_weapons_feedback(void){
    bool changed=feedback_serial!=weapons.shot_serial;feedback_serial=weapons.shot_serial;
    return changed&&csz_weapons_single_active()&&!weapons.fault;
}
void csz_weapons_init(uint32_t player){
    if(!real_player(player))return;
    qpellets_cancel(&pellets);hit_owned=hit_active=false;
    qweapons_new_game(&weapons);context_valid=false;enabled_previous=false;feedback_serial=0;
}
void csz_weapons_reset(uint32_t player){
    /* The original game uses this on restart/continue; preserve its own other
     * player fields. Our reset is at the ammo store, not an invented reload. */
    if(!real_player(player))return;
    qpellets_cancel(&pellets);hit_owned=hit_active=false;
    bool apply=weapons.enabled&&context_valid&&live_context();QWeapon selected=weapons.selected;
    qweapons_new_game(&weapons);weapons.selected=apply?selected:QWEAPON_MACHINE_GUN;
    if(apply)ammo_write((uint32_t)weapons.ammo[weapons.selected]);
    else {context_valid=false;enabled_previous=false;}
    feedback_serial=0;
}
void csz_weapons_reload(uint32_t player){
    if(!weapons.enabled||!weapons.ready||!real_player(player)||!context_valid||!live_context()||weapons.fault)return;
    qpellets_cancel(&pellets);
    ammo_write((uint32_t)qweapons_reload(&weapons));
}
void csz_weapons_begin(uint32_t gp){
    context_valid=(gp==0x803b4e00u)&&live_context();
    if(!context_valid){csz_weapons_cancel();return;}
    if(!weapons.ready)return;
    if((int32_t)read32(PLAYER)<=0||(read32(PLAYER+0x30)&0x4e))qpellets_cancel(&pellets);
    int game_ammo=(int32_t)read32(PLAYER+4);
    if(!weapons.enabled&&!enabled_previous){
        /* Entire original path is read-only when the feature is off. */
        weapons.selected=QWEAPON_MACHINE_GUN;weapons.ammo[0]=game_ammo;
        qweapons_game_tick(&weapons);return;
    }
    if(!weapons.enabled&&enabled_previous){
        /* Even a guarded custom-mode failure must have a safe way back to the
         * original weapon. Restore its last known magazine, not a free refill. */
        if(weapons.selected!=QWEAPON_MACHINE_GUN){
            if(game_ammo>=0&&game_ammo<=qweapons_capacity(weapons.selected))weapons.ammo[weapons.selected]=game_ammo;
            int saved=weapons.ammo[0];if(saved<0||saved>40)saved=0;
            ammo_write((uint32_t)saved);
        }
        weapons.selected=QWEAPON_MACHINE_GUN;weapons.fault=false;
        enabled_previous=false;csz_weapons_cancel();qweapons_game_tick(&weapons);return;
    }
    if(weapons.enabled&&!enabled_previous){
        weapons.selected=QWEAPON_MACHINE_GUN;weapons.ammo[0]=game_ammo;
    }
    int replacement;
    if(qweapons_switch(&weapons,game_ammo,&replacement)){qpellets_cancel(&pellets);ammo_write((uint32_t)replacement);}
    else if(game_ammo!=weapons.ammo[weapons.selected]){
        /* Unknown writes are not silently treated as accepted shots/reloads. */
        if(!weapons.fault)fprintf(stderr,"[WEAPONS] unrecognised ammunition transition; custom fire disabled\n");
        weapons.fault=true;csz_weapons_cancel();
    }
    enabled_previous=weapons.enabled;qweapons_game_tick(&weapons);
}
uint32_t csz_weapons_filter(uint32_t eligibility){
    if(!weapons.enabled||!weapons.ready||!context_valid||!live_context())return eligibility;
    if(weapons.selected!=QWEAPON_MACHINE_GUN&&!qweapons_can_fire(&weapons))return eligibility&~1u;
    return eligibility;
}
void csz_weapons_accepted(uint32_t player){
    if(!weapons.enabled||!weapons.ready||!context_valid||!live_context()||!real_player(player))return;
    bool accepted=qweapons_accepted(&weapons,(int32_t)read32(PLAYER+4));
    if(accepted&&weapons.selected==QWEAPON_SHOTGUN){
        qpellets_cancel(&pellets);
        /* An accepted discharge can still be an off-screen/non-damaging shot.
         * Queue only the game's own live hit classification, never a trigger. */
        if((read32(GP+0x55fc)&6)==6)
            qpellets_start(&pellets,weapons.tick,(int32_t)read32(GP+0x532c),(int32_t)read32(GP+0x5330));
    }
}

/* Called after the original hit-dispatch routine's pause/death/context gates.
 * Only native projection registers are changed. The global aim/cursor pair and
 * the original enemy collision/damage implementation are not overwritten. */
void csz_weapons_hit_begin(void){
    hit_owned=hit_active=false;
    if(!csz_weapons_single_active()||weapons.selected!=QWEAPON_SHOTGUN)return;
    hit_owned=true;
    if(weapons.fault||!weapons.active||!weapons.exposed||
       (int32_t)read32(PLAYER)<=0||(read32(PLAYER+0x30)&0x4e)){
        qpellets_cancel(&pellets);return;
    }
    if(!qpellets_point(&pellets,weapons.tick,&hit_x,&hit_y))return;
    if(!qpellets_inside(hit_x,hit_y)){qpellets_advance(&pellets,weapons.tick);return;}
    hit_active=true;
}
int32_t csz_weapons_hit_axis(uint32_t caller,int axis,int32_t original){
    if(!hit_active||caller!=0x800950b4u)return original;
    return axis==0?hit_x:axis==1?hit_y:original;
}
uint32_t csz_weapons_hit_flags(uint32_t original){
    if(!hit_owned)return original;
    return (original&~6u)|(hit_active?6u:2u);
}
void csz_weapons_hit_complete(void){
    if(hit_active)qpellets_advance(&pellets,weapons.tick);
    hit_active=false;
}
void csz_weapons_hit_end(void){hit_owned=hit_active=false;}
