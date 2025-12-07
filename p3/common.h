#pragma once
#include <cstdarg>

char* va(const char* format, ...);

extern inline bool IsSoundChar(char c);
extern inline char* PSkipSoundChars(const char* pch);