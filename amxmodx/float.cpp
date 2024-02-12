/*  Float arithmetic for the Small AMX engine
 *
 *  Copyright (c) Artran, Inc. 1999
 *  Written by Greg Garner (gmg@artran.com)
 *  This file may be freely used. No warranties of any kind.
 *
 * CHANGES -
 * 2002-08-27: Basic conversion of source from C++ to C by Adam D. Moss
 *             <adam@gimp.org> <aspirin@icculus.org>
 * 2003-08-29: Removal of the dynamic memory allocation and replacing two
 *             type conversion functions by macros, by Thiadmer Riemersma
 * 2003-09-22: Moved the type conversion macros to AMX.H, and simplifications
 *             of some routines, by Thiadmer Riemersma
 * 2003-11-24: A few more native functions (geometry), plus minor modifications,
 *             mostly to be compatible with dynamically loadable extension
 *             modules, by Thiadmer Riemersma
 * 2004-01-09: Adaptions for 64-bit cells (using "double precision"), by
 *             Thiadmer Riemersma
 */
#include <stdlib.h>     /* for atof() */
#include <stdio.h>      /* for NULL */
#include <assert.h>
#include <cmath>
#include <clib.h>

// this file does not include amxmodx.h, so we have to include the memory manager here
#ifdef MEMORY_TEST
#include "mmgr/mmgr.h"
#endif // MEMORY_TEST

#include "amx.h"

/*
  #if defined __BORLANDC__
    #pragma resource "amxFloat.res"
  #endif
*/

static REAL FromRadians(const REAL angle, const int radix)
{
	switch (radix)
	{
		case 1:         /* degrees, sexagesimal system (technically: degrees/minutes/seconds) */
			return angle / 565.486677646f;
		case 2:         /* grades, centesimal system */
			return angle / 628.318530718f;
		default:        /* assume already radian */
			return angle;
	} /* switch */
}

#if defined __BORLANDC__ || defined __WATCOMC__
  #pragma argsused
#endif
/******************************************************************/
static cell AMX_NATIVE_CALL n_float(AMX* amx, cell* params)
{
    /*
    *   params[0] = number of bytes
    *   params[1] = long value to convert to a float
    */
    const REAL fValue = static_cast<REAL>(params[1]);
    return amx_ftoc(fValue);
}

/******************************************************************/
#if defined __BORLANDC__ || defined __WATCOMC__
  #pragma argsused
#endif
static cell AMX_NATIVE_CALL n_floatstr(AMX* amx, cell* params)
{
    /*
    *   params[0] = number of bytes
    *   params[1] = virtual string address to convert to a float
    */

    /* They should have sent us 1 cell. */
    if (params[0] / sizeof(cell) != 1)
        return 0;

    char szSource[60];
    cell* pString;
    int nLen;

    /* Get the real address of the string. */
    amx_GetAddr(amx,params[1], &pString);

    /* Find out how long the string is in characters. */
    amx_StrLen(pString, &nLen);
    if (nLen == 0 || static_cast<unsigned int>(nLen) >= sizeof(szSource))
        return 0;

    /* Now convert the Small String into a C type null terminated string */
    amx_GetStringOld(szSource, pString, 0);

    /* Now convert this to a float. */
    const REAL fNum = catof(szSource);
    return amx_ftoc(fNum);
}

#if defined __BORLANDC__ || defined __WATCOMC__
  #pragma argsused
#endif
/******************************************************************/
static cell AMX_NATIVE_CALL n_floatmul(AMX *amx,cell *params)
{
    /*
    *   params[0] = number of bytes
    *   params[1] = float operand 1
    *   params[2] = float operand 2
    */
    const REAL fRes = amx_ctof(params[1]) * amx_ctof(params[2]);
    return amx_ftoc(fRes);
}

#if defined __BORLANDC__ || defined __WATCOMC__
  #pragma argsused
#endif
/******************************************************************/
static cell AMX_NATIVE_CALL n_floatdiv(AMX *amx,cell *params)
{
    /*
    *   params[0] = number of bytes
    *   params[1] = float dividend (top)
    *   params[2] = float divisor (bottom)
    */
    const REAL fRes = amx_ctof(params[1]) / amx_ctof(params[2]);
    return amx_ftoc(fRes);
}

#if defined __BORLANDC__ || defined __WATCOMC__
  #pragma argsused
#endif
/******************************************************************/
static cell AMX_NATIVE_CALL n_floatadd(AMX *amx,cell *params)
{
    /*
    *   params[0] = number of bytes
    *   params[1] = float operand 1
    *   params[2] = float operand 2
    */
    const REAL fRes = amx_ctof(params[1]) + amx_ctof(params[2]);
    return amx_ftoc(fRes);
}

#if defined __BORLANDC__ || defined __WATCOMC__
  #pragma argsused
#endif
/******************************************************************/
static cell AMX_NATIVE_CALL n_floatsub(AMX *amx,cell *params)
{
    /*
    *   params[0] = number of bytes
    *   params[1] = float operand 1
    *   params[2] = float operand 2
    */
    const REAL fRes = amx_ctof(params[1]) - amx_ctof(params[2]);
    return amx_ftoc(fRes);
}

#if defined __BORLANDC__ || defined __WATCOMC__
  #pragma argsused
#endif
/******************************************************************/
/* Return fractional part of float */
static cell AMX_NATIVE_CALL n_floatfract(AMX *amx,cell *params)
{
    /*
    *   params[0] = number of bytes
    *   params[1] = float operand
    */
    REAL fA = amx_ctof(params[1]);
    fA = fA - cfloorf(fA);
    return amx_ftoc(fA);
}

#if defined __BORLANDC__ || defined __WATCOMC__
  #pragma argsused
#endif
/******************************************************************/
/* Return integer part of float, rounded */
static cell AMX_NATIVE_CALL n_floatround(AMX *amx,cell *params)
{
    /*
    *   params[0] = number of bytes
    *   params[1] = float operand
    *   params[2] = Type of rounding (long)
    */
    REAL fA = amx_ctof(params[1]);
    switch (params[2])
    {
        case 1:       /* round downwards (truncate) */
        {
            fA = cfloorf(fA);
            break;
        }
        case 2:       /* round upwards */
        {
            fA = cceilf(fA);
            break;
        }
        case 3:       /* round towards zero */
        {
            if (fA > 0.0f)
                fA = cfloorf(fA);
            else
                fA = cceilf(fA);
            break;
        }
        default:      /* standard, round to nearest */
        {
            fA = croundf(fA);
            break;
        }
    }

    return (long)fA;
}

#if defined __BORLANDC__ || defined __WATCOMC__
  #pragma argsused
#endif
/******************************************************************/
static cell AMX_NATIVE_CALL n_floatcmp(AMX *amx,cell *params)
{
    /*
    *   params[0] = number of bytes
    *   params[1] = float operand 1
    *   params[2] = float operand 2
    */
    const REAL fA = amx_ctof(params[1]);
    const REAL fB = amx_ctof(params[2]);

    if (fA == fB)
        return 0;
    else if (fA > fB)
        return 1;
    else
        return -1;
}

/******************************************************************/
static cell AMX_NATIVE_CALL n_floatsqroot(AMX *amx,cell *params)
{
    /*
    *   params[0] = number of bytes
    *   params[1] = float operand
    */
    REAL fA = amx_ctof(params[1]);
    fA = csqrtf(fA);
    return amx_ftoc(fA);
}

/******************************************************************/
static cell AMX_NATIVE_CALL n_floatrsqroot(AMX* amx, cell* params)
{
    /*
    *   params[0] = number of bytes
    *   params[1] = float operand
    */
    REAL fA = amx_ctof(params[1]);
    fA = crsqrtf(fA);
    return amx_ftoc(fA);
}

#if defined __BORLANDC__ || defined __WATCOMC__
  #pragma argsused
#endif
/******************************************************************/
static cell AMX_NATIVE_CALL n_floatpower(AMX *amx,cell *params)
{
    /*
    *   params[0] = number of bytes
    *   params[1] = float operand 1 (base)
    *   params[2] = float operand 2 (exponent)
    */
    REAL fA = amx_ctof(params[1]);
    REAL fB = amx_ctof(params[2]);
    fA = cpowf(fA, fB);
    return amx_ftoc(fA);
}

#if defined __BORLANDC__ || defined __WATCOMC__
  #pragma argsused
#endif
/******************************************************************/
static cell AMX_NATIVE_CALL n_floatlog(AMX *amx,cell *params)
{
    /*
    *   params[0] = number of bytes
    *   params[1] = float operand 1 (value)
    *   params[2] = float operand 2 (base)
    */
    REAL fValue = amx_ctof(params[1]);
    REAL fBase = amx_ctof(params[2]);
    if (fValue <= 0.0f || fBase <= 0.0f)
        return amx_RaiseError(amx, AMX_ERR_DOMAIN);

    if (fBase == 10.0f) // ??? epsilon
        fValue = log10f(fValue);
    else
        fValue = clogf(fValue) / clogf(fBase);
    return amx_ftoc(fValue);
}

static REAL ToRadians(const REAL angle, const int radix)
{
    switch (radix)
    {
        case 1:         /* degrees, sexagesimal system (technically: degrees/minutes/seconds) */
            return angle * 0.01745329251f;
        case 2:         /* grades, centesimal system */
            return angle * 0.01570796326f;
        default:        /* assume already radian */
            return angle;
    } /* switch */
}

#if defined __BORLANDC__ || defined __WATCOMC__
  #pragma argsused
#endif
/******************************************************************/
static cell AMX_NATIVE_CALL n_floatsin(AMX *amx,cell *params)
{
    /*
    *   params[0] = number of bytes
    *   params[1] = float operand 1 (angle)
    *   params[2] = float operand 2 (radix)
    */
    REAL fA = amx_ctof(params[1]);
    fA = ToRadians(fA, params[2]);
    fA = csinf(fA);
    return amx_ftoc(fA);
}

#if defined __BORLANDC__ || defined __WATCOMC__
  #pragma argsused
#endif
/******************************************************************/
static cell AMX_NATIVE_CALL n_floatcos(AMX *amx,cell *params)
{
    /*
    *   params[0] = number of bytes
    *   params[1] = float operand 1 (angle)
    *   params[2] = float operand 2 (radix)
    */
    REAL fA = amx_ctof(params[1]);
    fA = ToRadians(fA, params[2]);
    fA = ccosf(fA);
    return amx_ftoc(fA);
}

#if defined __BORLANDC__ || defined __WATCOMC__
  #pragma argsused
#endif
/******************************************************************/
static cell AMX_NATIVE_CALL n_floattan(AMX *amx,cell *params)
{
    /*
    *   params[0] = number of bytes
    *   params[1] = float operand 1 (angle)
    *   params[2] = float operand 2 (radix)
    */
    REAL fA = amx_ctof(params[1]);
    fA = ToRadians(fA, params[2]);
    fA = ctanf(fA);
    return amx_ftoc(fA);
}

#if defined __BORLANDC__ || defined __WATCOMC__
  #pragma argsused
#endif
/* Added by BAILOPAN */
static cell AMX_NATIVE_CALL n_floatatan(AMX *amx, cell *params)
{
	/*
	 * params[1] = angle
	 * params[2] = radix
	 */
	REAL fA = amx_ctof(params[1]);
	fA = atan(fA);
	fA = FromRadians(fA, params[2]);
	return amx_ftoc(fA);
}

#if defined __BORLANDC__ || defined __WATCOMC__
  #pragma argsused
#endif
/* Added by BAILOPAN */
static cell AMX_NATIVE_CALL n_floatacos(AMX *amx, cell *params)
{
	/*
	 * params[1] = angle
	 * params[2] = radix
	 */
	REAL fA = amx_ctof(params[1]);
	fA = acos(fA);
	fA = FromRadians(fA, params[2]);
	return amx_ftoc(fA);
}

#if defined __BORLANDC__ || defined __WATCOMC__
  #pragma argsused
#endif
/* Added by BAILOPAN */
static cell AMX_NATIVE_CALL n_floatasin(AMX *amx, cell *params)
{
	/*
	 * params[1] = angle
	 * params[2] = radix
	 */
	REAL fA = amx_ctof(params[1]);
	fA = asin(fA);
	fA = FromRadians(fA, params[2]);
	return amx_ftoc(fA);
}

#if defined __BORLANDC__ || defined __WATCOMC__
  #pragma argsused
#endif
/* Added by BAILOPAN */
static cell AMX_NATIVE_CALL n_floatatan2(AMX *amx, cell *params)
{
	/*
	 * params[1] = x
	 * params[2] = y
	 * params[3] = radix
	 */
	REAL fA = amx_ctof(params[1]);
	REAL fB = amx_ctof(params[2]);
	REAL fC;
	fC = catan2f(fA, fB);
	fC = FromRadians(fC, params[3]);
	return amx_ftoc(fC);
}

#if defined __BORLANDC__ || defined __WATCOMC__
  #pragma argsused
#endif
/* Added by DS */
static cell AMX_NATIVE_CALL n_floatsinh(AMX *amx, cell *params)
{
	/*
	 * params[1] = angle
	 * params[2] = radix
	 */
	REAL fA = amx_ctof(params[1]);
	fA = ToRadians(fA, params[2]);
	fA = sinh(fA);
	return amx_ftoc(fA);
}

#if defined __BORLANDC__ || defined __WATCOMC__
  #pragma argsused
#endif
/* Added by DS */
static cell AMX_NATIVE_CALL n_floatcosh(AMX *amx, cell *params)
{
	/*
	 * params[1] = angle
	 * params[2] = radix
	 */
	REAL fA = amx_ctof(params[1]);
	fA = ToRadians(fA, params[2]);
	fA = cosh(fA);
	return amx_ftoc(fA);
}

#if defined __BORLANDC__ || defined __WATCOMC__
  #pragma argsused
#endif
/* Added by DS */
static cell AMX_NATIVE_CALL n_floattanh(AMX *amx, cell *params)
{
	/*
	 * params[1] = angle
	 * params[2] = radix
	 */
	REAL fA = amx_ctof(params[1]);
	fA = ToRadians(fA, params[2]);
	fA = tanh(fA);
	return amx_ftoc(fA);
}

#if defined __BORLANDC__ || defined __WATCOMC__
  #pragma argsused
#endif
/******************************************************************/
static cell AMX_NATIVE_CALL n_floatabs(AMX *amx,cell *params)
{
    REAL fA = amx_ctof(params[1]);
    fA = cabsf(fA);
    return amx_ftoc(fA);
}

AMX_NATIVE_INFO float_Natives[] = {
  { "float",        n_float       },
  { "floatstr",     n_floatstr    },
  { "floatmul",     n_floatmul    },
  { "floatdiv",     n_floatdiv    },
  { "floatadd",     n_floatadd    },
  { "floatsub",     n_floatsub    },
  { "floatfract",   n_floatfract  },
  { "floatround",   n_floatround  },
  { "floatcmp",     n_floatcmp    },
  { "floatsqroot",  n_floatsqroot },
  { "floatrsqroot", n_floatrsqroot},
  { "floatpower",   n_floatpower  },
  { "floatlog",     n_floatlog    },
  { "floatsin",     n_floatsin    },
  { "floatcos",     n_floatcos    },
  { "floattan",     n_floattan    },
  { "floatabs",     n_floatabs    },
  { "floatasin",    n_floatasin   },
  { "floatacos",    n_floatacos   },
  { "floatatan",    n_floatatan   },
  { "floatatan2",   n_floatatan2  },
  { "floatsinh",    n_floatsinh   },
  { "floatcosh",    n_floatcosh   },
  { "floattanh",    n_floattanh   },
  { nullptr, nullptr } /* terminator */
};

int AMXEXPORT amx_FloatInit(AMX *amx)
{
  return amx_Register(amx,float_Natives,-1);
}

#if defined __BORLANDC__ || defined __WATCOMC__
  #pragma argsused
#endif
int AMXEXPORT amx_FloatCleanup(AMX *amx)
{
  return AMX_ERR_NONE;
}
