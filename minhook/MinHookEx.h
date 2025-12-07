#pragma once

#include "MinHook.h"

// some extended minhook functions that i didnt want to have in the main cpp
template<typename T, typename D, typename O>
MH_STATUS WINAPI MH_CreateHookOffset(T pTarget, int iOffset, D pDetour, O ppOriginal) {
	return MH_CreateHook(reinterpret_cast<void*>(
		reinterpret_cast<uintptr_t>(pTarget) + iOffset
		), reinterpret_cast<LPVOID*>(pDetour), reinterpret_cast<LPVOID*>(ppOriginal));
}

template<typename T, typename D, typename O>
MH_STATUS WINAPI MH_CreateHookEx(T pTarget, D pDetour, O ppOriginal) {
	return MH_CreateHook(reinterpret_cast<void*>(pTarget), reinterpret_cast<LPVOID*>(pDetour), reinterpret_cast<LPVOID*>(ppOriginal));
}