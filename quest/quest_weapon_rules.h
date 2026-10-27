#pragma once
/* Expanded weapons policy. No graphics, device I/O, wall-clock or game RAM.
 * Ammunition changes ONLY on game-confirmed shots/reloads, never trigger pulls.
 * Single-thread ownership: the simulation and XR host use the same thread. */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef enum { QWEAPON_MACHINE_GUN=0, QWEAPON_HANDGUN=1, QWEAPON_COUNT=2 } QWeapon;
enum { QWEAPON_HANDGUN_COOLDOWN=10, QWEAPON_REQUEST_LIFETIME=8 };
typedef struct {
    bool enabled, ready, active, exposed, armed, pending, fault;
    QWeapon selected, requested;
    int ammo[QWEAPON_COUNT];
    uint64_t tick, expires, next_shot[QWEAPON_COUNT], shot_serial;
} QWeapons;
static inline int qweapons_capacity(QWeapon w) { return w==QWEAPON_HANDGUN?12:40; }
static inline void qweapons_cancel(QWeapons *s) {
    s->active=s->exposed=s->armed=s->pending=false;
}
static inline void qweapons_new_game(QWeapons *s) {
    bool enabled=s->enabled;QWeapon requested=s->requested;
    memset(s,0,sizeof *s);s->enabled=enabled;s->requested=requested;
    s->selected=QWEAPON_MACHINE_GUN;s->ammo[0]=40;s->ammo[1]=12;s->ready=true;
}
static inline void qweapons_configure(QWeapons *s,bool enabled,QWeapon requested) {
    if(requested<0||requested>=QWEAPON_COUNT)requested=QWEAPON_MACHINE_GUN;
    if(s->enabled!=enabled||s->requested!=requested)qweapons_cancel(s);
    s->enabled=enabled;s->requested=requested;
}
/* A switch is a transaction at the game's update boundary. Preserve each
 * magazine and each cooldown; switching is never a reload. */
static inline bool qweapons_switch(QWeapons *s,int game_ammo,int *replacement) {
    QWeapon next=s->enabled?s->requested:QWEAPON_MACHINE_GUN;
    if(!s->ready||s->fault||next==s->selected)return false;
    if(game_ammo<0||game_ammo>qweapons_capacity(s->selected)){
        s->fault=true;qweapons_cancel(s);return false;
    }
    s->ammo[s->selected]=game_ammo;s->selected=next;
    *replacement=s->ammo[next];s->armed=s->pending=false;
    return true;
}
static inline void qweapons_sample(QWeapons *s,bool active,bool exposed,float level) {
    /* NaN also fails this range check. Inactive/cover/reconnect require release. */
    if(!s->enabled||!s->ready||s->fault||!active||!exposed||!(level>=0&&level<=1)){
        qweapons_cancel(s);return;
    }
    s->active=true;s->exposed=true;
    if(level<.2f){s->armed=true;return;}
    if(level>.55f&&s->armed){
        s->armed=false;
        if(s->selected==QWEAPON_HANDGUN&&!s->pending&&s->ammo[s->selected]>0&&
           s->tick>=s->next_shot[s->selected]){
            s->pending=true;s->expires=s->tick+QWEAPON_REQUEST_LIFETIME;
        }
    }
}
static inline void qweapons_game_tick(QWeapons *s) {
    if(s->tick<UINT64_MAX-QWEAPON_REQUEST_LIFETIME-QWEAPON_HANDGUN_COOLDOWN)s->tick++;
    else {s->fault=true;qweapons_cancel(s);}
    if(s->pending&&s->tick>=s->expires)s->pending=false;
}
static inline bool qweapons_can_fire(const QWeapons *s) {
    return s->enabled&&s->ready&&!s->fault&&s->active&&s->exposed&&
           s->ammo[s->selected]>0&&s->tick>=s->next_shot[s->selected]&&
           (s->selected==QWEAPON_MACHINE_GUN||s->pending);
}
/* Called AFTER the original game decrements its real ammunition field. */
static inline bool qweapons_accepted(QWeapons *s,int actual_remaining) {
    if(!s->enabled||!s->ready)return false;
    if(actual_remaining!=s->ammo[s->selected]-1||actual_remaining<0||
       (s->selected==QWEAPON_HANDGUN&&!qweapons_can_fire(s))){
        s->fault=true;qweapons_cancel(s);return false;
    }
    s->ammo[s->selected]=actual_remaining;s->pending=false;s->shot_serial++;
    if(s->selected==QWEAPON_HANDGUN)s->next_shot[s->selected]=s->tick+QWEAPON_HANDGUN_COOLDOWN;
    return true;
}
/* Only the selected weapon reloads when the original game's reload event fires.
 * No timer, shield visibility guess, solenoid bit or button release can refill it. */
static inline int qweapons_reload(QWeapons *s) {
    s->ammo[s->selected]=qweapons_capacity(s->selected);s->pending=false;
    return s->ammo[s->selected];
}
