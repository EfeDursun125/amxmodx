// vim: set ts=4 sw=4 tw=99 noet:
//
// AMX Mod X, based on AMX Mod by Aleksander Naszko ("OLO").
// Copyright (C) The AMX Mod X Development Team.
//
// This software is licensed under the GNU General Public License, version 3 or higher.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://alliedmods.net/amxmodx-license

#include <string.h>
#include "nongpl_matches.h"
#include <clib.h>

bool CheckBadConList(const char *cvar, int type)
{
	struct NONGPL_PLUGIN_T
	{
		const char* author;
		const char* title;
		const char* filename;
	};

	struct NONGPL_CVAR_T
	{
		const char* cvar;
		int type;
	};

	NONGPL_PLUGIN_T NONGPL_PLUGIN_LIST[] =
	{
		{"Live", "CZ Gun Game", "czgungame.amxx"},
		{"Live", "AMXX Gun Game", "czgungame.amxx"},
		{nullptr, nullptr, nullptr},
	};

	NONGPL_CVAR_T NONGPL_CVAR_LIST[] =
	{
		{"gg_mode", 0},
		{"gg_warmuptimer", 0},
		{"gg_ff", 0},
		{"gg_fflevel", 0},
		{"gg_stats", 0},
		{"gg_dm", 0},
		{"gg_turbo", 0},
		{"amx_ggreset", 1},
		{"amx_gg", 1},
		{nullptr, 0},
	};

	int i = 0;
	while (NONGPL_CVAR_LIST[i].cvar)
	{
		if (NONGPL_CVAR_LIST[i].type == type && cstrcmp(NONGPL_CVAR_LIST[i].cvar, cvar) == 0)
			return true;

		i++;
	}

	return false;
}
