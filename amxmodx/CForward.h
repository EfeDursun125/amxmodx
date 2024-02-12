// vim: set ts=4 sw=4 tw=99 noet:
//
// AMX Mod X, based on AMX Mod by Aleksander Naszko ("OLO").
// Copyright (C) The AMX Mod X Development Team.
//
// This software is licensed under the GNU General Public License, version 3 or higher.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://alliedmods.net/amxmodx-license

/*
	CForward.h
	forwards
	1) normal forwards: called in all plugins
	2) single plugin (sp) forwards: called in one plugin

	The SP Forwards are handled differently because they are expected to be created / deleted
	often, but the "normal" forwards are expected to be initialized at start up.

	Note about forward ids:
		for normal forwards:	<index to vector> << 1
		for sp forwards:		(<index to vector> << 1) | 1
*/

#ifndef FORWARD_H
#define FORWARD_H

#include <stdarg.h>
#include "sh_stack.h"

#define FORWARD_MAX_PARAMS 32

enum ForwardExecType : int8_t
{
	ET_IGNORE = 0,					// Ignore return vaue
	ET_STOP,						// Stop on PLUGIN_HANDLED
	ET_STOP2,						// Stop on PLUGIN_HANDLED, continue on other values, return biggest return value
	ET_CONTINUE,					// Continue; return biggest return value
};

enum ForwardParam : int8_t
{
	FP_DONE = -1,					// specify this as the last argument
									// only tells the function that there are no more arguments
	FP_CELL,						// normal cell
	FP_FLOAT,						// float; used as normal cell though
	FP_STRING,						// string
	FP_STRINGEX,					// string; will be updated to the last function's value
	FP_ARRAY,						// array; use the return value of prepareArray.
	FP_CELL_BYREF,                  // cell; pass by reference
	FP_FLOAT_BYREF,                 // float; pass by reference
};

// for prepareArray
enum ForwardArrayElemType : int8_t
{
	Type_Cell = 0,
	Type_Char
};

struct ForwardPreparedArray
{
	void *ptr;
	ForwardArrayElemType type;
	unsigned int size;
	bool copyBack;
};

// Normal forward
class CForward
{
	const char *m_FuncName;
	ForwardExecType m_ExecType;
	int8_t m_NumParams;
	ke::AString m_Name;
	
	struct AMXForward
	{
		CPluginMngr::CPlugin *pPlugin;
		cell func;
	};
	
	typedef ke::Vector<AMXForward> AMXForwardList;
	
	AMXForwardList m_Funcs;
	ForwardParam m_ParamTypes[FORWARD_MAX_PARAMS];

public:
	CForward(const char *name, const ForwardExecType et, const int8_t numParams, const ForwardParam * paramTypes);
	CForward(void) {}		// leaves everything unitialized'
	cell execute(cell *params, ForwardPreparedArray *preparedArrays);
	inline int8_t getParamsNum(void) const { return m_NumParams; }
	inline int getFuncsNum(void) const { return m_Funcs.length(); }
	inline const char *getFuncName(void) const { return m_Name.chars(); }
	inline ForwardParam getParamType(const int8_t paramId) const
	{
		if (paramId < 0 || paramId >= m_NumParams)
			return FP_DONE;
		
		return m_ParamTypes[paramId];
	}
};

// Single plugin forward
class CSPForward
{
	friend class CForwardMngr;
	int8_t m_NumParams;
	ForwardParam m_ParamTypes[FORWARD_MAX_PARAMS];
	AMX *m_Amx;
	cell m_Func;
	bool m_HasFunc;
	ke::AString m_Name;
	bool m_InExec;
	bool m_ToDelete;
public:
	bool isFree;
public:
	CSPForward(void) { m_HasFunc = false; }
	void Set(const char *funcName, AMX *amx, const int8_t numParams, const ForwardParam * paramTypes);
	void Set(const cell func, AMX *amx, const int8_t numParams, const ForwardParam * paramTypes);
	cell execute(cell *params, ForwardPreparedArray *preparedArrays);
	inline int8_t getParamsNum(void) const { return m_NumParams; }
	inline int getFuncsNum(void) const { return m_HasFunc; }
	inline const char *getFuncName(void) const { return m_Name.chars(); }
	inline ForwardParam getParamType(const int8_t paramId) const
	{
		if (paramId < 0 || paramId >= m_NumParams)
			return FP_DONE;
		
		return m_ParamTypes[paramId];
	}
};

class CForwardMngr
{
	typedef ke::Vector<CForward*> ForwardVec;
	typedef ke::Vector<CSPForward*> SPForwardVec;
	typedef CStack<cell> FreeSPVec;							// Free SP Forwards
	ForwardVec m_Forwards;
	SPForwardVec m_SPForwards;
	FreeSPVec m_FreeSPForwards;								// so we don't have to free memory
	ForwardPreparedArray m_TmpArrays[FORWARD_MAX_PARAMS];	// used by prepareArray
	cell m_TmpArraysNum;
public:

	CForwardMngr(void)
	{ m_TmpArraysNum = 0; }
	~CForwardMngr(void) {}

	// Interface
	// Register normal forward
	int registerForward(const char *funcName, const ForwardExecType et, const int8_t numParams, const ForwardParam *paramTypes);
	// Register single plugin forward
	int registerSPForward(const char *funcName, AMX *amx, const int8_t numParams, const ForwardParam * paramTypes);
	int registerSPForward(const cell func, AMX *amx, const int8_t numParams, const ForwardParam * paramTypes);
	
	// Unregister single plugin forward
	void unregisterSPForward(const int id);
	int duplicateSPForward(const int id);
	int isSameSPForward(const int id1, const int id2);
	
	// execute forward
	cell executeForwards(const int id, cell *params);
	void clear(void);							// delete all forwards
	
	inline bool isIdValid(const int id) const { return (id >= 0) && ((id & 1) ? (static_cast<size_t>(id >> 1) < m_SPForwards.length()) : (static_cast<size_t>(id >> 1) < m_Forwards.length())); }
	bool isSPForward(const int id) const;			// check whether forward is single plugin
	int getParamsNum(const int id) const;			// get num of params of a forward
	int getFuncsNum(const int id) const;			// get num of found functions of a forward
	const char *getFuncName(const int id) const;	// get the function name
	
	ForwardParam getParamType(const int id, const int8_t paramId) const;
	cell prepareArray(void *ptr, const unsigned int size, const ForwardArrayElemType type, const bool copyBack);		// prepare array
};

// (un)register forward
int registerForward(const char *funcName, const ForwardExecType et, ...);
int registerForwardC(const char *funcName, const ForwardExecType et, cell *list, const int8_t num);
int registerSPForwardByName(AMX *amx, const char *funcName, ...);
int registerSPForwardByNameC(AMX *amx, const char *funcName, cell *list, const int8_t num);
int registerSPForward(AMX *amx, const int func, ...);
void unregisterSPForward(const int id);

// execute forwards
cell executeForwards(const int id, ...);
// prepare array
cell prepareCellArray(cell *ptr, const unsigned int size, const bool copyBack = false);
cell prepareCharArray(char *ptr, const unsigned int size, const bool copyBack = false);

#endif //FORWARD_H
