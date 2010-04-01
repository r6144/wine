/*
 * msvcr90 specific functions
 *
 * Copyright 2010 Detlef Riekenberg
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(msvcr90);

typedef int (CDECL *_INITTERM_E_FN)(void);
typedef void (__cdecl *_invalid_parameter_handler)(const wchar_t*, const wchar_t*, const wchar_t*, unsigned, unsigned*);

/*********************************************************************
 *  DllMain (MSVCR90.@)
 */
BOOL WINAPI DllMain(HINSTANCE hdll, DWORD reason, LPVOID reserved)
{
    switch (reason)
    {
    case DLL_WINE_PREATTACH:
        return FALSE;  /* prefer native version */

    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hdll);
    }
    return TRUE;
}

/*********************************************************************
 *  _decode_pointer (MSVCR90.@)
 *
 * cdecl version of DecodePointer
 *
 */
void * CDECL MSVCR90_decode_pointer(void * ptr)
{
    return DecodePointer(ptr);
}

/*********************************************************************
 *  _encode_pointer (MSVCR90.@)
 *
 * cdecl version of EncodePointer
 *
 */
void * CDECL MSVCR90_encode_pointer(void * ptr)
{
    return EncodePointer(ptr);
}

/*********************************************************************
 *  _encoded_null (MSVCR90.@)
 */
void * CDECL _encoded_null(void)
{
    TRACE("\n");

    return MSVCR90_encode_pointer(NULL);
}

/*********************************************************************
 *  _get_invalid_parameter_handler (MSVCR90.@)
 */
_invalid_parameter_handler CDECL _get_invalid_parameter_handler(void)
{
    TRACE("\n");
    return *((_invalid_parameter_handler*)GetProcAddress(GetModuleHandleA("msvcrt.dll"), "_invalid_parameter"));
}

/*********************************************************************
 *  _set_invalid_parameter_handler (MSVCR90.@)
 */
_invalid_parameter_handler CDECL _set_invalid_parameter_handler(_invalid_parameter_handler handler)
{
    _invalid_parameter_handler *ptr = (_invalid_parameter_handler*)GetProcAddress(
            GetModuleHandleA("msvcrt.dll"), "_invalid_parameter");
    _invalid_parameter_handler old = *ptr;

    TRACE("(%p)\n", handler);

    *ptr = handler;
    return old;
}

/*********************************************************************
 *  _initterm_e (MSVCR90.@)
 *
 * call an array of application initialization functions and report the return value
 */
int CDECL _initterm_e(_INITTERM_E_FN *table, _INITTERM_E_FN *end)
{
    int res = 0;

    TRACE("(%p, %p)\n", table, end);

    while (!res && table < end) {
        if (*table) {
            res = (**table)();
            if (res)
                TRACE("function %p failed: 0x%x\n", *table, res);

        }
        table++;
    }
    return res;
}

/*********************************************************************
 * __sys_nerr (MSVCR90.@)
 */
int* CDECL __sys_nerr(void)
{
        return (int*)GetProcAddress(GetModuleHandleA("msvcrt.dll"), "_sys_nerr");
}

/*********************************************************************
 *  __sys_errlist (MSVCR90.@)
 */
char** CDECL __sys_errlist(void)
{
    return (char**)GetProcAddress(GetModuleHandleA("msvcrt.dll"), "_sys_errlist");
}
