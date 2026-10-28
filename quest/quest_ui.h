#pragma once
#include "vr_math.h"
#include "quest_options.h"
#include "quest_weapon_rules.h"
bool qui_init(void);
/* Actual startup size is distinct from the saved selection pending a restart. */
void qui_set_resolution(int preset,unsigned width,unsigned height);
void qui_draw(const float view[16],const float projection[16],V3 head,Q4 rotation,const QOptions *options,int page,int boot_side,bool saved);
void qui_shutdown(void);

void qui_metrics_update(const char *a,const char *b,const char *c,const char *d);
void qui_draw_metrics(const float view[16],const float projection[16],V3 head,Q4 rotation);

void qui_set_pointer(float x,float y,bool visible);

/* Informational cover-only selector. Never changes aiming or game state. */
void qui_weapons_update(const QWeapons *state,bool visible);
void qui_draw_weapons(const float view[16],const float projection[16],V3 head,Q4 rotation);

/* Head-relative presentation only; does not change FOV, aiming or game state. */
void qui_draw_border(const float projection[16],Q4 eye_to_head,int mode);
