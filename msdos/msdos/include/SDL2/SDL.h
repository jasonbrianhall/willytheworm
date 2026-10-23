// The bare-metal build's SDL2 stand-in, reached on its own: baremetal/overrides
// also replaces <fstream>/<sstream>, which DJGPP's libstdc++ provides for real.
#pragma once
#include "../../../baremetal/overrides/SDL2/SDL.h"
