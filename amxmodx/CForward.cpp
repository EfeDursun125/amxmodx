// vim: set ts=4 sw=4 tw=99 noet:
//
// AMX Mod X, based on AMX Mod by Aleksander Naszko ("OLO").
// Copyright (C) The AMX Mod X Development Team.
//
// This software is licensed under the GNU General Public License, version 3 or higher.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://alliedmods.net/amxmodx-license

#include "amxmodx.h"
#include "debugger.h"
#include "binlog.h"
#include <clib.h>

CForward::CForward(const char *name, const ForwardExecType et, const int8_t numParams, const ForwardParam *paramTypes)
{
	m_FuncName = name;
	m_ExecType = et;
	m_NumParams = numParams;
	cmemcpy((void *)m_ParamTypes, paramTypes, numParams * sizeof(ForwardParam));
	
	// find funcs
	cell func;
	m_Funcs.clear();
	
	for (CPluginMngr::iterator iter = g_plugins.begin(); iter; ++iter)
	{
		if ((*iter).isValid() && amx_FindPublic((*iter).getAMX(), name, &func) == AMX_ERR_NONE)
		{
			AMXForward tmp;
			tmp.pPlugin = &(*iter);
			tmp.func = func;
			m_Funcs.append(tmp);
		}
	}

	m_Name = name;
}

#define STRINGEX_MAXLENGTH 128
cell CForward::execute(cell *params, ForwardPreparedArray *preparedArrays)
{
	cell realParams[FORWARD_MAX_PARAMS];
	cell *physAddrs[FORWARD_MAX_PARAMS];
	cell globRetVal = 0;

	// loop cache
	cell retVal;
	int err;
	unsigned int k;
	char* tmp2;
	AMX* amx;
	Debugger* pDebugger;
	cell* tmp;
	int8_t j;
	size_t i;
	for (i = 0; i < m_Funcs.length(); ++i)
	{
		auto iter = &m_Funcs[i];
		if (iter->pPlugin->isExecutable(iter->func))
		{
			// get debug info
			amx = iter->pPlugin->getAMX();
			pDebugger = (Debugger*)amx->userdata[UD_DEBUGGER];
			if (pDebugger)
				pDebugger->BeginExec();
			
			// handle strings & arrays & values by reference
			for (j = 0; j < m_NumParams; ++j)
			{
				if (m_ParamTypes[j] == FP_STRING || m_ParamTypes[j] == FP_STRINGEX)
				{
					tmp2 = reinterpret_cast<char*>(params[j]);
					if (!tmp2)
						tmp2 = "";

					amx_Allot(amx, (m_ParamTypes[j] == FP_STRING) ? cstrlen(tmp2) + 1 : STRINGEX_MAXLENGTH, &realParams[j], &tmp);
					amx_SetStringOld(tmp, tmp2, 0, 0);
					physAddrs[j] = tmp;
				}
				else if (m_ParamTypes[j] == FP_ARRAY)
				{
					amx_Allot(amx, preparedArrays[params[j]].size, &realParams[j], &tmp);
					physAddrs[j] = tmp;
					
					if (preparedArrays[params[j]].type == Type_Cell)
						cmemcpy(tmp, preparedArrays[params[j]].ptr, preparedArrays[params[j]].size * sizeof(cell));
					else
					{
						tmp2 = (char*)preparedArrays[params[j]].ptr;
						for (k = 0; k < preparedArrays[params[j]].size; ++k)
							*tmp++ = (static_cast<cell>(*tmp2++)) & 0xFF;
					}
				}
				else if (m_ParamTypes[j] == FP_CELL_BYREF || m_ParamTypes[j] == FP_FLOAT_BYREF)
				{
					amx_Allot(amx, 1, &realParams[j], &tmp);
					physAddrs[j] = tmp;

					if (m_ParamTypes[j] == FP_CELL_BYREF)
						cmemcpy(tmp, reinterpret_cast<cell*>(params[j]), sizeof(cell));
					else
						cmemcpy(tmp, reinterpret_cast<REAL*>(params[j]), sizeof(REAL));
				}
				else
					realParams[j] = params[j];
			}
			
			// push the parameters in reverse order. Weird, unfriendly part of Small 3.0!
			for (j = m_NumParams-1; j >= 0; j--)
				amx_Push(amx, realParams[j]);
			
			// exec
			retVal = 0;
#if defined BINLOG_ENABLED
			g_BinLog.WriteOp(BinLog_CallPubFunc, iter->pPlugin->getId(), iter->func);
#endif

			err = amx_ExecPerf(amx, &retVal, iter->func);
			// log runtime error, if any
			if (err != AMX_ERR_NONE)
			{
				//Did something else set an error?
				if (pDebugger && pDebugger->ErrorExists())
				{
					// we don't care, something else logged the error.
				}
				else if (err != -1)
				{
					// nothing logged the error so spit it out anyway
					LogError(amx, err, nullptr);
				}
			}
			
			amx->error = AMX_ERR_NONE;
			if (pDebugger)
				pDebugger->EndExec();

			// cleanup strings & arrays & values by reference
			for (j = 0; j < m_NumParams; ++j)
			{
				if (m_ParamTypes[j] == FP_STRING)
					amx_Release(amx, realParams[j]);
				else if (m_ParamTypes[j] == FP_STRINGEX)
				{
					// copy back
					amx_GetStringOld(reinterpret_cast<char*>(params[j]), physAddrs[j], 0);
					amx_Release(amx, realParams[j]);
				}
				else if (m_ParamTypes[j] == FP_ARRAY)
				{
					// copy back
					if (preparedArrays[params[j]].copyBack)
					{
						tmp = physAddrs[j];
						if (preparedArrays[params[j]].type == Type_Cell)
							cmemcpy(preparedArrays[params[j]].ptr, tmp, preparedArrays[params[j]].size * sizeof(cell));
						else
						{
							tmp2 = (char*)preparedArrays[params[j]].ptr;
							for (k = 0; k < preparedArrays[params[j]].size; ++k)
								*tmp2++ = static_cast<char>(*tmp++ & 0xFF);
						}
					}

					amx_Release(amx, realParams[j]);
				}
				else if (m_ParamTypes[j] == FP_CELL_BYREF || m_ParamTypes[j] == FP_FLOAT_BYREF)
				{
					// copy back
					tmp = physAddrs[j];
					if (m_ParamTypes[j] == FP_CELL_BYREF)
						cmemcpy(reinterpret_cast<cell *>(params[j]), tmp, sizeof(cell));
					else
						cmemcpy(reinterpret_cast<REAL *>(params[j]), tmp, sizeof(REAL));

					amx_Release(amx, realParams[j]);
				}
			}

			// decide what to do (based on exectype and retval)
			switch (m_ExecType)
			{
				case ET_IGNORE:
					break;
				case ET_STOP:
				{
					if (retVal > 0)
						return retVal;
				}
				case ET_STOP2:
				{
					if (retVal == 1)
						return 1;
					else if (retVal > globRetVal)
						globRetVal = retVal;
					break;
				}
				case ET_CONTINUE:
				{
					if (retVal > globRetVal)
						globRetVal = retVal;
					break;
				}
			}
		}
	}
	
	return globRetVal;
}

void CSPForward::Set(const cell func, AMX *amx, const int8_t numParams, const ForwardParam *paramTypes)
{
	char name[sNAMEMAX];
	m_Func = func;
	m_Amx = amx;
	m_NumParams = numParams;
	cmemcpy((void *)m_ParamTypes, paramTypes, numParams * sizeof(ForwardParam));
	m_HasFunc = true;
	isFree = false;
	name[0] = '\0';
	amx_GetPublic(amx, func, name);
	m_Name = name;
	m_ToDelete = false;
	m_InExec = false;
}

void CSPForward::Set(const char *funcName, AMX *amx, const int8_t numParams, const ForwardParam *paramTypes)
{
	m_Amx = amx;
	m_NumParams = numParams;
	cmemcpy((void *)m_ParamTypes, paramTypes, numParams * sizeof(ForwardParam));
	m_HasFunc = (amx_FindPublic(amx, funcName, &m_Func) == AMX_ERR_NONE);
	isFree = false;
	m_Name = funcName;
	m_ToDelete = false;
	m_InExec = false;
}

cell CSPForward::execute(cell *params, ForwardPreparedArray *preparedArrays)
{
	if (isFree)
		return 0;

	cell realParams[FORWARD_MAX_PARAMS];
	cell *physAddrs[FORWARD_MAX_PARAMS];

	if (!m_HasFunc || m_ToDelete)
		return 0;

	CPluginMngr::CPlugin *pPlugin = g_plugins.findPluginFast(m_Amx);
	if (pPlugin && !pPlugin->isExecutable(m_Func))
		return 0;

	m_InExec = true;

	Debugger *pDebugger = (Debugger *)m_Amx->userdata[UD_DEBUGGER];
	if (pDebugger)
		pDebugger->BeginExec();

	// handle strings & arrays & values by reference
	cell retVal;
	int err;
	unsigned int k;
	char* tmp2;
	cell* tmp;
	int8_t i;
	for (i = 0; i < m_NumParams; ++i)
	{
		if (m_ParamTypes[i] == FP_STRING || m_ParamTypes[i] == FP_STRINGEX)
		{
			tmp2 = reinterpret_cast<char*>(params[i]);
			if (!tmp2)
				tmp2 = "";
			amx_Allot(m_Amx, (m_ParamTypes[i] == FP_STRING) ? cstrlen(tmp2) + 1 : STRINGEX_MAXLENGTH, &realParams[i], &tmp);
			amx_SetStringOld(tmp, tmp2, 0, 0);
			physAddrs[i] = tmp;
		}
		else if (m_ParamTypes[i] == FP_ARRAY)
		{
			amx_Allot(m_Amx, preparedArrays[params[i]].size, &realParams[i], &tmp);
			physAddrs[i] = tmp;
			
			if (preparedArrays[params[i]].type == Type_Cell)
				cmemcpy(tmp, preparedArrays[params[i]].ptr, preparedArrays[params[i]].size * sizeof(cell));
			else
			{
				tmp2 = (char*)preparedArrays[params[i]].ptr;
				for (k = 0; k < preparedArrays[params[i]].size; ++k)
					*tmp++ = (static_cast<cell>(*tmp2++)) & 0xFF;
			}
		}
		else if (m_ParamTypes[i] == FP_CELL_BYREF || m_ParamTypes[i] == FP_FLOAT_BYREF)
		{
			amx_Allot(m_Amx, 1, &realParams[i], &tmp);
			physAddrs[i] = tmp;
			if (m_ParamTypes[i] == FP_CELL_BYREF)
				cmemcpy(tmp, reinterpret_cast<cell*>(params[i]), sizeof(cell));
			else
				cmemcpy(tmp, reinterpret_cast<REAL*>(params[i]), sizeof(REAL));
		}
		else
			realParams[i] = params[i];
	}
	
	for (i = m_NumParams - 1; i >= 0; i--)
		amx_Push(m_Amx, realParams[i]);
	
	// exec
	retVal = 0;
#if defined BINLOG_ENABLED
	g_BinLog.WriteOp(BinLog_CallPubFunc, pPlugin->getId(), m_Func);
#endif
	err = amx_ExecPerf(m_Amx, &retVal, m_Func);
	if (err != AMX_ERR_NONE)
	{
		// did something else set an error?
		if (pDebugger && pDebugger->ErrorExists())
		{
			// we don't care, something else logged the error.
		}
		else if (err != -1)
		{
			// nothing logged the error so spit it out anyway
			LogError(m_Amx, err, nullptr);
		}
	}
	
	if (pDebugger)
		pDebugger->EndExec();
	
	m_Amx->error = AMX_ERR_NONE;

	// cleanup strings & arrays & values by reference
	for (i = 0; i < m_NumParams; ++i)
	{
		if (m_ParamTypes[i] == FP_STRING)
			amx_Release(m_Amx, realParams[i]);
		else if (m_ParamTypes[i] == FP_STRINGEX)
		{
			// copy back
			amx_GetStringOld(reinterpret_cast<char*>(params[i]), physAddrs[i], 0);
			amx_Release(m_Amx, realParams[i]);
		}
		else if (m_ParamTypes[i] == FP_ARRAY)
		{
			// copy back
			if (preparedArrays[params[i]].copyBack)
			{
				tmp = physAddrs[i];
				if (preparedArrays[params[i]].type == Type_Cell)
					cmemcpy(preparedArrays[params[i]].ptr, tmp, preparedArrays[params[i]].size * sizeof(cell));
				else
				{
					tmp2 = (char*)preparedArrays[params[i]].ptr;
					for (k = 0; k < preparedArrays[params[i]].size; ++k)
						*tmp2++ = static_cast<char>(*tmp++ & 0xFF);
				}
			}
			amx_Release(m_Amx, realParams[i]);
		}
		else if (m_ParamTypes[i] == FP_CELL_BYREF || m_ParamTypes[i] == FP_FLOAT_BYREF)
		{
			// copy back
			tmp = physAddrs[i];
			if (m_ParamTypes[i] == FP_CELL_BYREF)
				cmemcpy(reinterpret_cast<cell *>(params[i]), tmp, sizeof(cell));
			else
				cmemcpy(reinterpret_cast<REAL *>(params[i]), tmp, sizeof(REAL));
			amx_Release(m_Amx, realParams[i]);
		}
	}

	m_InExec = false;
	return retVal;
}

int CForwardMngr::registerForward(const char *funcName, const ForwardExecType et, const int8_t numParams, const ForwardParam * paramTypes)
{
	CForward *tmp = new(std::nothrow) CForward(funcName, et, numParams, paramTypes);
	if (!tmp)
		return -1; // should be invalid
	
	const int retVal = (m_Forwards.length() << 1);
	m_Forwards.append(tmp);
	return retVal;
}

int CForwardMngr::registerSPForward(const cell func, AMX *amx, const int8_t numParams, const ForwardParam *paramTypes)
{
	if (!m_FreeSPForwards.empty())
	{
		const int retVal = m_FreeSPForwards.front();
		CSPForward* pForward = m_SPForwards[retVal >> 1];
		pForward->Set(func, amx, numParams, paramTypes);
		if (pForward->getFuncsNum() == 0)
			return -1;
		
		m_FreeSPForwards.pop();
		return retVal;
	}
	else
	{
		
		CSPForward* pForward = new(std::nothrow) CSPForward();
		if (pForward == nullptr)
			return -1;
		
		const int retVal = ((m_SPForwards.length() << 1) | 1);
		pForward->Set(func, amx, numParams, paramTypes);
		if (pForward->getFuncsNum() == 0)
		{
			delete pForward;
			return -1;
		}
 
		m_SPForwards.append(pForward);
		return retVal;
	}
	
	return -1;
}

int CForwardMngr::registerSPForward(const char *funcName, AMX *amx, const int8_t numParams, const ForwardParam *paramTypes)
{
	if (!m_FreeSPForwards.empty())
	{
		const int retVal = m_FreeSPForwards.front();
		CSPForward* pForward = m_SPForwards[retVal >> 1]; // >> 1 because unregisterSPForward pushes the id which contains the sp flag
		pForward->Set(funcName, amx, numParams, paramTypes);
		if (pForward->getFuncsNum() == 0)
			return -1;
		
		m_FreeSPForwards.pop();
		return retVal;
	}
	else
	{
		CSPForward* pForward = new(std::nothrow) CSPForward();
		if (pForward == nullptr)
			return -1;
		
		const int retVal = ((m_SPForwards.length() << 1) | 1);
		pForward->Set(funcName, amx, numParams, paramTypes);
		if (pForward->getFuncsNum() == 0)
		{
			delete pForward;
			return -1;
		}
		
		m_SPForwards.append(pForward);
		return retVal;
	}
	
	return -1;
}

cell CForwardMngr::executeForwards(const int id, cell *params)
{
	int retVal;
	if (id & 1)
	{
		CSPForward *fwd = m_SPForwards[id >> 1];
		retVal = fwd->execute(params, m_TmpArrays);
		if (fwd->m_ToDelete)
		{
			fwd->m_ToDelete = false;
			unregisterSPForward(id);
		}
	}
	else
		retVal = m_Forwards[id >> 1]->execute(params, m_TmpArrays);

	m_TmpArraysNum = 0;
	return retVal;
}

const char *CForwardMngr::getFuncName(const int id) const
{
	if (!isIdValid(id))
		return "";

	return (id & 1) ? m_SPForwards[id >> 1]->getFuncName() : m_Forwards[id >> 1]->getFuncName();
}

int CForwardMngr::getFuncsNum(const int id) const
{
	if (!isIdValid(id))
		return 0;

	return (id & 1) ? m_SPForwards[id >> 1]->getFuncsNum() : m_Forwards[id >> 1]->getFuncsNum();
}

int CForwardMngr::getParamsNum(const int id) const
{
	return (id & 1) ? m_SPForwards[id >> 1]->getParamsNum() : m_Forwards[id >> 1]->getParamsNum();
}

ForwardParam CForwardMngr::getParamType(const int id, const int8_t paramNum) const
{
	if (!isIdValid(id))
		return FP_DONE;

	return (id & 1) ? m_SPForwards[id >> 1]->getParamType(paramNum) : m_Forwards[id >> 1]->getParamType(paramNum);
}

void CForwardMngr::clear(void)
{
	size_t i;
	for (i = 0; i < m_Forwards.length(); ++i)
	{
		if (m_Forwards[i] != nullptr)
			delete m_Forwards[i];
	}

	for (i = 0; i < m_SPForwards.length(); ++i)
	{
		if (m_SPForwards[i] != nullptr)
			delete m_SPForwards[i];
	}

	m_Forwards.clear();
	m_SPForwards.clear();
	while (!m_FreeSPForwards.empty())
		m_FreeSPForwards.pop();
	m_TmpArraysNum = 0;
}

bool CForwardMngr::isSPForward(const int id) const
{
	return ((id & 1) == 0) ? false : true;
}

void CForwardMngr::unregisterSPForward(const int id)
{
	// make sure the id is valid
	if (!isIdValid(id) || m_SPForwards.at(id >> 1)->isFree)
		return;

	CSPForward *fwd = m_SPForwards.at(id >> 1);
	if (fwd->m_InExec)
		fwd->m_ToDelete = true;
	else
	{
		fwd->isFree = true;
		m_FreeSPForwards.push(id);
	}
}

int CForwardMngr::duplicateSPForward(const int id)
{
	if (!isIdValid(id) || m_SPForwards.at(id >> 1)->isFree)
		return -1;

	CSPForward *fwd = m_SPForwards.at(id >> 1);
	return registerSPForward(fwd->m_Func, fwd->m_Amx, fwd->m_NumParams, fwd->m_ParamTypes);
}

int CForwardMngr::isSameSPForward(const int id1, const int id2)
{
	if (!isIdValid(id1))
		return false;

	CSPForward *fwd1 = m_SPForwards.at(id1 >> 1);
	if (fwd1->isFree)
		return false;

	if (!isIdValid(id2))
		return false;

	CSPForward* fwd2 = m_SPForwards.at(id2 >> 1);
	if (fwd2->isFree)
		return false;

	return ((fwd1->m_Amx == fwd2->m_Amx) && (fwd1->m_Func == fwd2->m_Func) && (fwd1->m_NumParams == fwd2->m_NumParams));
}

int registerForwardC(const char *funcName, const ForwardExecType et, cell *list, const int8_t num)
{
	int8_t i;
	ForwardParam params[FORWARD_MAX_PARAMS];
	for (i = 0; i < num; i++)
		params[i] = static_cast<ForwardParam>(list[i]);
	
	return g_forwards.registerForward(funcName, et, num, params);
}

int registerForward(const char *funcName, ForwardExecType et, ...)
{
	int8_t curParam = 0;
	va_list argptr;
	va_start(argptr, et);
	ForwardParam params[FORWARD_MAX_PARAMS];
	ForwardParam tmp;
	
	while (true)
	{
		if (curParam == FORWARD_MAX_PARAMS)
			break;
		
		tmp = static_cast<ForwardParam>(va_arg(argptr, int8_t));
		if (tmp == FP_DONE)
			break;
		
		params[curParam] = tmp;
		++curParam;
	}
	
	va_end(argptr);
	return g_forwards.registerForward(funcName, et, curParam, params);
}

int registerSPForwardByNameC(AMX *amx, const char *funcName, cell *list, const int8_t num)
{
	int8_t i;
	ForwardParam params[FORWARD_MAX_PARAMS];
	for (i = 0; i < num; i++)
		params[i] = static_cast<ForwardParam>(list[i]);

	return g_forwards.registerSPForward(funcName, amx, num, params);
}

int registerSPForwardByName(AMX *amx, const char *funcName, ...)
{
	int8_t curParam = 0;
	va_list argptr;
	va_start(argptr, funcName);
	ForwardParam params[FORWARD_MAX_PARAMS];
	ForwardParam tmp;
	
	while (true)
	{
		if (curParam == FORWARD_MAX_PARAMS)
			break;
		
		tmp = static_cast<ForwardParam>(va_arg(argptr, int8_t));
		if (tmp == FP_DONE)
			break;
		
		params[curParam] = tmp;
		++curParam;
	}
	
	va_end(argptr);
	return g_forwards.registerSPForward(funcName, amx, curParam, params);
}

int registerSPForward(AMX *amx, const cell func, ...)
{
	int8_t curParam = 0;
	va_list argptr;
	va_start(argptr, func);
	ForwardParam params[FORWARD_MAX_PARAMS];
	ForwardParam tmp;
	
	while (true)
	{
		if (curParam == FORWARD_MAX_PARAMS)
			break;
		
		tmp = static_cast<ForwardParam>(va_arg(argptr, int8_t));
		if (tmp == FP_DONE)
			break;
		
		params[curParam] = tmp;
		++curParam;
	}
	
	va_end(argptr);
	return g_forwards.registerSPForward(func, amx, curParam, params);
}

cell executeForwards(const int id, ...)
{
	if (!g_forwards.isIdValid(id))
		return -1;

	cell params[FORWARD_MAX_PARAMS];
	const int8_t paramsNum = g_forwards.getParamsNum(id);
	
	va_list argptr;
	va_start(argptr, id);

	int8_t i;
	ForwardParam param_type;
	for (i = 0; i < paramsNum && i < FORWARD_MAX_PARAMS; ++i)
	{
		param_type = g_forwards.getParamType(id, i);
		if (param_type == FP_FLOAT)
		{
			REAL tmp = static_cast<REAL>(va_arg(argptr, double)); // floats get converted to doubles
			params[i] = amx_ftoc(tmp);
		}
		else if(param_type == FP_FLOAT_BYREF)
			params[i] = reinterpret_cast<cell>(reinterpret_cast<REAL*>(va_arg(argptr, double*)));
		else if(param_type == FP_CELL_BYREF)
			params[i] = reinterpret_cast<cell>(reinterpret_cast<cell*>(va_arg(argptr, cell*)));
		else
			params[i] = static_cast<cell>(va_arg(argptr, cell));
	}
	
	va_end(argptr);
	return g_forwards.executeForwards(id, params);
}

cell CForwardMngr::prepareArray(void *ptr, unsigned int size, const ForwardArrayElemType type, const bool copyBack)
{
	if (m_TmpArraysNum >= FORWARD_MAX_PARAMS)
	{
#ifdef MEMORY_TEST
		m_validateAllAllocUnits();
#endif // MEMORY_TEST
		
		AMXXLOG_Log("[AMXX] Forwards with more than 32 parameters are not supported (tried to prepare array # %d).", m_TmpArraysNum + 1);
		m_TmpArraysNum = 0;
		
		return -1;
	}
	
	m_TmpArrays[m_TmpArraysNum].ptr = ptr;
	m_TmpArrays[m_TmpArraysNum].size = size;
	m_TmpArrays[m_TmpArraysNum].type = type;
	m_TmpArrays[m_TmpArraysNum].copyBack = copyBack;
	return m_TmpArraysNum++;
}

cell prepareCellArray(cell *ptr, const unsigned int size, const bool copyBack)
{
	return g_forwards.prepareArray((void*)ptr, size, Type_Cell, copyBack);
}

cell prepareCharArray(char *ptr, const unsigned int size, const bool copyBack)
{
	return g_forwards.prepareArray((void*)ptr, size, Type_Char, copyBack);
}

void unregisterSPForward(const int id)
{
	g_forwards.unregisterSPForward(id);
}
