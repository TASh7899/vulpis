#pragma once
#ifndef VULPIS_COLOR_H
#define VULPIS_COLOR_H

#include <SDL2/SDL_pixels.h>
#include <cctype>
#include <iostream>
#include <sstream>
#include <cstring>

SDL_Color parseHexColor(const char* hexStr);

#endif
