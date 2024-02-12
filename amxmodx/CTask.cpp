// vim: set ts=4 sw=4 tw=99 noet:
//
// AMX Mod X, based on AMX Mod by Aleksander Naszko ("OLO").
// Copyright (C) The AMX Mod X Development Team.
//
// This software is licensed under the GNU General Public License, version 3 or higher.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://alliedmods.net/amxmodx-license

#include "amxmodx.h"
#include "CTask.h"
#include <clib.h>

/*********************** CTask ***********************/

void CTaskMngr::CTask::set(CPluginMngr::CPlugin *pPlugin, const cell iFunc, const int iFlags, const cell iId, const float fBase, const int8_t iParamsLen, const cell *pParams, const int16_t iRepeat, const float fCurrentTime)
{
	clear();
	m_bFree = false;

	m_pPlugin = pPlugin;
	m_iFunc = iFunc;
	m_iId = iId;
	m_fBase = fBase;
	m_bInExecute = false;

	if (iFlags & 2)
	{
		m_bLoop = true;
		m_iRepeat = -1;
	}
	else if (iFlags & 1)
	{
		m_bLoop = true;
		m_iRepeat = iRepeat;
	}
	
	m_bAfterStart =	(iFlags & 4) ? true : false;
	m_bBeforeEnd = (iFlags & 8) ? true : false;
	m_fNextExecTime = fCurrentTime + m_fBase;

	if (iParamsLen)
	{
		m_iParamLen = iParamsLen + 1;
		m_pParams = new(std::nothrow) cell[m_iParamLen];
		if (m_pParams != nullptr)
		{
			cmemcpy(m_pParams, pParams, sizeof(cell) * iParamsLen);
			m_pParams[iParamsLen] = 0;
		}
	}
	else
	{
		m_iParamLen = 0;
		m_pParams = nullptr;
	}
}

void CTaskMngr::CTask::clear(void)
{
	m_bFree = true;
	if (m_iFunc >= 0)
	{
		unregisterSPForward(m_iFunc);
		m_iFunc = -1;
	}

	if (m_pParams != nullptr)
	{
		delete[] m_pParams;
		m_pParams = nullptr;
	}

	m_pPlugin = nullptr;
	m_iId = 0;
	m_fBase = 0.0f;
	m_iRepeat =	0;
	m_bLoop = false;
	m_bAfterStart =	false;
	m_bBeforeEnd = false;
	m_fNextExecTime = 0.0f;
}

bool CTaskMngr::CTask::isFree(void) const
{
	return m_bFree;
}

void CTaskMngr::CTask::changeBase(const float fNewBase)
{
	m_fBase = fNewBase;
}

void CTaskMngr::CTask::resetNextExecTime(const float fCurrentTime)
{
	// If we're here while we're executing we would add m_fBase twice
	if (!m_bInExecute)
		m_fNextExecTime = fCurrentTime + m_fBase;
}

void CTaskMngr::CTask::executeIfRequired(const float fCurrentTime, const float fTimeLimit, const float fTimeLeft)
{
	bool execute = false;
	if (m_bAfterStart)
	{
		if (fCurrentTime - fTimeLeft + 1.0f >= m_fBase)
			execute = true;
	}
	else if (m_bBeforeEnd)
	{
		if (fTimeLimit != 0.0f && (fTimeLeft + fTimeLimit * 60.0f) - fCurrentTime - 1.0f <= m_fBase)
			execute = true;
	}
	else if (m_fNextExecTime <= fCurrentTime)
		execute = true;

	if (execute)
	{
		//only bother calling if we have something to call
		if (!(m_bLoop && !m_iRepeat))
		{
			m_bInExecute = true;
			if (m_iParamLen) // call with parameters
				executeForwards(m_iFunc, prepareCellArray(m_pParams, m_iParamLen), m_iId);
			else
				executeForwards(m_iFunc, m_iId);
			m_bInExecute = false;
		}

		if (isFree())
			return;

		// set new exec time OR remove the task if needed
		if (m_bLoop)
		{
			if (m_iRepeat != -1 && --m_iRepeat <= 0)
				clear();
			else
				m_fNextExecTime += m_fBase;
		}
		else
			clear();
	}
}

CTaskMngr::CTask::CTask(void)
{
	m_bFree = true;
	m_pPlugin = nullptr;
	m_iFunc = -1;
	m_iId = 0;
	m_fBase = 0.0f;
	m_iRepeat =	0;
	m_bLoop = false;
	m_bAfterStart =	false;
	m_bBeforeEnd = false;
	m_bInExecute = false;
	m_fNextExecTime = 0.0f;
	m_iParamLen = 0;
	m_pParams = nullptr;
}

CTaskMngr::CTask::~CTask(void)
{
	clear();
}

/*********************** CTaskMngr ***********************/

CTaskMngr::CTaskMngr(void)
{
	m_pTmr_CurrentTime = nullptr;
	m_pTmr_TimeLimit = nullptr;
	m_pTmr_TimeLeft = nullptr;
}

CTaskMngr::~CTaskMngr(void)
{
	clear();
}

void CTaskMngr::registerTimers(float *pCurrentTime, float *pTimeLimit, float *pTimeLeft)
{
	m_pTmr_CurrentTime = pCurrentTime;
	m_pTmr_TimeLimit = pTimeLimit;
	m_pTmr_TimeLeft = pTimeLeft;
}

void CTaskMngr::registerTask(CPluginMngr::CPlugin *pPlugin, const int iFunc, const int iFlags, const cell iId, const float fBase, const int iParamsLen, const cell *pParams, const int iRepeat)
{
	// first, search for free tasks
	for (auto &task : m_Tasks)
	{
		if (task->isFree() && !task->inExecute())
		{
			// found: reuse it
			task->set(pPlugin, iFunc, iFlags, iId, fBase, iParamsLen, pParams, iRepeat, *m_pTmr_CurrentTime);
			return;
		}
	}

	// not found: make a new one
	auto task = ke::AutoPtr<CTask>(new(std::nothrow) CTask);
	if (task == nullptr)
		return;
		
	task->set(pPlugin, iFunc, iFlags, iId, fBase, iParamsLen, pParams, iRepeat, *m_pTmr_CurrentTime);
	m_Tasks.append(ke::Move(task));
}

int CTaskMngr::removeTasks(const int iId, AMX *pAmx)
{
	int i = 0;
	for (auto &task : m_Tasks)
	{
		if (task->match(iId, pAmx))
		{
			task->clear();
			++i;
		}
	}
	
	return i;
}

int CTaskMngr::changeTasks(const int iId, AMX *pAmx, const float fNewBase)
{
	int i = 0;
	
	for (auto &task : m_Tasks)
	{
		if (task->match(iId, pAmx))
		{
			task->changeBase(fNewBase);
			task->resetNextExecTime(*m_pTmr_CurrentTime);
			++i;
		}
	}
	
	return i;
}

bool CTaskMngr::taskExists(const int iId, AMX *pAmx)
{
	for (auto &task : m_Tasks)
	{
		if (task->match(iId, pAmx))
			return true;
	}

	return false;
}

void CTaskMngr::startFrame(void)
{
	size_t i;
	auto lastSize = m_Tasks.length();
	for (i = 0u; i < lastSize; i++)
	{
		auto &task = m_Tasks[i];
		if (task->isFree())
			continue;

		task->executeIfRequired(*m_pTmr_CurrentTime, *m_pTmr_TimeLimit, *m_pTmr_TimeLeft);
	}
}

void CTaskMngr::clear(void)
{
	m_Tasks.clear();
}
