// vim: set ts=4 sw=4 tw=99 noet:
//
// AMX Mod X, based on AMX Mod by Aleksander Naszko ("OLO").
// Copyright (C) The AMX Mod X Development Team.
//
// This software is licensed under the GNU General Public License, version 3 or higher.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://alliedmods.net/amxmodx-license

#include "amxmodx.h"
#include "newmenus.h"
#include <clib.h>

// *****************************************************
// class CPlayer
// *****************************************************

void CPlayer::Init(edict_t* e, const int i)
{
	index = i;
	pEdict = e;
	initialized = false;
	ingame = false;
	authorized = false;
	disconnecting = false;
	teamIdsInitialized = false;

	current = 0;
	teamId = -1;
	deaths = 0;
	aiming = 0;
	menu = 0;
	keys = 0;
	menuexpire = 0.0;
	newmenu = -1;

	death_weapon = nullptr;
	name = nullptr;
	ip = nullptr;
	team = nullptr;
}

void CPlayer::Disconnect(void)
{
	ingame = false;
	initialized = false;
	authorized = false;
	disconnecting = false;
	teamIdsInitialized = false;

	if (Menu *pMenu = get_menu_by_id(newmenu))
		pMenu->Close(index);

	List<ClientCvarQuery_Info*>::iterator iter, end = queries.end();
	for (iter = queries.begin(); iter != end; iter++)
	{
		unregisterSPForward((*iter)->resultFwd);
		delete[] (*iter)->params;
		delete (*iter);
	}

	queries.clear();
	menu = 0;
	newmenu = -1;
}

void CPlayer::PutInServer(void)
{
	playtime = gpGlobals->time;
	ingame = true;
}

bool CPlayer::Connect(const char* connectname, const char* ipaddress)
{
	name = connectname;
	ip = ipaddress;
	time = gpGlobals->time;
	death_killer = 0;
	menu = 0;
	newmenu = -1;
	
	cmemset(flags, 0, sizeof(flags));
	cmemset(weapons, 0, sizeof(weapons));
	
	initialized = true;
	authorized = false;

	uint_fast8_t i;
	constexpr uint_fast8_t max = 4;
	for (i = 0; i <= max; i++)
	{
		channels[i] = 0.0f;
		hudmap[i] = 0;
	}

	List<ClientCvarQuery_Info*>::iterator iter, end = queries.end();
	for (iter = queries.begin(); iter != end; iter++)
	{
		unregisterSPForward((*iter)->resultFwd);
		delete[] (*iter)->params;
		delete (*iter);
	}

	queries.clear();

	const char* authid = GETPLAYERAUTHID(pEdict);
	if (authid == 0 || *authid == 0 || cstrcmp(authid, "STEAM_ID_PENDING") == 0)
		return true;

	return false;
}

// *****************************************************
// class Grenades
// *****************************************************

void Grenades::put(edict_t* grenade, const float time, const int type, CPlayer* player)
{
	Obj* a = new(std::nothrow) Obj;
	if (a == nullptr)
		return;

	a->player = player;
	a->grenade = grenade;
	a->time = gpGlobals->time + time;
	a->type = type;
	a->next = head;
	head = a;
}

bool Grenades::find(edict_t* enemy, CPlayer** p, int& type)
{
	bool found = false;
	Obj** a = &head;
	Obj* b;

	while (*a)
	{
		if ((*a)->time > gpGlobals->time)
		{
			if ((*a)->grenade == enemy)
			{
				found = true;
				(*p) = (*a)->player;
				type = (*a)->type;
			}
		}
		else
		{
			b = (*a)->next;
			delete *a;
			*a = b;
			continue;
		}

		a = &(*a)->next;
	}
	
	return found;
}

void Grenades::clear(void)
{
	Obj* a;
	while (head != nullptr)
	{
		a = head->next;
		delete head;
		head = a;
	}
}

// *****************************************************
// class XVars
// *****************************************************

void XVars::clear(void)
{
	delete[] head;
	head = 0;
	num = 0;
	size = 0;
}

int XVars::put(AMX* p, cell* v)
{
	int a;
	for (a = 0; a < num; ++a)
	{
		if ((head[a].amx == p) && (head[a].value == v))
			return a;
	}

	if ((num >= size) && realloc_array(size ? (size * 2) : 8))
		return -1;

	head[num].value = v;
	head[num].amx = p;
	return num++;
}

int XVars::realloc_array(const int nsize)
{
	XVarEle* me = new(std::nothrow) XVarEle[nsize];
	if (me != nullptr)
	{
		int a;
		for (a = 0 ; a < num; ++a)
			me[a] = head[a];
		
		delete[] head;
		head = me;
		size = nsize;
		return 0;
	}
	
	return 1;
}

// *****************************************************
// class TeamIds
// *****************************************************

TeamIds::TeamIds(void) { head = 0; newTeam = 0; }

TeamIds::~TeamIds(void)
{
	TeamEle* a;
	while (head != nullptr)
	{
		a = head->next;
		delete head;
		head = a;
	}
}

void TeamIds::registerTeam(const char* n, int s)
{
	TeamEle** a = &head;
	while (*a)
	{
		if (cstrcmp((*a)->name.chars(),n) == 0)
		{
			if (s != -1)
			{
				(*a)->id = s;
				newTeam &= ~(1<<(*a)->tid);				
			}
			
			return;
		}

		a = &(*a)->next;
	}

	*a = new(std::nothrow) TeamEle(n, s);
	if (*a == nullptr)
		return;
	
	newTeam |= (1 << (*a)->tid);
}

int TeamIds::findTeamId(const char* n)
{
	TeamEle* a = head;
	while (a != nullptr)
	{
		if (!cstricmp(a->name.chars(), n))
			return a->id;

		a = a->next;
	}
	
	return -1;
}

int TeamIds::findTeamIdCase(const char* n)
{
	TeamEle* a = head;
	while (a != nullptr)
	{
		if (!cstrcmp(a->name.chars(), n))
			return a->id;

		a = a->next;
	}
	
	return -1;
}

char TeamIds::TeamEle::uid = 0;
