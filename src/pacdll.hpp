#pragma once

#ifdef _WIN32
#ifdef PACDLL_EXPORTS
#define PACDLL_API __declspec(dllexport)
#else
#define PACDLL_API __declspec(dllimport)
#endif
#else
#define PACDLL_API
#endif

extern "C" PACDLL_API auto Init() -> void;
extern "C" PACDLL_API auto Term() -> void;
