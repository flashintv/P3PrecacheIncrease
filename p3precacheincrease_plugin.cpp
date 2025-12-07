//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include <stdio.h>
#include "minhook/MinHookEx.h"
#include "sdk/interface.h"
#include "sdk/IServerPlugin.h"
#include "sdk/precache.h"
#include "sourcemod/sigscan.h"
#include "sdk/modelloader.h"
#include "sdk/ienginereplay.h"
#include "sdk/icommandline.h"
#include "sdk/vgui_baseui_interface.h"
#include "p3/mapreslistgenerator.h"
#include "p3/common.h"
#include "sdk/icvar.h"

IModelLoader* modelloader = nullptr;
IEngineVGuiInternal* enginevgui = nullptr;
ICvar* cvar = nullptr;

void(__cdecl* Host_Disconnect)(bool bShowMainMenu);

CSfxTable*(__cdecl* S_PrecacheSound)(const char* name);

//---------------------------------------------------------------------------------
// Purpose: a sample 3rd party plugin class
//---------------------------------------------------------------------------------
class CEmptyServerPlugin: public IServerPluginCallbacks
{
public:
	CEmptyServerPlugin() {}
	~CEmptyServerPlugin() {}

	// IServerPluginCallbacks methods
	virtual bool			Load(CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory);
	virtual void			Unload(void);
	virtual void			Pause(void) {}
	virtual void			UnPause(void) {}
	virtual const char*		GetPluginDescription(void) { return "P3IncreasePrecache - Increases precache limits and stringtable limits in Postal 3. (By Grizzle)"; }
	virtual void			LevelInit(char const* pMapName) {}
	virtual void			ServerActivate(void* pEdictList, int edictCount, int clientMax) {}
	virtual void			GameFrame(bool simulating) {}
	virtual void			LevelShutdown(void) {}
	virtual void			ClientActive(void* pEntity) {}
	virtual void			ClientDisconnect(void* pEntity) {}
	virtual void			ClientPutInServer(void* pEntity, char const* playername) {}
	virtual void			SetCommandClient(int index) {}
	virtual void			ClientSettingsChanged(void* pEdict) {}
	virtual PLUGIN_RESULT	ClientConnect(bool* bAllowConnect, void* pEntity, const char* pszName, const char* pszAddress, char* reject, int maxrejectlen) { return PLUGIN_CONTINUE; }
	virtual PLUGIN_RESULT	ClientCommand(void* pEntity, const CCommand& args) { return PLUGIN_CONTINUE; }
	virtual PLUGIN_RESULT	NetworkIDValidated(const char* pszUserName, const char* pszNetworkID) { return PLUGIN_CONTINUE; }
	virtual void			OnQueryCvarValueFinished(QueryCvarCookie_t iCookie, void* pPlayerEntity, EQueryCvarValueStatus eStatus, const char* pCvarName, const char* pCvarValue) {}
};
CEmptyServerPlugin g_EmptyServerPlugin;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CEmptyServerPlugin, IServerPluginCallbacks, INTERFACEVERSION_ISERVERPLUGINCALLBACKS, g_EmptyServerPlugin );

class CGameServer
{
	char padding[0x1e8];
public:
	CPrecacheItem model_precache[1024];
	CPrecacheItem generic_precache[512];
	CPrecacheItem sound_precache[8192];
	CPrecacheItem decal_precache[512];
	CNetworkStringTable* m_pModelPrecacheTable;
	CNetworkStringTable* m_pSoundPrecacheTable;
	CNetworkStringTable* m_pGenericPrecacheTable;
	CNetworkStringTable* m_pDecalPrecacheTable;
};
class CClientState
{
private:
	char padding1[0x8a00];
public:
	INetworkStringTableContainer* m_StringTableContainer;
private:
	char padding2[0x34c - sizeof(void*)];
public:
	CNetworkStringTable* m_pModelPrecacheTable;
	CNetworkStringTable* m_pGenericPrecacheTable;
	CNetworkStringTable* m_pSoundPrecacheTable;
	CNetworkStringTable* m_pDecalPrecacheTable;
	CNetworkStringTable* m_pInstanceBaselineTable;
	CNetworkStringTable* m_pLightStyleTable;
	CNetworkStringTable* m_pUserInfoTable;
	CNetworkStringTable* m_pServerStartupTable;
	CNetworkStringTable* m_pDownloadableTable;
	CPrecacheItem model_precache[1024];
	CPrecacheItem generic_precache[512];
	CPrecacheItem sound_precache[8192];
	CPrecacheItem decal_precache[512];
};

CPrecacheItem cl_precache_model[4096];
CPrecacheItem cl_precache_sound[16384];
CPrecacheItem sv_precache_model[4096];
CPrecacheItem sv_precache_sound[16384];

CSigScan CClientState__ClearSounds;
void __fastcall HK_CClientState_ClearSounds(CClientState* clientstate, void*)
{
	int c = ARRAYSIZE(cl_precache_sound);
	for (int i = 0; i < c; ++i)
	{
		cl_precache_sound[i].SetSound(NULL);
	}
}

CSigScan CClientState_Clear;
void(__thiscall* CClientState_Clear_org)(CClientState*);
void __fastcall HK_CClientState_Clear(CClientState* clientstate, void*)
{
	memset(cl_precache_model, 0, sizeof(cl_precache_model));
	memset(cl_precache_sound, 0, sizeof(cl_precache_sound));

	CClientState_Clear_org(clientstate);
}

CSigScan CGameServer_DumpPrecacheStats;
void __fastcall HK_CGameServer_DumpPrecacheStats(CGameServer* gameserver, void*, INetworkStringTable* table)
{
	if (table == NULL)
	{
		ConMsg("Can only dump stats when active in a level\n");
		return;
	}

	CPrecacheItem* items = NULL;
	if (table == gameserver->m_pModelPrecacheTable)
	{
		items = sv_precache_model;
	}
	else if (table == gameserver->m_pGenericPrecacheTable)
	{
		items = gameserver->generic_precache;
	}
	else if (table == gameserver->m_pSoundPrecacheTable)
	{
		items = sv_precache_sound;
	}
	else if (table == gameserver->m_pDecalPrecacheTable)
	{
		items = gameserver->decal_precache;
	}

	if (!items)
		return;

	int count = table->GetNumStrings();
	int maxcount = table->GetMaxStrings();

	ConMsg("\n");
	ConMsg("Precache table %s:  %i of %i slots used\n", table->GetTableName(),
		count, maxcount);

	for (int i = 0; i < count; i++)
	{
		char const* name = table->GetString(i);
		CPrecacheItem* slot = &items[i];

		int testLength;
		const CPrecacheUserData* p = (const CPrecacheUserData*)table->GetStringUserData(i, &testLength);
		ErrorIfNot(testLength == sizeof(*p),
			("CGameServer::DumpPrecacheStats: invalid CPrecacheUserData length (%d)", testLength)
		);

		if (!name || !slot || !p)
			continue;

		ConMsg("%03i:  %s (%s):   ",
			i,
			name,
			GetFlagString(p->flags));

		if (slot->GetReferenceCount() == 0)
		{
			ConMsg(" never used\n");
		}
		else
		{
			ConMsg(" %i refs, first %.2f mru %.2f\n",
				slot->GetReferenceCount(),
				slot->GetFirstReference(),
				slot->GetMostRecentReference());
		}
	}

	ConMsg("\n");
}

CSigScan CClientState_DumpPrecacheStats;
void __fastcall HK_CClientState_DumpPrecacheStats(CClientState* clientstate, void*, const char* name)
{
	if (!name || !name[0])
	{
		ConMsg("Can only dump stats when active in a level\n");
		return;
	}

	CPrecacheItem* items = NULL;

	if (!strcmp(MODEL_PRECACHE_TABLENAME, name))
	{
		items = cl_precache_model;
	}
	else if (!strcmp(GENERIC_PRECACHE_TABLENAME, name))
	{
		items = clientstate->generic_precache;
	}
	else if (!strcmp(SOUND_PRECACHE_TABLENAME, name))
	{
		items = cl_precache_sound;
	}
	else if (!strcmp(DECAL_PRECACHE_TABLENAME, name))
	{
		items = clientstate->decal_precache;
	}

	INetworkStringTable* table = clientstate->m_StringTableContainer->FindTable(name);
	if (!items || !table)
	{
		ConMsg("Precache table '%s' not found.\n", name);
		return;
	}

	int count = table->GetNumStrings();
	int maxcount = table->GetMaxStrings();

	ConMsg("\n");
	ConMsg("Precache table %s:  %i of %i slots used\n", table->GetTableName(),
		count, maxcount);

	for (int i = 0; i < count; i++)
	{
		char const* pchName = table->GetString(i);
		CPrecacheItem* slot = &items[i];
		int testLength;
		const CPrecacheUserData* p = (CPrecacheUserData*)table->GetStringUserData(i, &testLength);

		if (!pchName || !slot || !p)
			continue;

		ConMsg("%03i:  %s (%s):   ",
			i,
			pchName,
			GetFlagString(p->flags));


		if (slot->GetReferenceCount() == 0)
		{
			ConMsg(" never used\n");
		}
		else
		{
			ConMsg(" %i refs, first %.2f mru %.2f\n",
				slot->GetReferenceCount(),
				slot->GetFirstReference(),
				slot->GetMostRecentReference());
		}
	}

	ConMsg("\n");
}

CSigScan CClientState_GetModel;
model_t* __fastcall HK_CClientState_GetModel(CClientState* clientstate, void*, int index)
{
	if (!clientstate->m_pModelPrecacheTable)
	{
		return NULL;
	}

	if (index <= 0)
	{
		return NULL;
	}

	if (index >= clientstate->m_pModelPrecacheTable->GetNumStrings())
	{
		Assert(0); // model index for unkown model requested
		return NULL;
	}

	CPrecacheItem* p = &cl_precache_model[index];
	model_t* m = p->GetModel();
	if (m)
	{
		return m;
	}

	char const* name = clientstate->m_pModelPrecacheTable->GetString(index);

	m = modelloader->GetModelForName(name, IModelLoader::FMODELLOADER_CLIENT);
	if (!m)
	{
		int testLength;
		const CPrecacheUserData* data = (CPrecacheUserData*)clientstate->m_pModelPrecacheTable->GetStringUserData(index, &testLength);
		if (data && (data->flags & RES_FATALIFMISSING))
		{
			ConMsg("Cannot continue without model %s, disconnecting\n", name);
			Host_Disconnect(true);
		}
	}

	p->SetModel(m);
	return m;
}

CSigScan CClientState_SetModel;
void __fastcall HK_CClientState_SetModel(CClientState* clientstate, void*, int tableIndex)
{
	if (!clientstate->m_pModelPrecacheTable)
	{
		return;
	}

	// Bogus index
	if (tableIndex < 0 || tableIndex >= clientstate->m_pModelPrecacheTable->GetNumStrings())
	{
		return;
	}

	CPrecacheItem* p = &cl_precache_model[tableIndex];
	int testLength;
	const CPrecacheUserData* data = (CPrecacheUserData*)clientstate->m_pModelPrecacheTable->GetStringUserData(tableIndex, &testLength);

	static ConVar* cl_forcepreload = cvar->FindVar("cl_forcepreload");
	bool bLoadNow = (data && (data->flags & RES_PRELOAD));
	if (CommandLine()->FindParm("-nopreload") || CommandLine()->FindParm("-nopreloadmodels"))
	{
		bLoadNow = false;
	}
	else if (cl_forcepreload->GetInt() || CommandLine()->FindParm("-preload"))
	{
		bLoadNow = true;
	}

	if (bLoadNow)
	{
		char const* name = clientstate->m_pModelPrecacheTable->GetString(tableIndex);
		p->SetModel(modelloader->GetModelForName(name, IModelLoader::FMODELLOADER_CLIENT));
	}
	else
	{
		p->SetModel(NULL);
	}


	// log the file reference, if necssary
	if (MapReslistGenerator()->IsEnabled())
	{
		char const* name = clientstate->m_pModelPrecacheTable->GetString(tableIndex);
		MapReslistGenerator()->OnModelPrecached(name);
	}
}

CSigScan CClientState_GetSound;
void* __fastcall HK_CClientState_GetSound(CClientState* clientstate, void*, int index)
{
	if (index <= 0 || !clientstate->m_pSoundPrecacheTable)
		return NULL;

	if (index >= clientstate->m_pSoundPrecacheTable->GetNumStrings())
	{
		return NULL;
	}

	CPrecacheItem* p = &cl_precache_sound[index];
	CSfxTable* s = p->GetSound();
	if (s)
		return s;

	char const* name = clientstate->m_pSoundPrecacheTable->GetString(index);

	s = S_PrecacheSound(name);

	p->SetSound(s);
	return s;
}

CSigScan CClientState_SetSound;
void __fastcall HK_CClientState_SetSound(CClientState* clientstate, void*, int tableIndex)
{
	// Bogus index
	if (!clientstate->m_pSoundPrecacheTable)
		return;

	if (tableIndex < 0 || tableIndex >= clientstate->m_pSoundPrecacheTable->GetNumStrings()) {
		return;
	}

	CPrecacheItem* p = &cl_precache_sound[tableIndex];

	int testLength;
	const CPrecacheUserData* data = (CPrecacheUserData*)clientstate->m_pSoundPrecacheTable->GetStringUserData(tableIndex, &testLength);

	bool bLoadNow = (data && (data->flags & RES_PRELOAD));
	static ConVar* cl_forcepreload = cvar->FindVar("cl_forcepreload");
	if (CommandLine()->FindParm("-nopreload") || CommandLine()->FindParm("-nopreloadsounds"))
	{
		bLoadNow = false;
	}
	else if (cl_forcepreload->GetInt() || CommandLine()->FindParm("-preload"))
	{
		bLoadNow = true;
	}

	if (bLoadNow)
	{
		char const* name = clientstate->m_pSoundPrecacheTable->GetString(tableIndex);
		p->SetSound(S_PrecacheSound(name));
	}
	else
	{
		p->SetSound(NULL);
	}

	// log the file reference, if necssary
	if (MapReslistGenerator()->IsEnabled())
	{
		char const* name = clientstate->m_pSoundPrecacheTable->GetString(tableIndex);
		MapReslistGenerator()->OnSoundPrecached(name);
	}
}

CSigScan CGameServer_GetSound;
const char* __fastcall HK_CGameServer_GetSound(CGameServer* gameserver, void*, int index)
{
	if (index <= 0 || !gameserver->m_pSoundPrecacheTable) {
		return NULL;
	}

	if (index >= gameserver->m_pSoundPrecacheTable->GetNumStrings()) {
		return NULL;
	}

	CPrecacheItem* slot = &sv_precache_sound[index];
	return slot->GetName();
}

CSigScan CGameServer_GetModel;
model_t* __fastcall HK_CGameServer_GetModel(CGameServer* gameserver, void*, int index)
{
	if (index <= 0 || !gameserver->m_pModelPrecacheTable)
		return NULL;

	if (index >= gameserver->m_pModelPrecacheTable->GetNumStrings())
	{
		return NULL;
	}

	CPrecacheItem* slot = &sv_precache_model[index];
	model_t* m = slot->GetModel();
	if (m)
	{
		return m;
	}

	char const* modelname = gameserver->m_pModelPrecacheTable->GetString(index);
	Assert(modelname);

	m = modelloader->GetModelForName(modelname, IModelLoader::FMODELLOADER_SERVER);
	slot->SetModel(m);

	return m;
}

CSigScan CGameServer_PrecacheModel;
int __fastcall HK_CGameServer_PrecacheModel(CGameServer* gameserver, void*, char const* name, int flags, model_t* model)
{
	if (!gameserver->m_pModelPrecacheTable)
		return -1;

	int idx = gameserver->m_pModelPrecacheTable->AddString(true, name);
	if (idx == INVALID_STRING_INDEX)
	{
		return -1;
	}

	CPrecacheUserData p;

	// first time, set file size & flags
	CPrecacheUserData const* pExisting = (CPrecacheUserData const*)gameserver->m_pModelPrecacheTable->GetStringUserData(idx, NULL);
	if (!pExisting)
	{
		p.flags = flags;
	}
	else
	{
		// Just or in any new flags
		p = *pExisting;
		p.flags |= flags;
	}

	gameserver->m_pModelPrecacheTable->SetStringUserData(idx, sizeof(p), &p);

	CPrecacheItem* slot = &sv_precache_model[idx];

	if (model)
	{
		slot->SetModel(model);
	}

	bool bLoadNow = (!slot->GetModel() && (flags & RES_PRELOAD));
	static ConVar* sv_forcepreload = cvar->FindVar("sv_forcepreload");
	if (CommandLine()->FindParm("-nopreload") || CommandLine()->FindParm("-nopreloadmodels"))
	{
		bLoadNow = false;
	}
	else if (sv_forcepreload->GetInt() || CommandLine()->FindParm("-preload"))
	{
		bLoadNow = true;
	}

	if (idx != 0)
	{
		if (bLoadNow)
		{
			slot->SetModel(modelloader->GetModelForName(name, IModelLoader::FMODELLOADER_SERVER));
			enginevgui->UpdateProgressBar(PROGRESS_PRECACHE);
			MapReslistGenerator()->OnModelPrecached(name);
		}
		else
		{
			modelloader->ReferenceModel(name, IModelLoader::FMODELLOADER_SERVER);
			slot->SetModel(NULL);
		}
	}

	return idx;
}

CSigScan CGameServer_PrecacheSound;
int __fastcall HK_CGameServer_PrecacheSound(CGameServer* gameserver, void*, char const* name, int flags)
{
	if (!gameserver->m_pSoundPrecacheTable)
		return -1;

	int idx = gameserver->m_pSoundPrecacheTable->AddString(true, name);
	if (idx == INVALID_STRING_INDEX)
	{
		return -1;
	}

	// mark the sound as being precached, but check first that reslist generation is enabled to save on the va() call
	if (MapReslistGenerator()->IsEnabled() && name[0])
	{
		MapReslistGenerator()->OnResourcePrecached(va("sound/%s", PSkipSoundChars(name)));
	}

	// first time, set file size & flags
	CPrecacheUserData p;
	CPrecacheUserData const* pExisting = (CPrecacheUserData const*)gameserver->m_pSoundPrecacheTable->GetStringUserData(idx, NULL);
	if (!pExisting)
	{
		p.flags = flags;
	}
	else
	{
		// Just or in any new flags
		p = *pExisting;
		p.flags |= flags;
	}

	gameserver->m_pSoundPrecacheTable->SetStringUserData(idx, sizeof(p), &p);

	CPrecacheItem* slot = &sv_precache_sound[idx];
	slot->SetName(name);

	return idx;
}

bool CEmptyServerPlugin::Load(CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory )
{
	MH_Initialize();
	CSigScan::GetDllMemInfo("engine.dll");

	enginevgui = (IEngineVGuiInternal*)interfaceFactory(VENGINE_VGUI_VERSION, NULL);
	if (!enginevgui) {
		ConColorMsg(Color(255, 0, 0), "Failed to get IEngineVGui interface!\n");
		return false;
	}

	cvar = (ICvar*)interfaceFactory(CVAR_INTERFACE_VERSION, NULL);
	if (!cvar) {
		ConColorMsg(Color(255, 0, 0), "Failed to get ICvar interface!\n");
		return false;
	}

	if (!CMapReslistGenerator::SigScan())
		return false;

	// -----------------------------------------------------------------
	// Host_Disconnect function
	CSigScan Host_Disconnect_Sig;
	Host_Disconnect_Sig.Init(
		"\x80\x3D\xCC\xCC\xCC\xCC\xCC\x75\x0F\x8B\x44\x24\x04", "xx?????xxxxxx", 13);
	if (!Host_Disconnect_Sig.is_set) {
		ConColorMsg(Color(255, 0, 0), "Failed to find Host_Disconnect signature!\n");
		return false;
	}
	Host_Disconnect = (decltype(Host_Disconnect))Host_Disconnect_Sig.sig_addr;

	// -----------------------------------------------------------------
	// S_PrecacheSound function
	CSigScan S_PrecacheSound_Sig;
	S_PrecacheSound_Sig.Init(
		"\x8B\x0D\xCC\xCC\xCC\xCC\x85\xC9\x75\x03\x33\xC0\xC3\x8B\x01\x8B\x10",
		"xx????xxxxxxxxxxx", 17);
	if (!S_PrecacheSound_Sig.is_set) {
		ConColorMsg(Color(255, 0, 0), "Failed to find S_PrecacheSound signature!\n");
		return false;
	}
	S_PrecacheSound = (decltype(S_PrecacheSound))S_PrecacheSound_Sig.sig_addr;

	// -----------------------------------------------------------------
	// IModelLoader pointer from a signature for the CClientState hooks
	CSigScan modelloader_Sig;
	modelloader_Sig.Init(
		"\x8B\x0D\xCC\xCC\xCC\xCC\x8B\x11\x8B\x42\x1C\x6A\x04\x53\xFF\xD0\x8B\xE8",
		"xx????xxxxxxxxxxxx", 18);
	if (!modelloader_Sig.is_set) {
		ConColorMsg(Color(255, 0, 0), "Failed to find modelloader signature!\n");
		return false;
	}
	modelloader = **(IModelLoader***)((unsigned long)modelloader_Sig.sig_addr + 2);
	
	// -----------------------------------------------------------------
	// CClientState::ClearSounds signatures
	CClientState__ClearSounds.Init(
		"\x8D\x49\x00\x6A\x00\x8B\xCE\xE8\xCC\xCC\xCC\xCC\x83\xC6\x08", 
		"xxxxxxxx????xxx", 15);
	if (!CClientState__ClearSounds.is_set) {
		ConColorMsg(Color(255, 0, 0), "Failed to find CClientState::ClearSounds signature!\n");
		return false;
	}
	MH_CreateHookOffset(CClientState__ClearSounds.sig_addr, -13, 
		&HK_CClientState_ClearSounds, NULL);
	
	// -----------------------------------------------------------------
	// CreateEngineStringTables signatures and writes
	CSigScan CGameServer__CreateEngineStringTables_model;
	CSigScan CGameServer__CreateEngineStringTables_sound;

	CGameServer__CreateEngineStringTables_model.Init(
		"\x68\x00\x04\x00\x00\x8D\x54\x24\x20\x52\xFF\xD0", "xxxxxxxxxxxx", 12);
	CGameServer__CreateEngineStringTables_sound.Init(
		"\x68\x00\x20\x00\x00\x8D\x94\x24\x20\x02\x00\x00", "xxxxxxxxxxxx", 12);
	
	if (!CGameServer__CreateEngineStringTables_model.is_set || !CGameServer__CreateEngineStringTables_sound.is_set) {
		ConColorMsg(Color(255, 0, 0), "Failed to find CGameServer::CreateEngineStringTables signature!\n");
		return false;
	}
	WriteByte((void*)((unsigned long)CGameServer__CreateEngineStringTables_model.sig_addr + 2), 0x10); // 4096 models
	WriteByte((void*)((unsigned long)CGameServer__CreateEngineStringTables_sound.sig_addr + 2), 0x40); // 16384 sounds
	
	// DumpPrecacheStats signatures and hooks
	CGameServer_DumpPrecacheStats.Init(
		"\x55\x8B\xEC\x83\xE4\xC0\x83\xEC\x34\x53\x56\x8B\x75\x08", "xxxxxxxxxxxxxx", 14);
	CClientState_DumpPrecacheStats.Init(
		"\x55\x8B\xEC\x83\xE4\xC0\x8B\x45\x08\x83\xEC\x34\x85\xC0", "xxxxxxxxxxxxxx", 14);
	
	if (!CGameServer_DumpPrecacheStats.is_set || !CClientState_DumpPrecacheStats.is_set) {
		ConColorMsg(Color(255, 0, 0), "Failed to find DumpPrecacheStats signature!\n");
		return false;
	}
	
	MH_CreateHook(CGameServer_DumpPrecacheStats.sig_addr,
		&HK_CGameServer_DumpPrecacheStats, NULL);
	MH_CreateHook(CClientState_DumpPrecacheStats.sig_addr,
		&HK_CClientState_DumpPrecacheStats, NULL);
	
	// -----------------------------------------------------------------
	// CClientState::Clear hook
	CClientState_Clear.Init(
		"\x53\x56\x8B\xF1\xE8\xCC\xCC\xCC\xCC\x33\xDB\x6A\xFF", "xxxxx????xxxx", 13);
	
	if (!CClientState_Clear.is_set) {
		ConColorMsg(Color(255, 0, 0), "Failed to find DumpPrecacheStats signature!\n");
		return false;
	}
	
	MH_CreateHookEx(CClientState_Clear.sig_addr,
		&HK_CClientState_Clear, &CClientState_Clear_org);
	
	// -----------------------------------------------------------------
	// CClientState Get signatures and hooks
	CClientState_GetModel.Init(
		"\x56\x8B\xF1\x83\xBE\x4C\x8D\x00\x00\x00\x75\x06", "xxxxxxxxxxxx", 12);
	CClientState_GetSound.Init(
		"\x56\x57\x8B\x7C\x24\x0C\x85\xFF\x8B\xF1\x7E\x1A\x83\xBE\x54\x8D\x00\x00\x00", 
		"xxxxxxxxxxxxxxxxxxx", 19);
	
	if (!CClientState_GetModel.is_set || !CClientState_GetSound.is_set) {
		ConColorMsg(Color(255, 0, 0), "Failed to find a CClientState Getter signature!\n");
		return false;
	}
	
	MH_CreateHook(CClientState_GetModel.sig_addr,
		&HK_CClientState_GetModel, NULL);
	MH_CreateHook(CClientState_GetSound.sig_addr,
		&HK_CClientState_GetSound, NULL);
	
	// -----------------------------------------------------------------
	// CClientState Set signatures and hooks
	CClientState_SetModel.Init(
		"\x56\x8B\xF1\x83\xBE\x4C\x8D\x00\x00\x00\x0F\x84\xF4\x00\x00\x00", 
		"xxxxxxxxxxxxxxxx", 16);
	CClientState_SetSound.Init(
		"\x56\x8B\xF1\x83\xBE\x54\x8D\x00\x00\x00", "xxxxxxxxxx", 10);
	
	if (!CClientState_SetModel.is_set || !CClientState_SetSound.is_set) {
		ConColorMsg(Color(255, 0, 0), "Failed to find a CClientState Setter signature!\n");
		return false;
	}
	
	MH_CreateHook(CClientState_SetModel.sig_addr,
		&HK_CClientState_SetModel, NULL);
	MH_CreateHook(CClientState_SetSound.sig_addr,
		&HK_CClientState_SetSound, NULL);
	
	// -----------------------------------------------------------------
	// CGameServer Get signatures and hooks
	CGameServer_GetSound.Init(
		"\x56\x57\x8B\x7C\x24\x0C\x85\xFF\x8B\xF1\x7E\x2B", "xxxxxxxxxxxx", 12);
	CGameServer_GetModel.Init(
		"\x56\x57\x8B\x7C\x24\x0C\x85\xFF\x8B\xF1\x7E\x1A\x83\xBE\xE8\x41\x01\x00\x00",
		"xxxxxxxxxxxxxxxxxxx", 19);
	
	if (!CGameServer_GetSound.is_set || !CGameServer_GetModel.is_set) {
		ConColorMsg(Color(255, 0, 0), "Failed to find a CGameServer Getter signature!\n");
		return false;
	}
	
	MH_CreateHook(CGameServer_GetSound.sig_addr,
		&HK_CGameServer_GetSound, NULL);
	MH_CreateHook(CGameServer_GetModel.sig_addr,
		&HK_CGameServer_GetModel, NULL);
	
	// -----------------------------------------------------------------
	// CGameServer Precache signatures and hooks
	CGameServer_PrecacheModel.Init(
		"\x51\x56\x8B\xF1\x83\xBE\xE8\x41\x01\x00\x00", "xxxxxxxxxxx", 11);
	CGameServer_PrecacheSound.Init(
		"\x57\x8B\xF9\x83\xBF\xEC\x41\x01\x00\x00", "xxxxxxxxxx", 10);
	
	if (!CGameServer_PrecacheModel.is_set || !CGameServer_PrecacheSound.is_set) {
		ConColorMsg(Color(255, 0, 0), "Failed to find a CGameServer Precacher signature!\n");
		return false;
	}
	
	MH_CreateHook(CGameServer_PrecacheModel.sig_addr,
		&HK_CGameServer_PrecacheModel, NULL);
	MH_CreateHook(CGameServer_PrecacheSound.sig_addr,
		&HK_CGameServer_PrecacheSound, NULL);
	
	// Fully initialized!
	ConColorMsg(Color(255, 0, 255), "Initialized P3IncreasePrecache plugin.\n");
	ConColorMsg(Color(255, 0, 255), "Enjoy your increased limits!\n");
	
	MH_EnableHook(MH_ALL_HOOKS);
	
	return true;
}

void CEmptyServerPlugin::Unload( void )
{
	MH_DisableHook(MH_ALL_HOOKS);
	MH_Uninitialize();
}