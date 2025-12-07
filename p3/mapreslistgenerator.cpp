#pragma once

#include <Windows.h>
#include <stdio.h>
#include "mapreslistgenerator.h"

#include "../sourcemod/sigscan.h"
#include "../sdk/dbg.h"

CMapReslistGenerator* (*GetMapReslistGenerator)();
CMapReslistGenerator* (__thiscall* MapReslistGenerator_OnResourcePrecached)(CMapReslistGenerator*, const char*);

CMapReslistGenerator* MapReslistGenerator()
{
	static CMapReslistGenerator* ptr = GetMapReslistGenerator();
	return ptr;
}

bool CMapReslistGenerator::SigScan()
{
	// MapReslistGenerator function
	CSigScan MapReslistGenerator_Sig;
	MapReslistGenerator_Sig.Init(
		"\xE8\xCC\xCC\xCC\xCC\x80\x78\x01\x00\x74\x26", "x????xxxxxx", 11);
	if (!MapReslistGenerator_Sig.is_set) {
		ConColorMsg(Color(255, 0, 0), "Failed to find MapReslistGenerator signature!\n");
		return false;
	}

	uintptr_t EIP = reinterpret_cast<uintptr_t>(MapReslistGenerator_Sig.sig_addr);
	EIP += 5 + *reinterpret_cast<uintptr_t*>(EIP + 1);
	GetMapReslistGenerator = (decltype(GetMapReslistGenerator))EIP;

	// MapReslistGenerator_OnResourcePrecached function
	CSigScan MapReslistGenerator_OnResourcePrecached_Sig;
	MapReslistGenerator_OnResourcePrecached_Sig.Init(
		"\x81\xEC\x04\x01\x00\x00\x56\x8B\xF1\x80\x7E\x01\x00\x74\x3E", "xxxxxxxxxxxxxxx", 15);
	if (!MapReslistGenerator_OnResourcePrecached_Sig.is_set) {
		ConColorMsg(Color(255, 0, 0), "Failed to find MapReslistGenerator_OnResourcePrecached signature!\n");
		return false;
	}
	MapReslistGenerator_OnResourcePrecached = (decltype(MapReslistGenerator_OnResourcePrecached))MapReslistGenerator_OnResourcePrecached_Sig.sig_addr;
	
	return true;
}

void CMapReslistGenerator::OnResourcePrecached(const char* relativePathFileName)
{
	MapReslistGenerator_OnResourcePrecached(this, relativePathFileName);
}

void CMapReslistGenerator::OnModelPrecached(const char* relativePathFileName)
{
	if (!IsEnabled())
		return;

	if (strstr(relativePathFileName, ".vmt"))
	{
		// it's a materials file, make sure that it starts in the materials directory, and we get the .vtf
		char file[_MAX_PATH];

		if (!_strnicmp(relativePathFileName, "materials", strlen("materials")))
		{
			strncpy(file, relativePathFileName, sizeof(file));
		}
		else
		{
			// prepend the materials directory
			snprintf(file, sizeof(file), "materials\\%s", relativePathFileName);
		}
		OnResourcePrecached(file);

		// get the matching vtf file
		char* ext = strstr(file, ".vmt");
		if (ext)
		{
			strncpy(ext, ".vtf", 5);
			OnResourcePrecached(file);
		}
	}
	else
	{
		OnResourcePrecached(relativePathFileName);
	}
}

void CMapReslistGenerator::OnSoundPrecached(const char* relativePathFileName)
{
	// skip any special characters
	if (!isalnum(relativePathFileName[0]))
	{
		++relativePathFileName;
	}

	// prepend the sound/ directory if necessary
	char file[_MAX_PATH];
	if (!_strnicmp(relativePathFileName, "sound", strlen("sound")))
	{
		strncpy(file, relativePathFileName, sizeof(file));
	}
	else
	{
		// prepend the sound directory
		snprintf(file, sizeof(file), "sound\\%s", relativePathFileName);
	}

	OnResourcePrecached(file);
}