// vim: set ts=4 sw=4 tw=99 noet:
//
// AMX Mod X, based on AMX Mod by Aleksander Naszko ("OLO").
// Copyright (C) The AMX Mod X Development Team.
//
// This software is licensed under the GNU General Public License, version 3 or higher.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://alliedmods.net/amxmodx-license

#include "amxmodx.h"
#include "CMenu.h"

// *****************************************************
// class MenuMngr
// *****************************************************
MenuMngr::MenuCommand::MenuCommand(CPluginMngr::CPlugin *a, const int mi, const int k, const int f, const bool new_menu)
{
	plugin = a;
	keys = k;
	menuid = mi;
	next = 0;
	is_new_menu = new_menu;
	function = f;
}

MenuMngr::~MenuMngr(void) 
{
	clear();
	MenuMngr::MenuIdEle::uniqueid = 0;
}

int MenuMngr::findMenuId(const char* name, AMX* amx)
{
	MenuIdEle* b;
	for (b = headid; b; b = b->next)
	{
		if ((!amx || !b->amx || amx == b->amx) && strstr(name, b->name.chars()))
			return b->id;
	}
	
	return 0;
}

int MenuMngr::registerMenuId(const char* n, AMX* a)
{
	const int id = findMenuId(n, a);
	if (id)
		return id;
	
	MenuIdEle* pointer = new(std::nothrow) MenuIdEle(n, a, headid);
	if (pointer == nullptr)
		return 0;

	headid = pointer;
	pointer = nullptr;
	return headid->id;
}

void MenuMngr::registerMenuCmd(CPluginMngr::CPlugin *a, const int mi, const int k, const int f, const bool from_new_menu)
{
	MenuCommand **temp = &headcmd;
	if (from_new_menu)
	{
		MenuCommand *ptr;
		while (*temp)
		{
			ptr = *temp;
			if (ptr->is_new_menu && ptr->plugin == a && ptr->menuid == mi)
			{
				if (g_forwards.isSameSPForward(ptr->function, f))
					return;
			}

			temp = &(*temp)->next;
		}
	}
	else
	{
		while (*temp)
			temp = &(*temp)->next;
	}

	MenuCommand* pointer = new(std::nothrow) MenuCommand(a, mi, k, f, from_new_menu);
	if (pointer == nullptr)
		return;

	*temp = pointer;
	pointer = nullptr;
}

void MenuMngr::clear(void)
{
	MenuIdEle* a;
	while (headid != nullptr)
	{
		a = headid->next;
		delete headid;
		headid = a;
	}

	MenuCommand* b;
	while (headcmd != nullptr)
	{
		b = headcmd->next;
		delete headcmd;
		headcmd = b;
	}
}

MenuMngr::iterator MenuMngr::SetWatchIter(MenuMngr::iterator iter)
{
	MenuMngr::iterator old = m_watch_iter;
	m_watch_iter = iter;
	return old;
}

int MenuMngr::MenuIdEle::uniqueid = 0;
