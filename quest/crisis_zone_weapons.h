#pragma once
#include "quest_weapon_rules.h"
/* Host API. No weapon damage/shotgun hook is exposed until independently verified. */
void csz_weapons_configure(bool enabled,bool handgun);
void csz_weapons_sample(bool active,bool exposed,float trigger);
void csz_weapons_cancel(void);
bool csz_weapons_handgun_active(void);
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
