// vim: set ts=4 sw=4 tw=99 noet:
//
// AMX Mod X, based on AMX Mod by Aleksander Naszko ("OLO").
// Copyright (C) The AMX Mod X Development Team.
//
// This software is licensed under the GNU General Public License, version 3 or higher.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://alliedmods.net/amxmodx-license

#include "amxmodx.h"
#include "CCmd.h"
#include <clib.h>

// *****************************************************
// class CmdMngr
// *****************************************************

CmdMngr::CmdMngr(void)
{ 
	cmemset(sortedlists, 0, sizeof(sortedlists));
	srvcmdlist = 0;
	clcmdlist = 0;
	prefixHead = 0;
	buf_type = -1;
	buf_access = 0;
	buf_id = -1;
	buf_cmdid = -1;
	buf_cmdtype = -1;
	buf_cmdaccess = 0;
}

CmdMngr::Command::Command(CPluginMngr::CPlugin* pplugin, const char* pcmd, const char* pinfo, const int pflags, const int pfunc, const bool pviewable, const bool pinfo_ml, CmdMngr* pparent) : commandline(pcmd), info(pinfo)
{
	char szCmd[64], szArg[64];
	*szCmd = 0; *szArg = 0;
	sscanf(pcmd, "%s %s", szCmd, szArg);
	command = szCmd;
	argument = szArg;
	plugin = pplugin;
	flags = pflags;
	cmdtype = 0;
	prefix = 0;
	function = pfunc;
	listable = pviewable;
	info_ml = pinfo_ml;
	parent = pparent;
	id = --uniqueid;
}

CmdMngr::Command::~Command(void)
{
	++uniqueid;
}

CmdMngr::Command* CmdMngr::registerCommand(CPluginMngr::CPlugin* plugin, const int func, const char* cmd, const char* info, const int level, const bool listable, const bool info_ml)
{
	Command* b = new(std::nothrow) Command(plugin, cmd, info, level, func, listable, info_ml, this);
	if (!b)
		return 0;

	setCmdLink(&sortedlists[0], b);
	return b;
}

CmdMngr::Command* CmdMngr::getCmd(long int id, const int type, const int access)
{
	//if (id >= 1024 || id < 0) return (Command*)id;
	if (id < 0)
	{
		CmdMngr::iterator a = begin(type);
		const CmdMngr::iterator b = end();
		for (; a != b; ++a)
		{
			if (a && (*a).id == id)
				return &(*a);
		}
		
		return 0;
	}

	if ((id < buf_cmdid) || (access != buf_cmdaccess) || (type != buf_cmdtype))
	{
		buf_cmdptr = begin(type);
		buf_cmdaccess = access;
		buf_cmdtype = type;
		buf_cmdid = id;
	}
	else
	{
		const int a = id;
		id -= buf_cmdid;
		buf_cmdid = a;
	}

	while (buf_cmdptr)
	{
		if ((*buf_cmdptr).gotAccess(access) && (*buf_cmdptr).getPlugin()->isExecutable((*buf_cmdptr).getFunction()) && (*buf_cmdptr).isViewable())
		{
			if (id-- == 0) 
				return &(*buf_cmdptr);
		}

		++buf_cmdptr;
	}

	return 0;		
}

int CmdMngr::getCmdNum(const int type, const int access)
{
	buf_access = access;
	buf_type = type;
	buf_num = 0;
	CmdMngr::iterator a = begin(type);
	const CmdMngr::iterator b = end();
	for (; a != b; ++a)
	{
		if (a && (*a).gotAccess(access) && (*a).getPlugin()->isExecutable((*a).getFunction()) && (*a).isViewable())
			++buf_num;
	}

	return buf_num;
}

void CmdMngr::setCmdLink(CmdLink** a, Command* c, const bool sorted)
{
	CmdLink* np = new(std::nothrow) CmdLink(c);
	if (!np)
		return;

	if (sorted)
	{
		while (*a)
		{
			const int i = cstrcmp(c->getCommand(), (*a)->cmd->getCommand());
			if ((i < 0) || ((i == 0) && (cstrcmp(c->getArgument(), (*a)->cmd->getArgument()) < 0)))
				break;
			
			a = &(*a)->next;
		}

		np->next = *a;
		*a = np;
	}
	else
	{
		while (*a) a = &(*a)->next;
		*a = np;
	}
}

void CmdMngr::clearCmdLink(CmdLink** phead, const bool pclear)
{
	CmdLink* pp;
	while (*phead)
	{
		pp = (*phead)->next;
		if (pclear)
			delete (*phead)->cmd;

		delete *phead;
		*phead = pp;
	}
}

void CmdMngr::Command::setCmdType(const int a)
{
	switch (a)
	{
		case CMD_ConsoleCommand: cmdtype |= 3; break;
		case CMD_ClientCommand: cmdtype |= 1; break;
		case CMD_ServerCommand: cmdtype |= 2; break;
	}

	if (cmdtype & 1) // ClientCommand
	{
		parent->setCmdLink(&parent->sortedlists[1], this);
		if (!parent->registerCmdPrefix(this))
			parent->setCmdLink(&parent->clcmdlist, this, false);
	}
	
	if (cmdtype & 2) // ServerCommand
	{
		parent->setCmdLink(&parent->sortedlists[2], this);
		parent->setCmdLink(&parent->srvcmdlist, this, false);
	}
}

const char* CmdMngr::Command::getCmdType(void) const
{
	switch (cmdtype)
	{
		case 1:	return "client";
		case 2:	return "server";
		case 3:	return "console";
	}
	
	return "unknown";
}

bool CmdMngr::registerCmdPrefix(Command* cc)
{
	CmdPrefix** b = findPrefix(cc->getCommand());
	if (*b)
	{
		setCmdLink(&(*b)->list, cc, false);
		cc->prefix = (*b)->name.length();
		return true;
	}
	
	return false;
}

void CmdMngr::registerPrefix(const char* nn)
{
	if (*nn == 0)
		return;

	CmdPrefix** b = findPrefix(nn);
	if (*b)
		return;

	auto mem = new(std::nothrow) CmdPrefix(nn, this);
	if (mem)
		*b = mem;
}

CmdMngr::CmdPrefix** CmdMngr::findPrefix(const char* nn)
{
	CmdPrefix** aa = &prefixHead;
	while (*aa)
	{
		if (!cstrncmp((*aa)->name.chars(), nn, (*aa)->name.length()))
			break;

		aa = &(*aa)->next;
	}
	
	return aa;
}

void CmdMngr::clearPrefix(void)
{
	CmdPrefix* a;
	while (prefixHead != nullptr)
	{
		a = prefixHead->next;
		delete prefixHead;
		prefixHead = a;
	}
}

void CmdMngr::clear(void)
{
	clearCmdLink(&sortedlists[0], true);
	clearCmdLink(&sortedlists[1]);
	clearCmdLink(&sortedlists[2]);
	clearCmdLink(&srvcmdlist);
	clearCmdLink(&clcmdlist);
	clearPrefix();
	clearBufforedInfo();
}

void CmdMngr::clearBufforedInfo(void)
{
	buf_type = -1; 
	buf_access = 0; 
	buf_id = -1; 
	buf_cmdid = -1;
	buf_cmdtype = -1;
	buf_cmdaccess = 0;
}

int CmdMngr::Command::uniqueid = 0;
