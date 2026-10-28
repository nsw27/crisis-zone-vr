#pragma once
#include "quest_weapon_rules.h"
/* Host API. Shotgun repeats ordinary hit processing; never writes enemy health. */
void csz_weapons_configure(bool enabled,bool handgun);
void csz_weapons_select(bool enabled,QWeapon weapon);
bool csz_weapons_single_active(void);
void csz_weapons_sample(bool active,bool exposed,float trigger);
void csz_weapons_cancel(void);
bool csz_weapons_handgun_active(void);
/* Current live player only: no attract/replay/fault state for weapon selection. */
bool csz_weapons_selection_ready(void);
bool csz_weapons_pending(void);
bool csz_weapons_feedback(void);
const QWeapons *csz_weapons_state(void);
/* Called only by build-time-instrumented CSZO4 Ver. B lifted instructions. */
void csz_weapons_init(uint32_t player);
void csz_weapons_reset(uint32_t player);
void csz_weapons_reload(uint32_t player);
void csz_weapons_begin(uint32_t gp);
uint32_t csz_weapons_filter(uint32_t eligibility);
void csz_weapons_accepted(uint32_t player);
/* Native hit-dispatch context, not global aim registers or reticle positions. */
void csz_weapons_hit_begin(void);
int32_t csz_weapons_hit_axis(uint32_t caller,int axis,int32_t original);
uint32_t csz_weapons_hit_flags(uint32_t original);
void csz_weapons_hit_complete(void);
void csz_weapons_hit_end(void);
