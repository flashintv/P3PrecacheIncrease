#pragma once

class CMapReslistGenerator
{
public:
	static bool SigScan();

public:
	void OnModelPrecached(const char* relativePathFileName);
	void OnSoundPrecached(const char* relativePathFileName);
	void OnResourcePrecached(const char* relativePathFileName);

	bool IsEnabled() { return m_bLoggingEnabled; }
	bool IsLoggingToMap() { return m_bLoggingEnabled && !m_bLogToEngineList; }

public:
	bool		m_bTrackingDeletions;
	bool		m_bLoggingEnabled;
	bool		m_bUsingMapList;
	bool		m_bRestartOnTransition;
	bool		m_bLogToEngineList;
	bool		m_bAutoQuit;
	// only these cuz i didnt feel like adding CUtl classes by hand
};

extern CMapReslistGenerator* MapReslistGenerator();
extern CMapReslistGenerator* (*GetMapReslistGenerator)();
extern CMapReslistGenerator* (__thiscall* MapReslistGenerator_OnResourcePrecached)(CMapReslistGenerator*, const char*);