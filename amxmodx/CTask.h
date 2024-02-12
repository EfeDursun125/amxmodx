// vim: set ts=4 sw=4 tw=99 noet:
//
// AMX Mod X, based on AMX Mod by Aleksander Naszko ("OLO").
// Copyright (C) The AMX Mod X Development Team.
//
// This software is licensed under the GNU General Public License, version 3 or higher.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://alliedmods.net/amxmodx-license

#ifndef CTASK_H
#define CTASK_H

class CTaskMngr
{
private:
	/*** class CTask ***/
	class CTask
	{
		// task settings
		CPluginMngr::CPlugin *m_pPlugin;
		cell m_iId;
		cell m_iFunc;
		int16_t m_iRepeat; // map change will save us :)
		
		bool m_bInExecute;
		bool m_bLoop;
		bool m_bAfterStart;
		bool m_bBeforeEnd;
		float m_fBase; // for normal tasks, stores the interval, for the others, stores the amount of time before start / after end
		int8_t m_iParamLen;
		
		cell *m_pParams;
		bool m_bFree;

		// execution
		float m_fNextExecTime;
	public:
		void set(CPluginMngr::CPlugin *pPlugin, const cell iFunc, const int iFlags, cell iId, const float fBase, const int8_t iParamsLen, const cell *pParams, const int16_t iRepeat, const float fCurrentTime);
		void clear(void);
		bool isFree(void) const;
		inline CPluginMngr::CPlugin *getPlugin(void) const { return m_pPlugin; }
		inline AMX *getAMX(void) const { return m_pPlugin->getAMX(); }
		inline int getTaskId(void) const { return m_iId; }
		void executeIfRequired(const float fCurrentTime, const float fTimeLimit, const float fTimeLeft);	// also removes the task if needed
		void changeBase(const float fNewBase);
		void resetNextExecTime(const float fCurrentTime);
		inline bool inExecute(void) const { return m_bInExecute; }
		bool shouldRepeat(void);
		inline bool match(const int id, AMX *amx)
		{
			return (!m_bFree) && (amx ? getAMX() == amx : true) && (m_iId == id);
		}

		CTask(void);
		~CTask(void);
	};

	/*** CTaskMngr priv members ***/
	ke::Vector<ke::AutoPtr<CTask>> m_Tasks;
	float *m_pTmr_CurrentTime;
	float *m_pTmr_TimeLimit;
	float *m_pTmr_TimeLeft;
public:
	CTaskMngr(void);
	~CTaskMngr(void);
	void registerTimers(float *pCurrentTime, float *pTimeLimit, float *pTimeLeft);	// The timers will always point to the right value
	void registerTask(CPluginMngr::CPlugin *pPlugin, const int iFunc, const int iFlags, const cell iId, const float fBase, const int iParamsLen, const cell *pParams, const int iRepeat);
	int removeTasks(const int iId, AMX *pAmx);											// remove all tasks that match the id and amx
	int changeTasks(const int iId, AMX *pAmx, const float fNewBase);							// change all tasks that match the id and amx
	bool taskExists(const int iId, AMX *pAmx);
	void startFrame(void);
	void clear(void);
};

#endif //CTASK_H
