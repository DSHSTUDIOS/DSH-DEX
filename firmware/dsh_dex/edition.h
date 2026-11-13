#pragma once

#include "ir_link.h"
#include "sprites.h"

#ifndef DSH_DEX_WIRE_ID
#define DSH_DEX_WIRE_ID 1
#endif

namespace edition {

struct Profile {
    const char *pokedexTitle;
    const char *saveKey;
    const uint8_t *idle;
    const uint8_t *step;
};

constexpr uint8_t wireId = DSH_DEX_WIRE_ID;
static_assert(wireId >= infrared::garchomp && wireId <= infrared::metagross,
              "Unknown DSH-DEX species code");

constexpr Profile profile =
    wireId == infrared::giratina ? Profile{"#0487  GIRATINA", "giratina-v1", sprites::giratina, nullptr} :
    wireId == infrared::metagross ? Profile{"#0376  METAGROSS", "metagross-v1", sprites::metagross, nullptr} :
    wireId == infrared::pikachu ? Profile{"#0025  PIKACHU", "pikachu-v1", nullptr, nullptr} :
    wireId == infrared::charizard ? Profile{"#0006  CHARIZARD", "charizard-v1", nullptr, nullptr} :
    Profile{"#0445  GARCHOMP", "garchomp-v1", sprites::idleA, sprites::idleB};

}  // namespace edition
