// vim: set ts=4 sw=4 tw=99 noet:
//
// AMX Mod X, based on AMX Mod by Aleksander Naszko ("OLO").
// Copyright (C) The AMX Mod X Development Team.
//
// This software is licensed under the GNU General Public License, version 3 or higher.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://alliedmods.net/amxmodx-license

#include "amxmodx.h"
#include "CEvent.h"
#include <clib.h>

// *****************************************************
// class ClEvent
// *****************************************************

NativeHandle<EventHook> EventHandles;

EventsMngr::ClEvent::ClEvent(CPluginMngr::CPlugin* plugin, const int func, const int flags)
{
	m_Plugin = plugin;
	m_Func = func;

	// flags
	m_FlagAlive = true;
	m_FlagDead = true;

	m_FlagWorld = (flags & 1) ? true : false; // flag a
	m_FlagClient = (flags & 2) ? true : false; // flag b
	m_FlagOnce = (flags & 4) ? true : false; // flag c
	
	if (flags & 24)
	{
		m_FlagAlive = (flags & 16) ? true : false; // flag e
		m_FlagDead = (flags & 8) ? true : false; // flag d
	}

	if (m_FlagClient)
	{
		m_FlagPlayer = true;
		m_FlagBot = true;

		if (flags & 96)
		{
			m_FlagPlayer = (flags & 32) ? true : false; // flag f
			m_FlagBot = (flags & 64) ? true : false; // flag g
		}
	}

	m_Stamp = 0.0f;
	m_Done = false;
	m_State = FSTATE_ACTIVE;
	m_Conditions = nullptr;
}

EventsMngr::ClEvent::~ClEvent(void)
{
	cond_t *tmp1 = m_Conditions;
	cond_t *tmp2 = nullptr;
	
	while (tmp1)
	{
		tmp2 = tmp1->next;
		delete tmp1;
		tmp1 = tmp2;
	}
	
	m_Conditions = nullptr;
}

void EventsMngr::NextParam(void)
{
	if (m_ParsePos < m_ParseVaultSize)
		return;

	constexpr int INITIAL_PARSEVAULT_SIZE = 32;
	MsgDataEntry *tmp = nullptr;
	int tmpSize = 0;
	
	if (m_ParseVault)
	{
		// copy to tmp
		tmp = new(std::nothrow) MsgDataEntry[m_ParseVaultSize];
		if (!tmp)
			return;	// :TODO: Error report !!
		
		cmemcpy(tmp, m_ParseVault, m_ParseVaultSize * sizeof(MsgDataEntry));
		tmpSize = m_ParseVaultSize;
		delete[] m_ParseVault;
		m_ParseVault = nullptr;
	}

	if (m_ParseVaultSize > 0)
		m_ParseVaultSize *= 2;
	else
		m_ParseVaultSize = INITIAL_PARSEVAULT_SIZE;

	MsgDataEntry* pointer = new(std::nothrow) MsgDataEntry[m_ParseVaultSize];
	if (!pointer)
		return;

	m_ParseVault = pointer;
	pointer = nullptr;
	
	if (tmp)
	{
		cmemcpy(m_ParseVault, tmp, tmpSize * sizeof(MsgDataEntry));
		delete[] tmp;
		tmp = nullptr;
	}
}

int EventsMngr::ClEvent::getFunction(void)
{
	return m_Func;
}

EventsMngr::EventsMngr(void)
{
	m_ParseVault = nullptr;
	m_ParseVaultSize = 0;
	m_ParseMsgType = -1;
	m_ReadVault = nullptr;
	m_ReadVaultSize = 0;
	m_ReadPos = -1;
	m_ReadMsgType = -1;
	clearEvents();
}

EventsMngr::~EventsMngr(void)
{
	clearEvents();
}

CPluginMngr::CPlugin * EventsMngr::ClEvent::getPlugin(void)
{
	return m_Plugin;
}

// *****************************************************
// class EventsMngr
// *****************************************************

void EventsMngr::ClEvent::registerFilter(char *filter)
{
	// filters (conditions) have the form x&y
	// x is the param number
	// & may also be other characters
	// y is a string or a number
	if (!filter)
		return;

	char* value = filter;

	// get the first numbr
	while (isdigit(*value))
		++value;

	// end of string => ignore
	if (!*value)
		return;

	cond_t *tmpCond = new(std::nothrow) cond_t;
	if (!tmpCond)
		return;

	// type character
	tmpCond->type = *value;

	// set a null here so param id can be recognized, and save it
	*value++ = 0;
	tmpCond->paramId = catoi(filter);

	// rest of line
	tmpCond->sValue = value;
	tmpCond->fValue = catof(value);
	tmpCond->iValue = catoi(value);
	tmpCond->next = nullptr;

	if (m_Conditions)
	{
		cond_t *tmp = m_Conditions;
		while (tmp->next)
			tmp = tmp->next;
		
		tmp->next = tmpCond;
	}
	else
		m_Conditions = tmpCond;
}

void EventsMngr::ClEvent::setForwardState(const ForwardState state)
{
	m_State = state;
}

int EventsMngr::registerEvent(CPluginMngr::CPlugin* plugin, const int func, const int flags, const int msgid)
{
	if (msgid < 0 || msgid >= MAX_AMX_REG_MSG)
		return 0;

	auto event = ke::AutoPtr<ClEvent>(new(std::nothrow) ClEvent(plugin, func, flags));
	if (!event)
		return 0;

	const int handle = EventHandles.create(event.get());
	if (!handle)
		return 0;

	m_Events[msgid].append(ke::Move(event));
	return handle;
}

void EventsMngr::parserInit(const int msg_type, float* timer, CPlayer* pPlayer, const int index)
{
	if (msg_type < 0 || msg_type > MAX_AMX_REG_MSG)
		return;

	m_ParseNotDone = false;

	// don't parse if nothing to do
	if (!m_Events[msg_type].length())
		return;

	m_ParseMsgType = msg_type;
	m_Timer = timer;

	for (auto &event : m_Events[msg_type])
	{
		if (event->m_Done)
			continue;

		if (!event->m_Plugin->isExecutable(event->m_Func))
		{
			event->m_Done = true;
			continue;
		}

		if (pPlayer)
		{
			if (!event->m_FlagClient || (pPlayer->IsBot() ? !event->m_FlagBot : !event->m_FlagPlayer) || (pPlayer->IsAlive() ? !event->m_FlagAlive : !event->m_FlagDead))
			{
				event->m_Done = true;
				continue;
			}
		}
		else if (!event->m_FlagWorld)
		{
			event->m_Done = true;
			continue;
		}

		if (event->m_FlagOnce && event->m_Stamp == *timer)
		{
			event->m_Done = true;
			continue;
		}
		
		m_ParseNotDone = true;
	}

	if (m_ParseNotDone)
	{
		m_ParsePos = 0;
		NextParam();
		m_ParseVault[0].type = MSG_INTEGER;
		m_ParseVault[0].iValue = index;
	}
	
	m_ParseFun = &m_Events[msg_type];
}

void EventsMngr::parseValue(const int iValue)
{
	// not parsing
	if (!m_ParseNotDone || !m_ParseFun)
		return;

	// grow if needed
	++m_ParsePos;
	NextParam();

	m_ParseVault[m_ParsePos].type = MSG_INTEGER;
	m_ParseVault[m_ParsePos].iValue = iValue;

	bool execute;
	bool anyConditions;

	// loop through the registered funcs, and decide whether they have to be called or not
	// if they shouldnt, their m_Done is set to true
	for (auto &event : *m_ParseFun)
	{
		if (event->m_Done)
			continue; // already skipped; don't bother with parsing

		// loop through conditions
		execute = false;
		anyConditions = false;
		
		for (auto condIter = event->m_Conditions; condIter; condIter = condIter->next)
		{
			if (condIter->paramId == m_ParsePos)
			{
				anyConditions = true;
				switch (condIter->type)
				{
					case '=': if (condIter->iValue == iValue) execute = true; break;
					case '!': if (condIter->iValue != iValue) execute = true; break;
					case '&': if (iValue & condIter->iValue) execute = true; break;
					case '<': if (iValue < condIter->iValue) execute = true; break;
					case '>': if (iValue > condIter->iValue) execute = true; break;
				}
				if (execute)
					break;
			}
		}
		
		if (anyConditions && !execute)
			event->m_Done = true; // don't execute
	}
}

void EventsMngr::parseValue(const float fValue)
{
	// not parsing
	if (!m_ParseNotDone || !m_ParseFun)
		return;

	// grow if needed
	++m_ParsePos;
	NextParam();

	m_ParseVault[m_ParsePos].type = MSG_FLOAT;
	m_ParseVault[m_ParsePos].fValue = fValue;

	bool execute;
	bool anyConditions;

	// loop through the registered funcs, and decide whether they have to be called or not
	// if they shouldnt, their m_Done is set to true
	for (auto &event : *m_ParseFun)
	{
		if (event->m_Done)
			continue; // already skipped; don't bother with parsing

		// loop through conditions
		execute = false;
		anyConditions = false;
		
		for (auto condIter = event->m_Conditions; condIter; condIter = condIter->next)
		{
			if (condIter->paramId == m_ParsePos)
			{
				anyConditions = true;
				switch (condIter->type)
				{
					case '=': if (condIter->fValue == fValue) execute = true; break;
					case '!': if (condIter->fValue != fValue) execute = true; break;
					case '<': if (fValue < condIter->fValue) execute = true; break;
					case '>': if (fValue > condIter->fValue) execute = true; break;
				}
				
				if (execute)
					break;
			}
		}
		
		if (anyConditions && !execute)
			event->m_Done = true; // don't execute
	}
}

void EventsMngr::parseValue(const char *sz)
{
	// not parsing
	if (!m_ParseNotDone || !m_ParseFun)
		return;

	// grow if needed
	++m_ParsePos;
	NextParam();

	m_ParseVault[m_ParsePos].type = MSG_STRING;
	m_ParseVault[m_ParsePos].sValue = sz;

	bool execute;
	bool anyConditions;

	// loop through the registered funcs, and decide whether they have to be called or not
	// if they shouldnt, their m_Done is set to true
	for (auto &event : *m_ParseFun)
	{
		if (event->m_Done)
			continue; // already skipped; don't bother with parsing

		// loop through conditions
		execute = false;
		anyConditions = false;
		
		for (auto condIter = event->m_Conditions; condIter; condIter = condIter->next)
		{
			if (condIter->paramId == m_ParsePos)
			{
				anyConditions = true;
				switch (condIter->type)
				{
					case '=': if (!cstrcmp(sz, condIter->sValue.chars())) execute = true; break;
					case '!': if (cstrcmp(sz, condIter->sValue.chars())) execute = true; break;
					case '&': if (strstr(sz, condIter->sValue.chars())) execute = true; break;
				}
				
				if (execute)
					break;
			}
		}
		
		if (anyConditions && !execute)
			event->m_Done = true; // don't execute
	}
}

void EventsMngr::executeEvents(void)
{
	static unsigned int reentrant = 0;
	if (!m_ParseFun)
		return;

	// store old read data, which are either default values or previous event data
	int oldMsgType = m_ReadMsgType, oldReadPos = m_ReadPos;
	MsgDataEntry *oldReadVault = m_ReadVault, *readVault = nullptr;
	
	// we have a re-entrant call
	if (reentrant++)
	{
		// create temporary read vault
		readVault = new(std::nothrow) MsgDataEntry[m_ParsePos + 1];
		if (!readVault)
			return;

		m_ReadVault = readVault;
	}
	else if (m_ReadVaultSize != m_ParseVaultSize)
	{
		// extend read vault size if necessary
		auto pointer = new(std::nothrow) MsgDataEntry[m_ParseVaultSize];
		if (!pointer)
			return;

		delete[] m_ReadVault;
		m_ReadVault = pointer;
		pointer = nullptr;
		m_ReadVaultSize = m_ParseVaultSize;
		
		// update old read vault so we don't restore to a wrong pointer
		oldReadVault = m_ReadVault;
	}

	// Copy data over to readvault
	m_ReadPos = m_ParsePos;
	m_ReadMsgType = m_ParseMsgType;

	if (m_ParseVault)
		cmemcpy(m_ReadVault, m_ParseVault, (m_ParsePos + 1) * sizeof(MsgDataEntry));

	// Reset this here so we don't trigger re-entrancy for unregistered messages
	auto parseFun = m_ParseFun;
	m_ParseFun = nullptr;

	size_t i;
	size_t lastSize = parseFun->length();
	for (i = 0; i < lastSize; i++)
	{
		auto &event = parseFun->at(i);
		if (event->m_Done) 
		{
			event->m_Done = false;
			continue;
		}
		
		event->m_Stamp = *m_Timer;
		if (event->m_State == FSTATE_ACTIVE)
			executeForwards(event->m_Func, static_cast<cell>(m_ReadVault ? m_ReadVault[0].iValue : 0));
	}
	
	// Restore old read data, either resetting to default or to previous event data
	m_ReadMsgType = oldMsgType;
	m_ReadPos = oldReadPos;
	m_ReadVault = oldReadVault;
	delete[] readVault;
	--reentrant;
}

int EventsMngr::getArgNum(void) const
{
	return m_ReadPos + 1;
}

const char* EventsMngr::getArgString(const int a) const
{
	if (a < 0 || a > m_ReadPos)
		return "";

	char* var = static_cast<char*>(malloc(33));
	if (!var)
		return "";

	switch (m_ReadVault[a].type)
	{
		case MSG_INTEGER: 
		{
			sprintf(var, "%d", m_ReadVault[a].iValue);
			return var;
		}
		case MSG_STRING: 
			return m_ReadVault[a].sValue;
		default:
		{
			sprintf(var, "%g", m_ReadVault[a].fValue);
			return var;
		}
	}
}

int EventsMngr::getArgInteger(const int a) const
{
	if (a < 0 || a > m_ReadPos)
		return 0;

	switch (m_ReadVault[a].type)
	{
		case MSG_INTEGER:
			return m_ReadVault[a].iValue;
		case MSG_STRING:
			return catoi(m_ReadVault[a].sValue);
		default:
			return static_cast<int>(m_ReadVault[a].fValue);
	}
}

float EventsMngr::getArgFloat(const int a) const
{
	if (a < 0 || a > m_ReadPos)
		return 0.0f;

	switch (m_ReadVault[a].type)
	{
		case MSG_INTEGER:
			return static_cast<float>(m_ReadVault[a].iValue);
		case MSG_STRING:
			return catof(m_ReadVault[a].sValue);
		default:
			return m_ReadVault[a].fValue; 
	}
}

void EventsMngr::clearEvents(void)
{
	int i;
	for (i = 0; i < MAX_AMX_REG_MSG; ++i)
		m_Events[i].clear();

	EventHandles.clear();

	// delete parsevault
	if (m_ParseVault)
	{
		delete[] m_ParseVault;
		m_ParseVault = nullptr;
		m_ParseVaultSize = 0;
	}

	if (m_ReadVault)
	{
		delete[] m_ReadVault;
		m_ReadVault = nullptr;
		m_ReadVaultSize = 0;
		m_ReadPos = -1;
	}
}

int EventsMngr::getEventId(const char* msg)
{
	// :TODO: Remove this somehow!!! :)
	const struct CS_Events
	{
		const char* name;
		CS_EventsIds id;
	} table[] =
	{
		{"CS_DeathMsg",		CS_DeathMsg},
//		{"CS_RoundEnd",		CS_RoundEnd},
//		{"CS_RoundStart",	CS_RoundStart},
//		{"CS_Restart",		CS_Restart},
		{"",				CS_Null}
	};

	// if msg is a number, return it
	int pos = catoi(msg);
	if (pos != 0)
		return pos;

	// try to find in table first
	for (pos = 0; table[pos].id != CS_Null; ++pos)
	{
		if (!cstrcmp(table[pos].name, msg))
			return table[pos].id;
	}

	// find the id of the message
	return pos = GET_USER_MSG_ID(PLID, msg, 0);
}

int EventsMngr::getCurrentMsgType(void)
{
	return m_ReadMsgType;
}
