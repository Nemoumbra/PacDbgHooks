#pragma once

#include "ppsspp_api.hpp"

#ifdef _WIN32
#ifdef PACDLL_EXPORTS
#define PACDLL_API __declspec(dllexport)
#else
#define PACDLL_API __declspec(dllimport)
#endif
#else
#define PACDLL_API
#endif

extern "C" PACDLL_API auto Init(EmuFuncs &) -> void;
extern "C" PACDLL_API auto Hook(PacState &, MIPSState &) -> void;
extern "C" PACDLL_API auto Term() -> void;
