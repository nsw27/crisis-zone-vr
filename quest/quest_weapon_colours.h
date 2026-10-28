#pragma once
#include <stdint.h>
/* Shared arcade shell identity. Index order: machine gun, handgun, shotgun.
 * Shotgun uses three native hit dispatches per confirmed shell. */
static const uint8_t q_arcade_weapon_colours[3][3]={
    {55,188,199}, {237,107,166}, {255,138,41}
};
