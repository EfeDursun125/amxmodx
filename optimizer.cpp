// vim: set ts=4 sw=4 tw=99 noet:
//
// AMX Mod X, based on AMX Mod by Aleksander Naszko ("OLO").
// Copyright (C) The AMX Mod X Development Team.
//
// This software is licensed under the GNU General Public License, version 3 or higher.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://alliedmods.net/amxmodx-license

#include <string.h>
#include "optimizer.h"
#include <new>

int g_opt_level = 0;
#define OP_SYSREQ_C		123
#define OP_NOP			134
#define OP_FLOAT_MUL	138
#define OP_FLOAT_DIV	139
#define OP_FLOAT_ADD	140
#define OP_FLOAT_SUB	141
#define OP_FLOAT_TO		142
#define OP_FLOAT_ROUND	143
#define OP_FLOAT_CMP	144

void OnBrowseRelocate(AMX *amx, cell *oplist, cell *cip)
{
	char *codeptr = (char *)amx->base + (long)(((AMX_HEADER *)amx->base)->cod);

	//jump to the parameter;
	codeptr += *cip;
	
	int native = -1;
	cell n_offs = *(cell *)codeptr;
	optimizer_s *opt = (optimizer_s *)amx->usertags[UT_OPTIMIZER];
	int i;
    for (i = 0; i < N_Total_FloatOps; i++)
	{
		if (opt->natives[i] == n_offs)
		{
			native = i;
			break;
		}
	}

	if (native != -1)
	{
		cell op_trans_table[] =
		{
			OP_FLOAT_MUL,
			OP_FLOAT_DIV,
			OP_FLOAT_ADD,
			OP_FLOAT_SUB,
			OP_FLOAT_TO,
			OP_FLOAT_ROUND,
			OP_FLOAT_CMP
		};

		//we're patching this:
		// 0x7B 0x??   SYSREQ.C float???
		//with:
		// 0x8A        FLOAT.MUL
		// 0x86	       NOP
		cell new_opcodes[2];
		new_opcodes[0] = op_trans_table[native];
		new_opcodes[1] = OP_NOP;
		codeptr -= sizeof(cell);
#if defined __GNUC__ || defined ASM32 || defined JIT
		*(cell *)codeptr = oplist[new_opcodes[0]];
		*(cell *)(codeptr + sizeof(cell)) = oplist[new_opcodes[1]];
#else
		*(cell *)codeptr = new_opcodes[0];
		*(cell *)(codeptr + sizeof(cell)) = new_opcodes[1];
#endif
	}

	*cip += sizeof(cell);
	return;
}

#define FIND_NATIVE(name, bind) \
	if (amx_FindNative(amx, name, &index) != AMX_ERR_NOTFOUND) \
		opt->natives[bind] = index;

void _Setup_Optimizer_Stage2(AMX *amx, cell *oplist, cell *cip)
{
	amx->usertags[UT_BROWSEHOOK] = (void *)OnBrowseRelocate;
	optimizer_s *opt = new(std::nothrow) optimizer_s;
	if (opt == nullptr)
		return;

	int i, index;
	for (i = 0; i < N_Total_FloatOps; i++)
		opt->natives[i] = -1;

	amx->usertags[UT_OPTIMIZER] = (void *)opt;

	if (g_opt_level & 1)
	{
		FIND_NATIVE("floatmul", N_Float_Mul);
		FIND_NATIVE("floatdiv", N_Float_Div);
		FIND_NATIVE("floatadd", N_Float_Add);
		FIND_NATIVE("floatsub", N_Float_Sub);
	}

	if (g_opt_level & 4)
	{
		FIND_NATIVE("float", N_Float_To);
		FIND_NATIVE("floatround", N_Float_Round);
	}

	if (g_opt_level & 2)
	{
#if !defined AMD64
		if (amxx_CpuSupport())
		{
#endif
			FIND_NATIVE("floatcmp", N_Float_Cmp);
#if !defined AMD64
		}
		else
			g_opt_level &= ~(2);
#endif
	}

	/*if (g_opt_level & 8)
	{
		FIND_NATIVE("floatabs", N_Float_Abs);
		FIND_NATIVE("floatsqroot", N_Float_SQRoot);
		FIND_NATIVE("floatrsqroot", N_Float_RSQRoot);
		FIND_NATIVE("floatpower", N_Float_Pow);
		FIND_NATIVE("floatlog", N_Float_Log);
		FIND_NATIVE("floatsin", N_Float_Sin);
		FIND_NATIVE("floatcos", N_Float_Cos);
		FIND_NATIVE("floattan", N_Float_Tan);
		FIND_NATIVE("floatatan2", N_Float_Atan2);
	}*/
}

void SetupOptimizer(AMX *amx)
{
	amx->usertags[UT_BROWSEHOOK] = (void *)_Setup_Optimizer_Stage2;
}

