/*
 * The C RunTime DLL
 * 
 * Implements C run-time functionality as known from UNIX.
 *
 * Copyright 1996,1998 Marcus Meissner
 * Copyright 1996 Jukka Iivonen
 * Copyright 1997,2000 Uwe Bonnes
 * Copyright 2000 Jon Griffiths
 */

/*
Unresolved issues Uwe Bonnes 970904:
- tested with ftp://ftp.remcomp.com/pub/remcomp/lcc-win32.zip, a C-Compiler
 		for Win32, based on lcc, from Jacob Navia
UB 000416:
- probably not thread safe
*/

/* NOTE: This file also implements the wcs* functions. They _ARE_ in 
 * the newer Linux libcs, but use 4 byte wide characters, so are unusable,
 * since we need 2 byte wide characters. - Marcus Meissner, 981031
 */

#include "config.h"

#include "crtdll.h"

#ifdef HAVE_IEEEFP_H
#include <ieeefp.h>
#endif

#include <ctype.h>
#define __USE_ISOC9X 1
#define __USE_ISOC99 1
#include <math.h>
#include <errno.h>
#include <stdlib.h>
#include "ntddk.h"
#include "wingdi.h"
#include "winuser.h"

#ifndef HAVE_FINITE
#ifndef finite /* Could be macro */
#ifdef isfinite 
#define finite(x) isfinite(x) 
#else
#define finite(x) (!isnan(x)) /* At least catch some cases */
#endif
#endif
#endif

#ifndef signbit
#define signbit(x) 0
#endif

DEFAULT_DEBUG_CHANNEL(crtdll);

double CRTDLL_HUGE_dll;       /* CRTDLL.20 */
UINT CRTDLL_argc_dll;         /* CRTDLL.23 */
LPSTR *CRTDLL_argv_dll;       /* CRTDLL.24 */
LPSTR  CRTDLL_acmdln_dll;     /* CRTDLL.38 */
UINT CRTDLL_basemajor_dll;    /* CRTDLL.42 */
UINT CRTDLL_baseminor_dll;    /* CRTDLL.43 */
UINT CRTDLL_baseversion_dll;  /* CRTDLL.44 */
UINT CRTDLL_commode_dll;      /* CRTDLL.59 */
LPSTR  CRTDLL_environ_dll;    /* CRTDLL.75 */
UINT CRTDLL_fmode_dll;        /* CRTDLL.104 */
UINT CRTDLL_osmajor_dll;      /* CRTDLL.241 */
UINT CRTDLL_osminor_dll;      /* CRTDLL.242 */
UINT CRTDLL_osmode_dll;       /* CRTDLL.243 */
UINT CRTDLL_osver_dll;        /* CRTDLL.244 */
UINT CRTDLL_osversion_dll;    /* CRTDLL.245 */
LONG CRTDLL_timezone_dll = 0; /* CRTDLL.304 */
UINT CRTDLL_winmajor_dll;     /* CRTDLL.329 */
UINT CRTDLL_winminor_dll;     /* CRTDLL.330 */
UINT CRTDLL_winver_dll;       /* CRTDLL.331 */
INT  CRTDLL_doserrno = 0; 
INT  CRTDLL_errno = 0;
INT  CRTDLL__mb_cur_max_dll = 1;
const INT  CRTDLL__sys_nerr = 43;

/* ASCII char classification flags - binary compatible */
#define _C_ CRTDLL_CONTROL
#define _S_ CRTDLL_SPACE
#define _P_ CRTDLL_PUNCT
#define _D_ CRTDLL_DIGIT
#define _H_ CRTDLL_HEX
#define _U_ CRTDLL_UPPER
#define _L_ CRTDLL_LOWER

WORD CRTDLL_ctype [257] = {
  0, _C_, _C_, _C_, _C_, _C_, _C_, _C_, _C_, _C_, _S_|_C_, _S_|_C_,
  _S_|_C_, _S_|_C_, _S_|_C_, _C_, _C_, _C_, _C_, _C_, _C_, _C_, _C_,
  _C_, _C_, _C_, _C_, _C_, _C_, _C_, _C_, _C_, _C_, _S_|CRTDLL_BLANK,
  _P_, _P_, _P_, _P_, _P_, _P_, _P_, _P_, _P_, _P_, _P_, _P_, _P_, _P_,
  _P_, _D_|_H_, _D_|_H_, _D_|_H_, _D_|_H_, _D_|_H_, _D_|_H_, _D_|_H_,
  _D_|_H_, _D_|_H_, _D_|_H_, _P_, _P_, _P_, _P_, _P_, _P_, _P_, _U_|_H_,
  _U_|_H_, _U_|_H_, _U_|_H_, _U_|_H_, _U_|_H_, _U_, _U_, _U_, _U_, _U_,
  _U_, _U_, _U_, _U_, _U_, _U_, _U_, _U_, _U_, _U_, _U_, _U_, _U_, _U_,
  _U_, _P_, _P_, _P_, _P_, _P_, _P_, _L_|_H_, _L_|_H_, _L_|_H_, _L_|_H_,
  _L_|_H_, _L_|_H_, _L_, _L_, _L_, _L_, _L_, _L_, _L_, _L_, _L_, _L_,
  _L_, _L_, _L_, _L_, _L_, _L_, _L_, _L_, _L_, _L_, _P_, _P_, _P_, _P_,
  _C_, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* Internal: Current ctype table for locale */
WORD __CRTDLL_current_ctype[257];
 
/* pctype is used by macros in the Win32 headers. It must point
 * To a table of flags exactly like ctype. To allow locale
 * changes to affect ctypes (i.e. isleadbyte), we use a second table
 * and update its flags whenever the current locale changes.
 */
WORD* CRTDLL_pctype_dll = __CRTDLL_current_ctype + 1;


/*********************************************************************
 *                  CRTDLL_MainInit  (CRTDLL.init)
 */
BOOL WINAPI CRTDLL_Init(HINSTANCE hinstDLL,DWORD fdwReason,LPVOID lpvReserved)
{
	TRACE("(0x%08x,%ld,%p)\n",hinstDLL,fdwReason,lpvReserved);

	if (fdwReason == DLL_PROCESS_ATTACH) {
	  __CRTDLL__init_io();
	  __CRTDLL_init_console();
	  CRTDLL_setlocale( CRTDLL_LC_ALL, "C" );
	  CRTDLL_HUGE_dll = HUGE_VAL;
	}
	else if (fdwReason == DLL_PROCESS_DETACH)
        {
	  CRTDLL__fcloseall();
	  __CRTDLL_free_console();
	}

	return TRUE;
}


/* INTERNAL: Set the crt and dos errno's from the OS error given. */
void __CRTDLL__set_errno(ULONG err)
{
  /* FIXME: not MT safe */
  CRTDLL_doserrno = err;

  switch(err)
  {
#define ERR_CASE(oserr) case oserr:
#define ERR_MAPS(oserr,crterr) case oserr:CRTDLL_errno = crterr;break;
    ERR_CASE(ERROR_ACCESS_DENIED)
    ERR_CASE(ERROR_NETWORK_ACCESS_DENIED)
    ERR_CASE(ERROR_CANNOT_MAKE)
    ERR_CASE(ERROR_SEEK_ON_DEVICE)
    ERR_CASE(ERROR_LOCK_FAILED)
    ERR_CASE(ERROR_FAIL_I24)
    ERR_CASE(ERROR_CURRENT_DIRECTORY)
    ERR_CASE(ERROR_DRIVE_LOCKED)
    ERR_CASE(ERROR_NOT_LOCKED)
    ERR_CASE(ERROR_INVALID_ACCESS)
    ERR_MAPS(ERROR_LOCK_VIOLATION,       EACCES);
    ERR_CASE(ERROR_FILE_NOT_FOUND)
    ERR_CASE(ERROR_NO_MORE_FILES)
    ERR_CASE(ERROR_BAD_PATHNAME)
    ERR_CASE(ERROR_BAD_NETPATH)
    ERR_CASE(ERROR_INVALID_DRIVE)
    ERR_CASE(ERROR_BAD_NET_NAME)
    ERR_CASE(ERROR_FILENAME_EXCED_RANGE)
    ERR_MAPS(ERROR_PATH_NOT_FOUND,       ENOENT);
    ERR_MAPS(ERROR_IO_DEVICE,            EIO);
    ERR_MAPS(ERROR_BAD_FORMAT,           ENOEXEC);
    ERR_MAPS(ERROR_INVALID_HANDLE,       EBADF);
    ERR_CASE(ERROR_OUTOFMEMORY)
    ERR_CASE(ERROR_INVALID_BLOCK)
    ERR_CASE(ERROR_NOT_ENOUGH_QUOTA);
    ERR_MAPS(ERROR_ARENA_TRASHED,        ENOMEM);
    ERR_MAPS(ERROR_BUSY,                 EBUSY);
    ERR_CASE(ERROR_ALREADY_EXISTS)
    ERR_MAPS(ERROR_FILE_EXISTS,          EEXIST);
    ERR_MAPS(ERROR_BAD_DEVICE,           ENODEV);
    ERR_MAPS(ERROR_TOO_MANY_OPEN_FILES,  EMFILE);
    ERR_MAPS(ERROR_DISK_FULL,            ENOSPC);
    ERR_MAPS(ERROR_BROKEN_PIPE,          EPIPE);
    ERR_MAPS(ERROR_POSSIBLE_DEADLOCK,    EDEADLK);
    ERR_MAPS(ERROR_DIR_NOT_EMPTY,        ENOTEMPTY);
    ERR_MAPS(ERROR_BAD_ENVIRONMENT,      E2BIG);
    ERR_CASE(ERROR_WAIT_NO_CHILDREN)
    ERR_MAPS(ERROR_CHILD_NOT_COMPLETE,   ECHILD);
    ERR_CASE(ERROR_NO_PROC_SLOTS)
    ERR_CASE(ERROR_MAX_THRDS_REACHED)
    ERR_MAPS(ERROR_NESTING_NOT_ALLOWED,  EAGAIN);
  default:
    /*  Remaining cases map to EINVAL */
    /* FIXME: may be missing some errors above */
    CRTDLL_errno = EINVAL;
  }
}

#if defined(__GNUC__) && defined(__i386__)
#define FPU_DOUBLE(var) double var; \
  __asm__ __volatile__( "fstpl %0;fwait" : "=m" (var) : )
#define FPU_DOUBLES(var1,var2) double var1,var2; \
  __asm__ __volatile__( "fstpl %0;fwait" : "=m" (var2) : ); \
  __asm__ __volatile__( "fstpl %0;fwait" : "=m" (var1) : )
#else
#define FPU_DOUBLE(var) double var = sqrt(-1); \
  FIXME(":not implemented\n");
#define FPU_DOUBLES(var1,var2) double var1,var2; \
  var1=var2=sqrt(-1); FIXME(":not implemented\n")
#endif

/*********************************************************************
 *                  _CIacos             (CRTDLL.004)
 */
double __cdecl CRTDLL__CIacos(void)
{
  FPU_DOUBLE(x);
  if (x < -1.0 || x > 1.0 || !finite(x)) CRTDLL_errno = EDOM;
  return acos(x);
}


/*********************************************************************
 *                  _CIasin             (CRTDLL.005)
 */
double __cdecl CRTDLL__CIasin(void)
{
  FPU_DOUBLE(x);
  if (x < -1.0 || x > 1.0 || !finite(x)) CRTDLL_errno = EDOM;
  return asin(x);
}


/*********************************************************************
 *                  _CIatan             (CRTDLL.006)
 */
double __cdecl CRTDLL__CIatan(void)
{
  FPU_DOUBLE(x);
  if (!finite(x)) CRTDLL_errno = EDOM;
  return atan(x);
}

/*********************************************************************
 *                  _CIatan2            (CRTDLL.007)
 */
double __cdecl CRTDLL__CIatan2(void)
{
  FPU_DOUBLES(x,y);
  if (!finite(x)) CRTDLL_errno = EDOM;
  return atan2(x,y);
}


/*********************************************************************
 *                  _CIcos             (CRTDLL.008)
 */
double __cdecl CRTDLL__CIcos(void)
{
  FPU_DOUBLE(x);
  if (!finite(x)) CRTDLL_errno = EDOM;
  return cos(x);
}

/*********************************************************************
 *                  _CIcosh            (CRTDLL.009)
 */
double __cdecl CRTDLL__CIcosh(void)
{
  FPU_DOUBLE(x);
  if (!finite(x)) CRTDLL_errno = EDOM;
  return cosh(x);
}

/*********************************************************************
 *                  _CIexp             (CRTDLL.010)
 */
double __cdecl CRTDLL__CIexp(void)
{
  FPU_DOUBLE(x);
  if (!finite(x)) CRTDLL_errno = EDOM;
  return exp(x);
}

/*********************************************************************
 *                  _CIfmod            (CRTDLL.011)
 */
double __cdecl CRTDLL__CIfmod(void)
{
  FPU_DOUBLES(x,y);
  if (!finite(x) || !finite(y)) CRTDLL_errno = EDOM;
  return fmod(x,y);
}

/*********************************************************************
 *                  _CIlog             (CRTDLL.012)
 */
double __cdecl CRTDLL__CIlog(void)
{
  FPU_DOUBLE(x);
  if (x < 0.0 || !finite(x)) CRTDLL_errno = EDOM;
  if (x == 0.0) CRTDLL_errno = ERANGE;
  return log(x);
}

/*********************************************************************
 *                  _CIlog10           (CRTDLL.013)
 */
double __cdecl CRTDLL__CIlog10(void)
{
  FPU_DOUBLE(x);
  if (x < 0.0 || !finite(x)) CRTDLL_errno = EDOM;
  if (x == 0.0) CRTDLL_errno = ERANGE;
  return log10(x);
}

/*********************************************************************
 *                  _CIpow             (CRTDLL.014)
 */
double __cdecl CRTDLL__CIpow(void)
{
  double z;
  FPU_DOUBLES(x,y);
  /* FIXME: If x < 0 and y is not integral, set EDOM */
  z = pow(x,y);
  if (!finite(z)) CRTDLL_errno = EDOM;
  return z;
}

/*********************************************************************
 *                  _CIsin             (CRTDLL.015)
 */
double __cdecl CRTDLL__CIsin(void)
{
  FPU_DOUBLE(x);
  if (!finite(x)) CRTDLL_errno = EDOM;
  return sin(x);
}

/*********************************************************************
 *                  _CIsinh            (CRTDLL.016)
 */
double __cdecl CRTDLL__CIsinh(void)
{
  FPU_DOUBLE(x);
  if (!finite(x)) CRTDLL_errno = EDOM;
  return sinh(x);
}

/*********************************************************************
 *                  _CIsqrt            (CRTDLL.017)
 */
double __cdecl CRTDLL__CIsqrt(void)
{
  FPU_DOUBLE(x);
  if (x < 0.0 || !finite(x)) CRTDLL_errno = EDOM;
  return sqrt(x);
}

/*********************************************************************
 *                  _CItan             (CRTDLL.018)
 */
double __cdecl CRTDLL__CItan(void)
{
  FPU_DOUBLE(x);
  if (!finite(x)) CRTDLL_errno = EDOM;
  return tan(x);
}

/*********************************************************************
 *                  _CItanh            (CRTDLL.019)
 */
double __cdecl CRTDLL__CItanh(void)
{
  FPU_DOUBLE(x);
  if (!finite(x)) CRTDLL_errno = EDOM;
  return tanh(x);
}

/*********************************************************************
 *                  _GetMainArgs  (CRTDLL.022)
 */
LPSTR * __cdecl CRTDLL__GetMainArgs(LPDWORD argc,LPSTR **argv,
                                LPSTR *environ,DWORD flag)
{
        char *cmdline;
        char  **xargv;
	int	xargc,end,last_arg,afterlastspace;
	DWORD	version;

	TRACE("(%p,%p,%p,%ld).\n",
		argc,argv,environ,flag
	);

	if (CRTDLL_acmdln_dll != NULL)
		HeapFree(GetProcessHeap(), 0, CRTDLL_acmdln_dll);

	CRTDLL_acmdln_dll = cmdline = CRTDLL__strdup( GetCommandLineA() );
 	TRACE("got '%s'\n", cmdline);

	version	= GetVersion();
	CRTDLL_osver_dll       = version >> 16;
	CRTDLL_winminor_dll    = version & 0xFF;
	CRTDLL_winmajor_dll    = (version>>8) & 0xFF;
	CRTDLL_baseversion_dll = version >> 16;
	CRTDLL_winver_dll      = ((version >> 8) & 0xFF) + ((version & 0xFF) << 8);
	CRTDLL_baseminor_dll   = (version >> 16) & 0xFF;
	CRTDLL_basemajor_dll   = (version >> 24) & 0xFF;
	CRTDLL_osversion_dll   = version & 0xFFFF;
	CRTDLL_osminor_dll     = version & 0xFF;
	CRTDLL_osmajor_dll     = (version>>8) & 0xFF;

	/* missing threading init */

	end=0;last_arg=0;xargv=NULL;xargc=0;afterlastspace=0;
	while (1)
	{
	    if ((cmdline[end]==' ') || (cmdline[end]=='\0'))
	    {
		if (cmdline[end]=='\0')
		    last_arg=1;
		else
		    cmdline[end]='\0';
		/* alloc xargc + NULL entry */
			xargv=(char**)HeapReAlloc( GetProcessHeap(), 0, xargv,
		                             sizeof(char*)*(xargc+1));
		if (strlen(cmdline+afterlastspace))
		{
		    xargv[xargc] = CRTDLL__strdup(cmdline+afterlastspace);
		    xargc++;
                    if (!last_arg) /* need to seek to the next arg ? */
		    {
			end++;
			while (cmdline[end]==' ')
			    end++;
	}
		    afterlastspace=end;
		}
		else
		{
		    xargv[xargc] = NULL; /* the last entry is NULL */
		    break;
		}
	    }
	    else
		end++;
	}
	CRTDLL_argc_dll	= xargc;
	*argc		= xargc;
	CRTDLL_argv_dll	= xargv;
	*argv		= xargv;

	TRACE("found %d arguments\n",
		CRTDLL_argc_dll);
	CRTDLL_environ_dll = *environ = GetEnvironmentStringsA();
	return environ;
}



/*********************************************************************
 *                  _clearfp         (CRTDLL.056)
 *
 * Clear and return the previous FP status.
 */
UINT __cdecl CRTDLL__clearfp( VOID )
{
  UINT retVal = CRTDLL__statusfp();
#if defined(__GNUC__) && defined(__i386__)
  __asm__ __volatile__( "fnclex" );
#else
  FIXME(":Not Implemented!\n");
#endif
  return retVal;
}

/*********************************************************************
 *                  _fpclass         (CRTDLL.105)
 *
 * Return the FP classification of d.
 */
INT __cdecl CRTDLL__fpclass(double d)
{
#if defined(HAVE_FPCLASS) || defined(fpclass)
  switch (fpclass( d ))
  {
  case FP_SNAN:  return _FPCLASS_SNAN;
  case FP_QNAN:  return _FPCLASS_QNAN;
  case FP_NINF:  return _FPCLASS_NINF;
  case FP_PINF:  return _FPCLASS_PINF;
  case FP_NDENORM: return _FPCLASS_ND;
  case FP_PDENORM: return _FPCLASS_PD;
  case FP_NZERO: return _FPCLASS_NZ;
  case FP_PZERO: return _FPCLASS_PZ;
  case FP_NNORM: return _FPCLASS_NN;
  }
  return _FPCLASS_PN;
#elif defined (fpclassify)
  switch (fpclassify( d ))
  {
  case FP_NAN: return _FPCLASS_QNAN;
  case FP_INFINITE: return signbit(d) ? _FPCLASS_NINF : _FPCLASS_PINF;
  case FP_SUBNORMAL: return signbit(d) ?_FPCLASS_ND : _FPCLASS_PD;
  case FP_ZERO: return signbit(d) ? _FPCLASS_NZ : _FPCLASS_PZ;
  }
  return signbit(d) ? _FPCLASS_NN : _FPCLASS_PN;
#else
  if (!finite(d))
    return _FPCLASS_QNAN;
  return d == 0.0 ? _FPCLASS_PZ : (d < 0 ? _FPCLASS_NN : _FPCLASS_PN);
#endif
}


/*********************************************************************
 *                  _initterm     (CRTDLL.135)
 */
DWORD __cdecl CRTDLL__initterm(_INITTERMFUN *start,_INITTERMFUN *end)
{
	_INITTERMFUN	*current;

	TRACE("(%p,%p)\n",start,end);
	current=start;
	while (current<end) {
		if (*current) (*current)();
		current++;
	}
	return 0;
}


/*******************************************************************
 *         _global_unwind2  (CRTDLL.129)
 */
void __cdecl CRTDLL__global_unwind2( PEXCEPTION_FRAME frame )
{
    RtlUnwind( frame, 0, NULL, 0 );
}


/*******************************************************************
 *         _local_unwind2  (CRTDLL.173)
 */
void __cdecl CRTDLL__local_unwind2( PEXCEPTION_FRAME endframe, DWORD nr )
{
    TRACE("(%p,%ld)\n",endframe,nr);
}


/*******************************************************************
 *         _setjmp  (CRTDLL.264)
 */
INT __cdecl CRTDLL__setjmp(LPDWORD *jmpbuf)
{
  FIXME(":(%p): stub\n",jmpbuf);
  return 0;
}


/*********************************************************************
 *                  _beep          (CRTDLL.045)
 *
 * Output a tone using the PC speaker.
 *
 * PARAMS
 * freq [in]     Frequency of the tone
 *
 * duration [in] Length of time the tone should sound
 *
 * RETURNS
 * None.
 */
void __cdecl CRTDLL__beep( UINT freq, UINT duration)
{
    TRACE(":Freq %d, Duration %d\n",freq,duration);
    Beep(freq, duration);
}


/*********************************************************************
 *                  rand          (CRTDLL.446)
 */
INT __cdecl CRTDLL_rand()
{
    return (rand() & CRTDLL_RAND_MAX); 
}


/*********************************************************************
 *                  _rotl          (CRTDLL.259)
 */
UINT __cdecl CRTDLL__rotl(UINT x,INT shift)
{
    shift &= 31;
    return (x << shift) | (x >> (32-shift));
}


/*********************************************************************
 *                  _logb           (CRTDLL.174)
 */
double __cdecl CRTDLL__logb(double x)
{
  if (!finite(x)) CRTDLL_errno = EDOM;
  return logb(x);
}


/*********************************************************************
 *                  _lrotl          (CRTDLL.175)
 */
DWORD __cdecl CRTDLL__lrotl(DWORD x,INT shift)
{
    shift &= 31;
    return (x << shift) | (x >> (32-shift));
}


/*********************************************************************
 *                  _lrotr          (CRTDLL.176)
 */
DWORD __cdecl CRTDLL__lrotr(DWORD x,INT shift)
{
    shift &= 0x1f;
    return (x >> shift) | (x << (32-shift));
}


/*********************************************************************
 *                  _rotr          (CRTDLL.258)
 */
DWORD __cdecl CRTDLL__rotr(UINT x,INT shift)
{
    shift &= 0x1f;
    return (x >> shift) | (x << (32-shift));
}


/*********************************************************************
 *                  _scalb          (CRTDLL.259)
 *
 * Return x*2^y.
 */
double  __cdecl CRTDLL__scalb(double x, LONG y)
{
  /* Note - Can't forward directly as libc expects y as double */
  double y2 = (double)y;
  if (!finite(x)) CRTDLL_errno = EDOM;
  return scalb( x, y2 );
}


/*********************************************************************
 *                  longjmp        (CRTDLL.426)
 */
VOID __cdecl CRTDLL_longjmp(jmp_buf env, int val)
{
    FIXME("CRTDLL_longjmp semistup, expect crash\n");
    longjmp(env, val);
}


/*********************************************************************
 *                  _isctype           (CRTDLL.138)
 */
INT __cdecl CRTDLL__isctype(INT c, UINT type)
{
  if (c >= -1 && c <= 255)
    return CRTDLL_pctype_dll[c] & type;

  if (CRTDLL__mb_cur_max_dll != 1 && c > 0)
  {
    /* FIXME: Is there a faster way to do this? */
    WORD typeInfo;
    char convert[3], *pconv = convert;

    if (CRTDLL_pctype_dll[(UINT)c >> 8] & CRTDLL_LEADBYTE)
      *pconv++ = (UINT)c >> 8;
    *pconv++ = c & 0xff;
    *pconv = 0;
    /* FIXME: Use ctype LCID */
    if (GetStringTypeExA(__CRTDLL_current_lc_all_lcid, CT_CTYPE1,
                         convert, convert[1] ? 2 : 1, &typeInfo))
      return typeInfo & type;
  }
  return 0;
}


/* INTERNAL: Helper for _fullpath. Modified PD code from 'snippets'. */
static void fln_fix(char *path)
{
  int dir_flag = 0, root_flag = 0;
  char *r, *p, *q, *s;

  /* Skip drive */
  if (NULL == (r = strrchr(path, ':')))
    r = path;
  else
    ++r;

  /* Ignore leading slashes */
  while ('\\' == *r)
    if ('\\' == r[1])
      strcpy(r, &r[1]);
    else
    {
      root_flag = 1;
      ++r;
    }

  p = r; /* Change "\\" to "\" */
  while (NULL != (p = strchr(p, '\\')))
    if ('\\' ==  p[1])
      strcpy(p, &p[1]);
    else
      ++p;

  while ('.' == *r) /* Scrunch leading ".\" */
  {
    if ('.' == r[1])
    {
      /* Ignore leading ".." */
      for (p = (r += 2); *p && (*p != '\\'); ++p)
	;
    }
    else
    {
      for (p = r + 1 ;*p && (*p != '\\'); ++p)
	;
    }
    strcpy(r, p + ((*p) ? 1 : 0));
  }

  while ('\\' == path[strlen(path)-1])   /* Strip last '\\' */
  {
    dir_flag = 1;
    path[strlen(path)-1] = '\0';
  }

  s = r;

  /* Look for "\." in path */

  while (NULL != (p = strstr(s, "\\.")))
  {
    if ('.' == p[2])
    {
      /* Execute this section if ".." found */
      q = p - 1;
      while (q > r)           /* Backup one level           */
      {
	if (*q == '\\')
	  break;
	--q;
      }
      if (q > r)
      {
	strcpy(q, p + 3);
	s = q;
      }
      else if ('.' != *q)
      {
	strcpy(q + ((*q == '\\') ? 1 : 0),
	       p + 3 + ((*(p + 3)) ? 1 : 0));
	s = q;
      }
      else  s = ++p;
    }
    else
    {
      /* Execute this section if "." found */
      q = p + 2;
      for ( ;*q && (*q != '\\'); ++q)
	;
      strcpy (p, q);
    }
  }

  if (root_flag)  /* Embedded ".." could have bubbled up to root  */
  {
    for (p = r; *p && ('.' == *p || '\\' == *p); ++p)
      ;
    if (r != p)
      strcpy(r, p);
  }

  if (dir_flag)
    strcat(path, "\\");
}


/*********************************************************************
 *                  _fullpath
 *
 * Convert a partial path into a complete, normalised path.
 */
LPSTR __cdecl CRTDLL__fullpath(LPSTR absPath, LPCSTR relPath, INT size)
{
  char drive[5],dir[MAX_PATH],file[MAX_PATH],ext[MAX_PATH];
  char res[MAX_PATH];
  size_t len;

  res[0] = '\0';

  if (!relPath || !*relPath)
    return CRTDLL__getcwd(absPath, size);

  if (size < 4)
  {
    CRTDLL_errno = ERANGE;
    return NULL;
  }

  TRACE(":resolving relative path '%s'\n",relPath);

  CRTDLL__splitpath(relPath, drive, dir, file, ext);

  /* Get Directory and drive into 'res' */
  if (!dir[0] || (dir[0] != '/' && dir[0] != '\\'))
  {
    /* Relative or no directory given */
    CRTDLL__getdcwd(drive[0] ? toupper(drive[0]) - 'A' + 1 :  0, res, MAX_PATH);
    strcat(res,"\\");
    if (dir[0])
      strcat(res,dir);
    if (drive[0])
      res[0] = drive[0]; /* If given a drive, preserve the letter case */
  }
  else
  {
    strcpy(res,drive);
    strcat(res,dir);
  }

  strcat(res,"\\");
  strcat(res, file);
  strcat(res, ext);
  fln_fix(res);

  len = strlen(res);
  if (len >= MAX_PATH || len >= (size_t)size)
    return NULL; /* FIXME: errno? */

  if (!absPath)
    return CRTDLL__strdup(res);
  strcpy(absPath,res);
  return absPath;
}


/*********************************************************************
 *                  _splitpath           (CRTDLL.279)
 *
 * Split a path string into components.
 *
 * PARAMETERS
 * inpath [in]       Path to split
 * drive [out]       "x:" or ""
 * directory [out]   "\dir", "\dir\", "/dir", "/dir/", "./" etc
 * filename [out]    filename, without dot or slashes
 * extension [out]   ".ext" or ""
 */
VOID __cdecl CRTDLL__splitpath(LPCSTR inpath, LPSTR drv, LPSTR dir,
                               LPSTR fname, LPSTR ext )
{
  /* Modified PD code from 'snippets' collection. */
  char ch, *ptr, *p;
  char pathbuff[MAX_PATH],*path=pathbuff;

  TRACE(":splitting path '%s'\n",path);
  strcpy(pathbuff, inpath);

  /* convert slashes to backslashes for searching */
  for (ptr = (char*)path; *ptr; ++ptr)
    if ('/' == *ptr)
      *ptr = '\\';

  /* look for drive spec */
  if ('\0' != (ptr = strchr(path, ':')))
  {
    ++ptr;
    if (drv)
    {
      strncpy(drv, path, ptr - path);
      drv[ptr - path] = '\0';
    }
    path = ptr;
  }
  else if (drv)
    *drv = '\0';

  /* find rightmost backslash or leftmost colon */
  if (NULL == (ptr = strrchr(path, '\\')))
    ptr = (strchr(path, ':'));

  if (!ptr)
  {
    ptr = (char *)path; /* no path */
    if (dir)
      *dir = '\0';
  }
  else
  {
    ++ptr; /* skip the delimiter */
    if (dir)
    {
      ch = *ptr;
      *ptr = '\0';
      strcpy(dir, path);
      *ptr = ch;
    }
  }

  if (NULL == (p = strrchr(ptr, '.')))
  {
    if (fname)
      strcpy(fname, ptr);
    if (ext)
      *ext = '\0';
  }
  else
  {
    *p = '\0';
    if (fname)
      strcpy(fname, ptr);
    *p = '.';
    if (ext)
      strcpy(ext, p);
  }

  /* Fix pathological case - Win returns ':' as part of the 
   * directory when no drive letter is given.
   */
  if (drv && drv[0] == ':')
  {
    *drv = '\0';
    if (dir)
    {
      pathbuff[0] = ':';
      pathbuff[1] = '\0';
      strcat(pathbuff,dir);
      strcpy(dir,pathbuff);
    }
  }
}


/*********************************************************************
 *                  _matherr            (CRTDLL.181)
 *
 * Default handler for math errors.
*/
INT __cdecl CRTDLL__matherr(struct _exception *e)
{
  /* FIXME: Supposedly this can be user overridden, but
   * currently it will never be called anyway. User will
   * need to use .spec ignore directive to override.
   */
  FIXME(":Unhandled math error!\n");
  return e == NULL ? 0 : 0;
}


/*********************************************************************
 *                  _makepath           (CRTDLL.182)
 */
VOID __cdecl CRTDLL__makepath(LPSTR path, LPCSTR drive,
                              LPCSTR directory, LPCSTR filename,
                              LPCSTR extension )
{
    char ch;
    TRACE("CRTDLL__makepath got %s %s %s %s\n", drive, directory,
          filename, extension);

    if ( !path )
        return;

    path[0] = 0;
    if (drive && drive[0])
    {
        path[0] = drive[0];
        path[1] = ':';
        path[2] = 0;
    }
    if (directory && directory[0])
    {
        strcat(path, directory);
        ch = path[strlen(path)-1];
        if (ch != '/' && ch != '\\')
            strcat(path,"\\");
    }
    if (filename && filename[0])
    {
        strcat(path, filename);
        if (extension && extension[0])
        {
            if ( extension[0] != '.' ) {
                strcat(path,".");
            }
            strcat(path,extension);
        }
    }

    TRACE("CRTDLL__makepath returns %s\n",path);
}


/*********************************************************************
 *                  _errno           (CRTDLL.52)
 * Return the address of the CRT errno (Not the libc errno).
 *
 * BUGS
 * Not MT safe.
 */
LPINT __cdecl CRTDLL__errno( VOID )
{
  return &CRTDLL_errno;
}


/*********************************************************************
 *                  __doserrno       (CRTDLL.26)
 * 
 * Return the address of the DOS errno (holding the last OS error).
 *
 * BUGS
 * Not MT safe.
 */
LPINT __cdecl CRTDLL___doserrno( VOID )
{
  return &CRTDLL_doserrno;
}

/**********************************************************************
 *                  _statusfp       (CRTDLL.279)
 *
 * Return the status of the FP control word.
 */
UINT __cdecl CRTDLL__statusfp( VOID )
{
  UINT retVal = 0;
#if defined(__GNUC__) && defined(__i386__)
  UINT fpword;

  __asm__ __volatile__( "fstsw %0" : "=m" (fpword) : );
  if (fpword & 0x1)  retVal |= _SW_INVALID;
  if (fpword & 0x2)  retVal |= _SW_DENORMAL;
  if (fpword & 0x4)  retVal |= _SW_ZERODIVIDE;
  if (fpword & 0x8)  retVal |= _SW_OVERFLOW;
  if (fpword & 0x10) retVal |= _SW_UNDERFLOW;
  if (fpword & 0x20) retVal |= _SW_INEXACT;
#else
  FIXME(":Not implemented!\n");
#endif
  return retVal;
}


/**********************************************************************
 *                  _strerror       (CRTDLL.284)
 *
 * Return a formatted system error message.
 *
 * NOTES
 * The caller does not own the string returned.
 */
extern int sprintf(char *str, const char *format, ...);

LPSTR __cdecl CRTDLL__strerror (LPCSTR err)
{
  static char strerrbuff[256];
  sprintf(strerrbuff,"%s: %s\n",err,CRTDLL_strerror(CRTDLL_errno));
  return strerrbuff;
}


/*********************************************************************
 *                  perror       (CRTDLL.435)
 *
 * Print a formatted system error message to stderr.
 */
VOID __cdecl CRTDLL_perror (LPCSTR err)
{
  char *err_str = CRTDLL_strerror(CRTDLL_errno);
  CRTDLL_fprintf(CRTDLL_stderr,"%s: %s\n",err,err_str);
}
 

/*********************************************************************
 *                  strerror       (CRTDLL.465)
 *
 * Return the text of an error.
 *
 * NOTES
 * The caller does not own the string returned.
 */
extern char *strerror(int errnum); 

LPSTR __cdecl CRTDLL_strerror (INT err)
{
  return strerror(err);
}


/*********************************************************************
 *                  signal           (CRTDLL.455)
 */
LPVOID __cdecl CRTDLL_signal(INT sig, sig_handler_type ptr)
{
    FIXME("(%d %p):stub.\n", sig, ptr);
    return (void*)-1;
}


/*********************************************************************
 *                  _sleep           (CRTDLL.267)
 */
VOID __cdecl CRTDLL__sleep(ULONG timeout)
{
  TRACE("CRTDLL__sleep for %ld milliseconds\n",timeout);
  Sleep((timeout)?timeout:1);
}


/*********************************************************************
 *                  getenv           (CRTDLL.437)
 */
LPSTR __cdecl CRTDLL_getenv(LPCSTR name)
{
     LPSTR environ = GetEnvironmentStringsA();
     LPSTR pp,pos = NULL;
     unsigned int length;

     for (pp = environ; (*pp); pp = pp + strlen(pp) +1)
       {
	 pos =strchr(pp,'=');
	 if (pos)
	   length = pos -pp;
	 else
	   length = strlen(pp);
	 if (!strncmp(pp,name,length)) break;
       }
     if ((pp)&& (pos)) 
       {
	 pp = pos+1;
	 TRACE("got %s\n",pp);
       }
     FreeEnvironmentStringsA( environ );
     return pp;
}


/*********************************************************************
 *                  isalnum          (CRTDLL.442)
 */
INT __cdecl CRTDLL_isalnum(INT c)
{
  return CRTDLL__isctype( c,CRTDLL_ALPHA | CRTDLL_DIGIT );
}


/*********************************************************************
 *                  isalpha          (CRTDLL.443)
 */
INT __cdecl CRTDLL_isalpha(INT c)
{
  return CRTDLL__isctype( c, CRTDLL_ALPHA );
}


/*********************************************************************
 *                  iscntrl          (CRTDLL.444)
 */
INT __cdecl CRTDLL_iscntrl(INT c)
{
  return CRTDLL__isctype( c, CRTDLL_CONTROL );
}


/*********************************************************************
 *                  isdigit          (CRTDLL.445)
 */
INT __cdecl CRTDLL_isdigit(INT c)
{
  return CRTDLL__isctype( c, CRTDLL_DIGIT );
}


/*********************************************************************
 *                  isgraph          (CRTDLL.446)
 */
INT __cdecl CRTDLL_isgraph(INT c)
{
  return CRTDLL__isctype( c, CRTDLL_ALPHA | CRTDLL_DIGIT | CRTDLL_PUNCT );
}


/*********************************************************************
 *                   isleadbyte         (CRTDLL.447)
 */
INT __cdecl CRTDLL_isleadbyte(UCHAR c)
{
    return CRTDLL__isctype( c, CRTDLL_LEADBYTE );
}


/*********************************************************************
 *                  islower          (CRTDLL.447)
 */
INT __cdecl CRTDLL_islower(INT c)
{
  return CRTDLL__isctype( c, CRTDLL_LOWER );
}


/*********************************************************************
 *                  isprint          (CRTDLL.448)
 */
INT __cdecl CRTDLL_isprint(INT c)
{
  return CRTDLL__isctype( c, CRTDLL_ALPHA | CRTDLL_DIGIT |
			  CRTDLL_BLANK | CRTDLL_PUNCT );
}


/*********************************************************************
 *                  ispunct           (CRTDLL.449)
 */
INT __cdecl CRTDLL_ispunct(INT c)
{
  return CRTDLL__isctype( c, CRTDLL_PUNCT );
}


/*********************************************************************
 *                  isspace           (CRTDLL.450)
 */
INT __cdecl CRTDLL_isspace(INT c)
{
  return CRTDLL__isctype( c, CRTDLL_SPACE );
}


/*********************************************************************
 *                  isupper           (CRTDLL.451)
 */
INT __cdecl CRTDLL_isupper(INT c)
{
  return CRTDLL__isctype( c, CRTDLL_UPPER );
}


/*********************************************************************
 *                  isxdigit           (CRTDLL.452)
 */
INT __cdecl CRTDLL_isxdigit(INT c)
{
  return CRTDLL__isctype( c, CRTDLL_HEX );
}


/*********************************************************************
 *                  ldexp            (CRTDLL.454)
 */
double __cdecl CRTDLL_ldexp(double x, LONG y)
{
  double z = ldexp(x,y);

  if (!finite(z))
    CRTDLL_errno = ERANGE;
  else if (z == 0 && signbit(z))
    z = 0.0; /* Convert -0 -> +0 */
  return z;
}

/*********************************************************************
 *                  _except_handler2  (CRTDLL.78)
 */
INT __cdecl CRTDLL__except_handler2 (
	PEXCEPTION_RECORD rec,
	PEXCEPTION_FRAME frame,
	PCONTEXT context,
	PEXCEPTION_FRAME  *dispatcher)
{
	FIXME ("exception %lx flags=%lx at %p handler=%p %p %p stub\n",
	rec->ExceptionCode, rec->ExceptionFlags, rec->ExceptionAddress,
	frame->Handler, context, dispatcher);
	return ExceptionContinueSearch;
}


/*********************************************************************
 *                  __isascii           (CRTDLL.028)
 *
 */
INT __cdecl CRTDLL___isascii(INT c)
{
  return isascii((unsigned)c);
}


/*********************************************************************
 *                  __toascii           (CRTDLL.035)
 *
 */
INT __cdecl CRTDLL___toascii(INT c)
{
  return (unsigned)c & 0x7f;
}


/*********************************************************************
 *                  iswascii           (CRTDLL.404)
 *
 */
INT __cdecl CRTDLL_iswascii(LONG c)
{
  return ((unsigned)c < 0x80);
}


/*********************************************************************
 *                  __iscsym           (CRTDLL.029)
 *
 * Is a character valid in a C identifier (a-Z,0-9,_).
 *
 * PARAMS
 *   c       [I]: Character to check
 *
 * RETURNS
 * Non zero if c is valid as t a C identifier.
 */
INT __cdecl CRTDLL___iscsym(UCHAR c)
{
  return (c < 127 && (isalnum(c) || c == '_'));
}


/*********************************************************************
 *                  __iscsymf           (CRTDLL.030)
 *
 * Is a character valid as the first letter in a C identifier (a-Z,_).
 *
 * PARAMS
 *   c [in]  Character to check
 *
 * RETURNS
 *   Non zero if c is valid as the first letter in a C identifier.
 */
INT __cdecl CRTDLL___iscsymf(UCHAR c)
{
  return (c < 127 && (isalpha(c) || c == '_'));
}


/*********************************************************************
 *                  _lfind          (CRTDLL.170)
 *
 * Perform a linear search of an array for an element.
 */
LPVOID __cdecl CRTDLL__lfind(LPCVOID match, LPCVOID start, LPUINT array_size,
			     UINT elem_size, comp_func cf)
{
  UINT size = *array_size;
  if (size)
    do
    {
      if (cf(match, start) == 0)
	return (LPVOID)start; /* found */
      start += elem_size;
    } while (--size);
  return NULL;
}


/*********************************************************************
 *                  _loaddll        (CRTDLL.171)
 *
 * Get a handle to a DLL in memory. The DLL is loaded if it is not already.
 *
 * PARAMS
 * dll [in]  Name of DLL to load.
 *
 * RETURNS
 * Success: A handle to the loaded DLL.
 *
 * Failure: FIXME.
 */
INT __cdecl CRTDLL__loaddll(LPSTR dllname)
{
  return LoadLibraryA(dllname);
}


/*********************************************************************
 *                  _unloaddll        (CRTDLL.313)
 *
 * Free reference to a DLL handle from loaddll().
 *
 * PARAMS
 *   dll [in] Handle to free.
 *
 * RETURNS
 * Success: 0.
 *
 * Failure: Error number.
 */
INT __cdecl CRTDLL__unloaddll(HANDLE dll)
{
  INT err;
  if (FreeLibrary(dll))
    return 0;
  err = GetLastError();
  __CRTDLL__set_errno(err);
  return err;
}


/*********************************************************************
 *                  _lsearch        (CRTDLL.177)
 *
 * Linear search of an array of elements. Adds the item to the array if
 * not found.
 *
 * PARAMS
 *   match [in]      Pointer to element to match
 *   start [in]      Pointer to start of search memory
 *   array_size [in] Length of search array (element count)
 *   elem_size [in]  Size of each element in memory
 *   cf [in]         Pointer to comparison function (like qsort()).
 *
 * RETURNS
 *   Pointer to the location where element was found or added.
 */
LPVOID __cdecl CRTDLL__lsearch(LPVOID match,LPVOID start, LPUINT array_size,
                               UINT elem_size, comp_func cf)
{
  UINT size = *array_size;
  if (size)
    do
    {
      if (cf(match, start) == 0)
	return start; /* found */
      start += elem_size;
    } while (--size);

  /* not found, add to end */
  memcpy(start, match, elem_size);
  array_size[0]++;
  return start;
}


/*********************************************************************
 *                  _itow           (CRTDLL.164)
 *
 * Convert an integer to a wide char string.
 */

extern LPSTR  __cdecl _itoa( long , LPSTR , INT); /* ntdll */

/********************************************************************/

WCHAR* __cdecl CRTDLL__itow(INT value,WCHAR* out,INT base)
{
  char buff[64]; /* FIXME: Whats the maximum buffer size for INT_MAX? */

  _itoa(value, buff, base);
  MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, buff, -1, out, 64);
  return out;
}


/*********************************************************************
 *                  _ltow           (CRTDLL.??)
 *
 * Convert a long to a wide char string.
 */

extern LPSTR  __cdecl _ltoa( long , LPSTR , INT); /* ntdll */

/********************************************************************/

WCHAR* __cdecl CRTDLL__ltow(LONG value,WCHAR* out,INT base)
{
  char buff[64]; /* FIXME: Whats the maximum buffer size for LONG_MAX? */

  _ltoa(value, buff, base);
  MultiByteToWideChar (CP_ACP, MB_PRECOMPOSED, buff, -1, out, 64);
  return out;
}


/*********************************************************************
 *                  _ultow           (CRTDLL.??)
 *
 * Convert an unsigned long to a wide char string.
 */

extern LPSTR  __cdecl _ultoa( long , LPSTR , INT); /* ntdll */

/********************************************************************/

WCHAR* __cdecl CRTDLL__ultow(ULONG value,WCHAR* out,INT base)
{
  char buff[64]; /* FIXME: Whats the maximum buffer size for ULONG_MAX? */

  _ultoa(value, buff, base);
  MultiByteToWideChar (CP_ACP, MB_PRECOMPOSED, buff, -1, out, 64);
  return out;
}


/*********************************************************************
 *                  _toupper           (CRTDLL.489)
 */
CHAR __cdecl CRTDLL__toupper(CHAR c)
{
  return toupper(c);
}


/*********************************************************************
 *                  _tolower           (CRTDLL.490)
 */
CHAR __cdecl CRTDLL__tolower(CHAR c)
{
  return tolower(c);
}


/* FP functions */

/*********************************************************************
 *                  _cabs           (CRTDLL.048)
 *
 * Return the absolue value of a complex number.
 *
 * PARAMS
 *   c [in] Structure containing real and imaginary parts of complex number.
 *
 * RETURNS
 *   Absolute value of complex number (always a positive real number).
 */
double __cdecl CRTDLL__cabs(struct complex c)
{
  return sqrt(c.real * c.real + c.imaginary * c.imaginary);
}


/*********************************************************************
 *                  _chgsign    (CRTDLL.053)
 *
 * Change the sign of an IEEE double.
 *
 * PARAMS
 *   d [in] Number to invert.
 *
 * RETURNS
 *   Number with sign inverted.
 */
double __cdecl CRTDLL__chgsign(double d)
{
  /* FIXME: +-infinity,Nan not tested */
  return -d;
}


/*********************************************************************
 *                  _control87    (CRTDLL.060)
 *
 * X86 implementation of _controlfp.
 *
 */
UINT __cdecl CRTDLL__control87(UINT newVal, UINT mask)
{
#if defined(__GNUC__) && defined(__i386__)
   UINT fpword, flags = 0;

  /* Get fp control word */
  __asm__ __volatile__( "fstsw %0" : "=m" (fpword) : );

  /* Convert into mask constants */
  if (fpword & 0x1)  flags |= _EM_INVALID;
  if (fpword & 0x2)  flags |= _EM_DENORMAL;
  if (fpword & 0x4)  flags |= _EM_ZERODIVIDE;
  if (fpword & 0x8)  flags |= _EM_OVERFLOW;
  if (fpword & 0x10) flags |= _EM_UNDERFLOW;
  if (fpword & 0x20) flags |= _EM_INEXACT;
  switch(fpword & 0xC00) {
  case 0xC00: flags |= _RC_UP|_RC_DOWN; break;
  case 0x800: flags |= _RC_UP; break;
  case 0x400: flags |= _RC_DOWN; break;
  }
  switch(fpword & 0x300) {
  case 0x0:   flags |= _PC_24; break;
  case 0x200: flags |= _PC_53; break;
  case 0x300: flags |= _PC_64; break;
  }
  if (fpword & 0x1000) flags |= _IC_AFFINE;

  /* Mask with parameters */
  flags = (flags & ~mask) | (newVal & mask);

  /* Convert (masked) value back to fp word */
  fpword = 0;
  if (flags & _EM_INVALID)    fpword |= 0x1;
  if (flags & _EM_DENORMAL)   fpword |= 0x2;
  if (flags & _EM_ZERODIVIDE) fpword |= 0x4;
  if (flags & _EM_OVERFLOW)   fpword |= 0x8;
  if (flags & _EM_UNDERFLOW)  fpword |= 0x10;
  if (flags & _EM_INEXACT)    fpword |= 0x20;
  switch(flags & (_RC_UP | _RC_DOWN)) {
  case _RC_UP|_RC_DOWN: fpword |= 0xC00; break;
  case _RC_UP:          fpword |= 0x800; break;
  case _RC_DOWN:        fpword |= 0x400; break;
  }
  switch (flags & (_PC_24 | _PC_53)) {
  case _PC_64: fpword |= 0x300; break;
  case _PC_53: fpword |= 0x200; break;
  case _PC_24: fpword |= 0x0; break;
  }
  if (!(flags & _IC_AFFINE)) fpword |= 0x1000;

  /* Put fp control word */
  __asm__ __volatile__( "fldcw %0" : : "m" (fpword) );
  return fpword;
#else
  return  CRTDLL__controlfp( newVal, mask );
#endif
}


/*********************************************************************
 *                  _controlfp    (CRTDLL.061)
 *
 * Set the state of the floating point unit.
 */
UINT __cdecl CRTDLL__controlfp( UINT newVal, UINT mask)
{
#if defined(__GNUC__) && defined(__i386__)
  return CRTDLL__control87( newVal, mask );
#else
  FIXME(":Not Implemented!\n");
  return 0;
#endif
}


/*********************************************************************
 *                  _copysign           (CRTDLL.062)
 *
 * Return the number x with the sign of y.
 */
double __cdecl CRTDLL__copysign(double x, double y)
{
  /* FIXME: Behaviour for Nan/Inf etc? */
  if (y < 0.0)
    return x < 0.0 ? x : -x;

  return x < 0.0 ? -x : x;
}


/*********************************************************************
 *                  _finite           (CRTDLL.101)
 *
 * Determine if an IEEE double is finite (i.e. not +/- Infinity).
 *
 * PARAMS
 *   d [in]  Number to check.
 *
 * RETURNS
 *   Non zero if number is finite.
 */
INT __cdecl  CRTDLL__finite(double d)
{
  return (finite(d)?1:0); /* See comment for CRTDLL__isnan() */
}


/*********************************************************************
 *                  _fpreset           (CRTDLL.107)
 *
 * Reset the state of the floating point processor.
 */
VOID __cdecl CRTDLL__fpreset(void)
{
#if defined(__GNUC__) && defined(__i386__)
  __asm__ __volatile__( "fninit" );
#else
  FIXME(":Not Implemented!\n");
#endif
}


/*********************************************************************
 *                  _isnan           (CRTDLL.164)
 *
 * Determine if an IEEE double is unrepresentable (NaN).
 *
 * PARAMS
 *   d [in]  Number to check.
 *
 * RETURNS
 *   Non zero if number is NaN.
 */
INT __cdecl  CRTDLL__isnan(double d)
{
  /* some implementations return -1 for true(glibc), crtdll returns 1.
   * Do the same, as the result may be used in calculations.
   */
  return isnan(d)?1:0;
}


/*********************************************************************
 *                  _purecall           (CRTDLL.249)
 *
 * Abort program after pure virtual function call.
 */
VOID __cdecl CRTDLL__purecall(VOID)
{
  CRTDLL__amsg_exit( 25 );
}


/*********************************************************************
 *                  div               (CRTDLL.358)
 *
 * Return the quotient and remainder of long integer division.
 *
 * VERSION
 *	[i386] Windows binary compatible - returns the struct in eax/edx.
 */
#ifdef __i386__
LONGLONG __cdecl CRTDLL_div(INT x, INT y)
{
  LONGLONG retVal;
  div_t dt = div(x,y);
  retVal = ((LONGLONG)dt.rem << 32) | dt.quot;
  return retVal;
}
#endif /* !defined(__i386__) */


/*********************************************************************
 *                  div               (CRTDLL.358)
 *
 * Return the quotient and remainder of long integer division.
 *
 * VERSION
 *	[!i386] Non-x86 can't run win32 apps so we don't need binary compatibility
 */
#ifndef __i386__
div_t __cdecl CRTDLL_div(INT x, INT y)
{
  return div(x,y);
}
#endif /* !defined(__i386__) */


/*********************************************************************
 *                  ldiv               (CRTDLL.249)
 *
 * Return the quotient and remainder of long integer division.
 * VERSION
 * 	[i386] Windows binary compatible - returns the struct in eax/edx.
 */
#ifdef __i386__
ULONGLONG __cdecl CRTDLL_ldiv(LONG x, LONG y)
{
  ULONGLONG retVal;
  ldiv_t ldt = ldiv(x,y);
  retVal = ((ULONGLONG)ldt.rem << 32) | (ULONG)ldt.quot;
  return retVal;
}
#endif /* defined(__i386__) */


/*********************************************************************
 *                  ldiv               (CRTDLL.249)
 *
 * Return the quotient and remainder of long integer division.
 *
 * VERSION
 *	[!i386] Non-x86 can't run win32 apps so we don't need binary compatibility
 */
#ifndef __i386__
ldiv_t __cdecl CRTDLL_ldiv(LONG x, LONG y)
{
  return ldiv(x,y);
}
#endif /* !defined(__i386__) */


/*********************************************************************
 *                  _y0               (CRTDLL.332)
 *
 */
double __cdecl CRTDLL__y0(double x)
{
  double retVal;

  if (!finite(x)) CRTDLL_errno = EDOM;
  retVal  = y0(x);
  if (CRTDLL__fpclass(retVal) == _FPCLASS_NINF)
  {
    CRTDLL_errno = EDOM;
    retVal = sqrt(-1);
  }
  return retVal;
}

/*********************************************************************
 *                  _y1               (CRTDLL.333)
 *
 */
double __cdecl CRTDLL__y1(double x)
{
  double retVal;

  if (!finite(x)) CRTDLL_errno = EDOM;
  retVal  = y1(x);
  if (CRTDLL__fpclass(retVal) == _FPCLASS_NINF)
  {
    CRTDLL_errno = EDOM;
    retVal = sqrt(-1);
  }
  return retVal;
}

/*********************************************************************
 *                  _yn               (CRTDLL.334)
 *
 */
double __cdecl CRTDLL__yn(INT x, double y)
{
  double retVal;

  if (!finite(y)) CRTDLL_errno = EDOM;
  retVal  = yn(x,y);
  if (CRTDLL__fpclass(retVal) == _FPCLASS_NINF)
  {
    CRTDLL_errno = EDOM;
    retVal = sqrt(-1);
  }
  return retVal;
}


/*********************************************************************
 *                  _nextafter        (CRTDLL.235)
 *
 */
double __cdecl CRTDLL__nextafter(double x, double y)
{
  double retVal;
  if (!finite(x) || !finite(y)) CRTDLL_errno = EDOM;
    retVal  = nextafter(x,y);
  return retVal;
}

/*********************************************************************
 *                  _searchenv        (CRTDLL.260)
 *
 * Search CWD and each directory of an environment variable for
 * location of a file.
 */
VOID __cdecl CRTDLL__searchenv(LPCSTR file, LPCSTR env, LPSTR buff)
{
  LPSTR envVal, penv;
  char curPath[MAX_PATH];

  *buff = '\0';

  /* Try CWD first */
  if (GetFileAttributesA( file ) != 0xFFFFFFFF)
  {
    GetFullPathNameA( file, MAX_PATH, buff, NULL );
    /* Sigh. This error is *always* set, regardless of sucess */
    __CRTDLL__set_errno(ERROR_FILE_NOT_FOUND);
    return;
  }

  /* Search given environment variable */
  envVal = CRTDLL_getenv(env);
  if (!envVal)
  {
    __CRTDLL__set_errno(ERROR_FILE_NOT_FOUND);
    return;
  }

  penv = envVal;
  TRACE(":searching for %s in paths %s\n", file, envVal);

  do
  {
    LPSTR end = penv;

	while(*end && *end != ';') end++; /* Find end of next path */
    if (penv == end || !*penv)
    {
      __CRTDLL__set_errno(ERROR_FILE_NOT_FOUND);
      return;
    }
    strncpy(curPath, penv, end - penv);
    if (curPath[end - penv] != '/' || curPath[end - penv] != '\\')
    {
      curPath[end - penv] = '\\';
      curPath[end - penv + 1] = '\0';
    }
    else
      curPath[end - penv] = '\0';

    strcat(curPath, file);
    TRACE("Checking for file %s\n", curPath);
    if (GetFileAttributesA( curPath ) != 0xFFFFFFFF)
    {
      strcpy(buff, curPath);
      __CRTDLL__set_errno(ERROR_FILE_NOT_FOUND);
      return; /* Found */
    }
    penv = *end ? end + 1 : end;
  } while(1);
}

