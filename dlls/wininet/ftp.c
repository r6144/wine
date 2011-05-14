/*
 * WININET - Ftp implementation
 *
 * Copyright 1999 Corel Corporation
 * Copyright 2004 Mike McCormack for CodeWeavers
 * Copyright 2004 Kevin Koltzau
 * Copyright 2007 Hans Leidekker
 *
 * Ulrich Czekalla
 * Noureddine Jemmali
 *
 * Copyright 2000 Andreas Mohr
 * Copyright 2002 Jaco Greeff
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

#include "config.h"
#include "wine/port.h"

#if defined(__MINGW32__) || defined (_MSC_VER)
#include <ws2tcpip.h>
#endif

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif
#include <time.h>
#include <assert.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "wininet.h"
#include "winnls.h"
#include "winerror.h"
#include "winreg.h"
#include "winternl.h"
#include "shlwapi.h"

#include "wine/debug.h"
#include "internet.h"

WINE_DEFAULT_DEBUG_CHANNEL(wininet);

typedef struct _ftp_session_t ftp_session_t;

typedef struct
{
    object_header_t hdr;
    ftp_session_t *lpFtpSession;
    BOOL session_deleted;
    int nDataSocket;
    WCHAR *cache_file;
    HANDLE cache_file_handle;
} ftp_file_t;

struct _ftp_session_t
{
    object_header_t hdr;
    appinfo_t *lpAppInfo;
    int sndSocket;
    int lstnSocket;
    int pasvSocket; /* data socket connected by us in case of passive FTP */
    ftp_file_t *download_in_progress;
    struct sockaddr_in socketAddress;
    struct sockaddr_in lstnSocketAddress;
    LPWSTR servername;
    INTERNET_PORT serverport;
    LPWSTR  lpszPassword;
    LPWSTR  lpszUserName;
};

typedef struct
{
    BOOL bIsDirectory;
    LPWSTR lpszName;
    DWORD nSize;
    SYSTEMTIME tmLastModified;
    unsigned short permissions;
} FILEPROPERTIESW, *LPFILEPROPERTIESW;

typedef struct
{
    object_header_t hdr;
    ftp_session_t *lpFtpSession;
    DWORD index;
    DWORD size;
    LPFILEPROPERTIESW lpafp;
} WININETFTPFINDNEXTW, *LPWININETFTPFINDNEXTW;

#define DATA_PACKET_SIZE 	0x2000
#define szCRLF 			"\r\n"
#define MAX_BACKLOG 		5

/* Testing shows that Windows only accepts dwFlags where the last
 * 3 (yes 3) bits define FTP_TRANSFER_TYPE_UNKNOWN, FTP_TRANSFER_TYPE_ASCII or FTP_TRANSFER_TYPE_BINARY.
 */
#define FTP_CONDITION_MASK      0x0007

typedef enum {
  /* FTP commands with arguments. */
  FTP_CMD_ACCT,
  FTP_CMD_CWD,
  FTP_CMD_DELE,
  FTP_CMD_MKD,
  FTP_CMD_PASS,
  FTP_CMD_PORT,
  FTP_CMD_RETR,
  FTP_CMD_RMD,
  FTP_CMD_RNFR,
  FTP_CMD_RNTO,
  FTP_CMD_STOR,
  FTP_CMD_TYPE,
  FTP_CMD_USER,
  FTP_CMD_SIZE,

  /* FTP commands without arguments. */
  FTP_CMD_ABOR,
  FTP_CMD_LIST,
  FTP_CMD_NLST,
  FTP_CMD_PASV,
  FTP_CMD_PWD,
  FTP_CMD_QUIT,
} FTP_COMMAND;

static const CHAR *const szFtpCommands[] = {
  "ACCT",
  "CWD",
  "DELE",
  "MKD",
  "PASS",
  "PORT",
  "RETR",
  "RMD",
  "RNFR",
  "RNTO",
  "STOR",
  "TYPE",
  "USER",
  "SIZE",
  "ABOR",
  "LIST",
  "NLST",
  "PASV",
  "PWD",
  "QUIT",
};

static const CHAR szMonths[] = "JANFEBMARAPRMAYJUNJULAUGSEPOCTNOVDEC";
static const WCHAR szNoAccount[] = {'n','o','a','c','c','o','u','n','t','\0'};

static BOOL FTP_SendCommand(INT nSocket, FTP_COMMAND ftpCmd, LPCWSTR lpszParam,
	INTERNET_STATUS_CALLBACK lpfnStatusCB, object_header_t *hdr, DWORD_PTR dwContext);
static BOOL FTP_SendStore(ftp_session_t*, LPCWSTR lpszRemoteFile, DWORD dwType);
static BOOL FTP_GetDataSocket(ftp_session_t*, LPINT nDataSocket);
static BOOL FTP_SendData(ftp_session_t*, INT nDataSocket, HANDLE hFile);
static INT FTP_ReceiveResponse(ftp_session_t*, DWORD_PTR dwContext);
static BOOL FTP_SendRetrieve(ftp_session_t*, LPCWSTR lpszRemoteFile, DWORD dwType);
static BOOL FTP_RetrieveFileData(ftp_session_t*, INT nDataSocket, HANDLE hFile);
static BOOL FTP_InitListenSocket(ftp_session_t*);
static BOOL FTP_ConnectToHost(ftp_session_t*);
static BOOL FTP_SendPassword(ftp_session_t*);
static BOOL FTP_SendAccount(ftp_session_t*);
static BOOL FTP_SendType(ftp_session_t*, DWORD dwType);
static BOOL FTP_SendPort(ftp_session_t*);
static BOOL FTP_DoPassive(ftp_session_t*);
static BOOL FTP_SendPortOrPasv(ftp_session_t*);
static BOOL FTP_ParsePermission(LPCSTR lpszPermission, LPFILEPROPERTIESW lpfp);
static BOOL FTP_ParseNextFile(INT nSocket, LPCWSTR lpszSearchFile, LPFILEPROPERTIESW fileprop);
static BOOL FTP_ParseDirectory(ftp_session_t*, INT nSocket, LPCWSTR lpszSearchFile,
        LPFILEPROPERTIESW *lpafp, LPDWORD dwfp);
static HINTERNET FTP_ReceiveFileList(ftp_session_t*, INT nSocket, LPCWSTR lpszSearchFile,
        LPWIN32_FIND_DATAW lpFindFileData, DWORD_PTR dwContext);
static DWORD FTP_SetResponseError(DWORD dwResponse);
static BOOL FTP_ConvertFileProp(LPFILEPROPERTIESW lpafp, LPWIN32_FIND_DATAW lpFindFileData);
static BOOL FTP_FtpPutFileW(ftp_session_t*, LPCWSTR lpszLocalFile,
        LPCWSTR lpszNewRemoteFile, DWORD dwFlags, DWORD_PTR dwContext);
static BOOL FTP_FtpSetCurrentDirectoryW(ftp_session_t*, LPCWSTR lpszDirectory);
static BOOL FTP_FtpCreateDirectoryW(ftp_session_t*, LPCWSTR lpszDirectory);
static HINTERNET FTP_FtpFindFirstFileW(ftp_session_t*,
        LPCWSTR lpszSearchFile, LPWIN32_FIND_DATAW lpFindFileData, DWORD dwFlags, DWORD_PTR dwContext);
static BOOL FTP_FtpGetCurrentDirectoryW(ftp_session_t*, LPWSTR lpszCurrentDirectory,
        LPDWORD lpdwCurrentDirectory);
static BOOL FTP_FtpRenameFileW(ftp_session_t*, LPCWSTR lpszSrc, LPCWSTR lpszDest);
static BOOL FTP_FtpRemoveDirectoryW(ftp_session_t*, LPCWSTR lpszDirectory);
static BOOL FTP_FtpDeleteFileW(ftp_session_t*, LPCWSTR lpszFileName);
static BOOL FTP_FtpGetFileW(ftp_session_t*, LPCWSTR lpszRemoteFile, LPCWSTR lpszNewFile,
        BOOL fFailIfExists, DWORD dwLocalFlagsAttribute, DWORD dwInternetFlags,
        DWORD_PTR dwContext);

/* A temporary helper until we get rid of INTERNET_GetLastError calls */
static BOOL res_to_le(DWORD res)
{
    if(res != ERROR_SUCCESS)
        INTERNET_SetLastError(res);
    return res == ERROR_SUCCESS;
}

/***********************************************************************
 *           FtpPutFileA (WININET.@)
 *
 * Uploads a file to the FTP server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI FtpPutFileA(HINTERNET hConnect, LPCSTR lpszLocalFile,
    LPCSTR lpszNewRemoteFile, DWORD dwFlags, DWORD_PTR dwContext)
{
    LPWSTR lpwzLocalFile;
    LPWSTR lpwzNewRemoteFile;
    BOOL ret;
    
    lpwzLocalFile = heap_strdupAtoW(lpszLocalFile);
    lpwzNewRemoteFile = heap_strdupAtoW(lpszNewRemoteFile);
    ret = FtpPutFileW(hConnect, lpwzLocalFile, lpwzNewRemoteFile,
                      dwFlags, dwContext);
    HeapFree(GetProcessHeap(), 0, lpwzLocalFile);
    HeapFree(GetProcessHeap(), 0, lpwzNewRemoteFile);
    return ret;
}

static void AsyncFtpPutFileProc(WORKREQUEST *workRequest)
{
    struct WORKREQ_FTPPUTFILEW const *req = &workRequest->u.FtpPutFileW;
    ftp_session_t *lpwfs = (ftp_session_t*) workRequest->hdr;

    TRACE("%p\n", lpwfs);

    FTP_FtpPutFileW(lpwfs, req->lpszLocalFile,
               req->lpszNewRemoteFile, req->dwFlags, req->dwContext);

    HeapFree(GetProcessHeap(), 0, req->lpszLocalFile);
    HeapFree(GetProcessHeap(), 0, req->lpszNewRemoteFile);
}

/***********************************************************************
 *           FtpPutFileW (WININET.@)
 *
 * Uploads a file to the FTP server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI FtpPutFileW(HINTERNET hConnect, LPCWSTR lpszLocalFile,
    LPCWSTR lpszNewRemoteFile, DWORD dwFlags, DWORD_PTR dwContext)
{
    ftp_session_t *lpwfs;
    appinfo_t *hIC = NULL;
    BOOL r = FALSE;

    if (!lpszLocalFile || !lpszNewRemoteFile)
    {
        INTERNET_SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    lpwfs = (ftp_session_t*) get_handle_object( hConnect );
    if (!lpwfs)
    {
        INTERNET_SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    if (WH_HFTPSESSION != lpwfs->hdr.htype)
    {
        INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
        goto lend;
    }

    if (lpwfs->download_in_progress != NULL)
    {
        INTERNET_SetLastError(ERROR_FTP_TRANSFER_IN_PROGRESS);
        goto lend;
    }

    if ((dwFlags & FTP_CONDITION_MASK) > FTP_TRANSFER_TYPE_BINARY)
    {
        INTERNET_SetLastError(ERROR_INVALID_PARAMETER);
        goto lend;
    }

    hIC = lpwfs->lpAppInfo;
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC)
    {
        WORKREQUEST workRequest;
        struct WORKREQ_FTPPUTFILEW *req = &workRequest.u.FtpPutFileW;

        workRequest.asyncproc = AsyncFtpPutFileProc;
        workRequest.hdr = WININET_AddRef( &lpwfs->hdr );
        req->lpszLocalFile = heap_strdupW(lpszLocalFile);
        req->lpszNewRemoteFile = heap_strdupW(lpszNewRemoteFile);
	req->dwFlags = dwFlags;
	req->dwContext = dwContext;

	r = res_to_le(INTERNET_AsyncCall(&workRequest));
    }
    else
    {
        r = FTP_FtpPutFileW(lpwfs, lpszLocalFile,
	    lpszNewRemoteFile, dwFlags, dwContext);
    }

lend:
    WININET_Release( &lpwfs->hdr );

    return r;
}

/***********************************************************************
 *           FTP_FtpPutFileW (Internal)
 *
 * Uploads a file to the FTP server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
static BOOL FTP_FtpPutFileW(ftp_session_t *lpwfs, LPCWSTR lpszLocalFile,
    LPCWSTR lpszNewRemoteFile, DWORD dwFlags, DWORD_PTR dwContext)
{
    HANDLE hFile;
    BOOL bSuccess = FALSE;
    appinfo_t *hIC = NULL;
    INT nResCode;

    TRACE(" lpszLocalFile(%s) lpszNewRemoteFile(%s)\n", debugstr_w(lpszLocalFile), debugstr_w(lpszNewRemoteFile));

    /* Clear any error information */
    INTERNET_SetLastError(0);

    /* Open file to be uploaded */
    if (INVALID_HANDLE_VALUE ==
        (hFile = CreateFileW(lpszLocalFile, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0)))
        /* Let CreateFile set the appropriate error */
        return FALSE;

    hIC = lpwfs->lpAppInfo;

    SendAsyncCallback(&lpwfs->hdr, lpwfs->hdr.dwContext, INTERNET_STATUS_SENDING_REQUEST, NULL, 0);

    if (FTP_SendStore(lpwfs, lpszNewRemoteFile, dwFlags))
    {
        INT nDataSocket;

        /* Get data socket to server */
        if (FTP_GetDataSocket(lpwfs, &nDataSocket))
        {
            FTP_SendData(lpwfs, nDataSocket, hFile);
            closesocket(nDataSocket);
	    nResCode = FTP_ReceiveResponse(lpwfs, dwContext);
	    if (nResCode)
	    {
	        if (nResCode == 226)
		    bSuccess = TRUE;
		else
		    FTP_SetResponseError(nResCode);
	    }
        }
    }

    if (lpwfs->lstnSocket != -1)
    {
        closesocket(lpwfs->lstnSocket);
        lpwfs->lstnSocket = -1;
    }

    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC)
    {
        INTERNET_ASYNC_RESULT iar;

        iar.dwResult = (DWORD)bSuccess;
        iar.dwError = bSuccess ? ERROR_SUCCESS : INTERNET_GetLastError();
        SendAsyncCallback(&lpwfs->hdr, lpwfs->hdr.dwContext, INTERNET_STATUS_REQUEST_COMPLETE,
            &iar, sizeof(INTERNET_ASYNC_RESULT));
    }

    CloseHandle(hFile);

    return bSuccess;
}


/***********************************************************************
 *           FtpSetCurrentDirectoryA (WININET.@)
 *
 * Change the working directory on the FTP server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI FtpSetCurrentDirectoryA(HINTERNET hConnect, LPCSTR lpszDirectory)
{
    LPWSTR lpwzDirectory;
    BOOL ret;
    
    lpwzDirectory = heap_strdupAtoW(lpszDirectory);
    ret = FtpSetCurrentDirectoryW(hConnect, lpwzDirectory);
    HeapFree(GetProcessHeap(), 0, lpwzDirectory);
    return ret;
}


static void AsyncFtpSetCurrentDirectoryProc(WORKREQUEST *workRequest)
{
    struct WORKREQ_FTPSETCURRENTDIRECTORYW const *req = &workRequest->u.FtpSetCurrentDirectoryW;
    ftp_session_t *lpwfs = (ftp_session_t*) workRequest->hdr;

    TRACE("%p\n", lpwfs);

    FTP_FtpSetCurrentDirectoryW(lpwfs, req->lpszDirectory);
    HeapFree(GetProcessHeap(), 0, req->lpszDirectory);
}

/***********************************************************************
 *           FtpSetCurrentDirectoryW (WININET.@)
 *
 * Change the working directory on the FTP server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI FtpSetCurrentDirectoryW(HINTERNET hConnect, LPCWSTR lpszDirectory)
{
    ftp_session_t *lpwfs = NULL;
    appinfo_t *hIC = NULL;
    BOOL r = FALSE;

    if (!lpszDirectory)
    {
        INTERNET_SetLastError(ERROR_INVALID_PARAMETER);
        goto lend;
    }

    lpwfs = (ftp_session_t*) get_handle_object( hConnect );
    if (NULL == lpwfs || WH_HFTPSESSION != lpwfs->hdr.htype)
    {
        INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
        goto lend;
    }

    if (lpwfs->download_in_progress != NULL)
    {
        INTERNET_SetLastError(ERROR_FTP_TRANSFER_IN_PROGRESS);
        goto lend;
    }

    TRACE("lpszDirectory(%s)\n", debugstr_w(lpszDirectory));

    hIC = lpwfs->lpAppInfo;
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC)
    {
        WORKREQUEST workRequest;
        struct WORKREQ_FTPSETCURRENTDIRECTORYW *req;

        workRequest.asyncproc = AsyncFtpSetCurrentDirectoryProc;
        workRequest.hdr = WININET_AddRef( &lpwfs->hdr );
        req = &workRequest.u.FtpSetCurrentDirectoryW;
        req->lpszDirectory = heap_strdupW(lpszDirectory);

	r = res_to_le(INTERNET_AsyncCall(&workRequest));
    }
    else
    {
        r = FTP_FtpSetCurrentDirectoryW(lpwfs, lpszDirectory);
    }

lend:
    if( lpwfs )
        WININET_Release( &lpwfs->hdr );

    return r;
}


/***********************************************************************
 *           FTP_FtpSetCurrentDirectoryW (Internal)
 *
 * Change the working directory on the FTP server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
static BOOL FTP_FtpSetCurrentDirectoryW(ftp_session_t *lpwfs, LPCWSTR lpszDirectory)
{
    INT nResCode;
    appinfo_t *hIC = NULL;
    DWORD bSuccess = FALSE;

    TRACE("lpszDirectory(%s)\n", debugstr_w(lpszDirectory));

    /* Clear any error information */
    INTERNET_SetLastError(0);

    hIC = lpwfs->lpAppInfo;
    if (!FTP_SendCommand(lpwfs->sndSocket, FTP_CMD_CWD, lpszDirectory,
        lpwfs->hdr.lpfnStatusCB, &lpwfs->hdr, lpwfs->hdr.dwContext))
        goto lend;

    nResCode = FTP_ReceiveResponse(lpwfs, lpwfs->hdr.dwContext);

    if (nResCode)
    {
        if (nResCode == 250)
            bSuccess = TRUE;
        else
            FTP_SetResponseError(nResCode);
    }

lend:
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC)
    {
        INTERNET_ASYNC_RESULT iar;

        iar.dwResult = bSuccess;
        iar.dwError = bSuccess ? ERROR_SUCCESS : ERROR_INTERNET_EXTENDED_ERROR;
        SendAsyncCallback(&lpwfs->hdr, lpwfs->hdr.dwContext, INTERNET_STATUS_REQUEST_COMPLETE,
            &iar, sizeof(INTERNET_ASYNC_RESULT));
    }
    return bSuccess;
}


/***********************************************************************
 *           FtpCreateDirectoryA (WININET.@)
 *
 * Create new directory on the FTP server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI FtpCreateDirectoryA(HINTERNET hConnect, LPCSTR lpszDirectory)
{
    LPWSTR lpwzDirectory;
    BOOL ret;
    
    lpwzDirectory = heap_strdupAtoW(lpszDirectory);
    ret = FtpCreateDirectoryW(hConnect, lpwzDirectory);
    HeapFree(GetProcessHeap(), 0, lpwzDirectory);
    return ret;
}


static void AsyncFtpCreateDirectoryProc(WORKREQUEST *workRequest)
{
    struct WORKREQ_FTPCREATEDIRECTORYW const *req = &workRequest->u.FtpCreateDirectoryW;
    ftp_session_t *lpwfs = (ftp_session_t*) workRequest->hdr;

    TRACE(" %p\n", lpwfs);

    FTP_FtpCreateDirectoryW(lpwfs, req->lpszDirectory);
    HeapFree(GetProcessHeap(), 0, req->lpszDirectory);
}

/***********************************************************************
 *           FtpCreateDirectoryW (WININET.@)
 *
 * Create new directory on the FTP server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI FtpCreateDirectoryW(HINTERNET hConnect, LPCWSTR lpszDirectory)
{
    ftp_session_t *lpwfs;
    appinfo_t *hIC = NULL;
    BOOL r = FALSE;

    lpwfs = (ftp_session_t*) get_handle_object( hConnect );
    if (!lpwfs)
    {
        INTERNET_SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    if (WH_HFTPSESSION != lpwfs->hdr.htype)
    {
        INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
        goto lend;
    }

    if (lpwfs->download_in_progress != NULL)
    {
        INTERNET_SetLastError(ERROR_FTP_TRANSFER_IN_PROGRESS);
        goto lend;
    }

    if (!lpszDirectory)
    {
        INTERNET_SetLastError(ERROR_INVALID_PARAMETER);
        goto lend;
    }

    hIC = lpwfs->lpAppInfo;
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC)
    {
        WORKREQUEST workRequest;
        struct WORKREQ_FTPCREATEDIRECTORYW *req;

        workRequest.asyncproc = AsyncFtpCreateDirectoryProc;
        workRequest.hdr = WININET_AddRef( &lpwfs->hdr );
        req = &workRequest.u.FtpCreateDirectoryW;
        req->lpszDirectory = heap_strdupW(lpszDirectory);

	r = res_to_le(INTERNET_AsyncCall(&workRequest));
    }
    else
    {
        r = FTP_FtpCreateDirectoryW(lpwfs, lpszDirectory);
    }
lend:
    WININET_Release( &lpwfs->hdr );

    return r;
}


/***********************************************************************
 *           FTP_FtpCreateDirectoryW (Internal)
 *
 * Create new directory on the FTP server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
static BOOL FTP_FtpCreateDirectoryW(ftp_session_t *lpwfs, LPCWSTR lpszDirectory)
{
    INT nResCode;
    BOOL bSuccess = FALSE;
    appinfo_t *hIC = NULL;

    TRACE("lpszDirectory(%s)\n", debugstr_w(lpszDirectory));

    /* Clear any error information */
    INTERNET_SetLastError(0);

    if (!FTP_SendCommand(lpwfs->sndSocket, FTP_CMD_MKD, lpszDirectory, 0, 0, 0))
        goto lend;

    nResCode = FTP_ReceiveResponse(lpwfs, lpwfs->hdr.dwContext);
    if (nResCode)
    {
        if (nResCode == 257)
            bSuccess = TRUE;
        else
            FTP_SetResponseError(nResCode);
    }

lend:
    hIC = lpwfs->lpAppInfo;
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC)
    {
        INTERNET_ASYNC_RESULT iar;

        iar.dwResult = (DWORD)bSuccess;
        iar.dwError = bSuccess ? ERROR_SUCCESS : INTERNET_GetLastError();
        SendAsyncCallback(&lpwfs->hdr, lpwfs->hdr.dwContext, INTERNET_STATUS_REQUEST_COMPLETE,
            &iar, sizeof(INTERNET_ASYNC_RESULT));
    }

    return bSuccess;
}

/***********************************************************************
 *           FtpFindFirstFileA (WININET.@)
 *
 * Search the specified directory
 *
 * RETURNS
 *    HINTERNET on success
 *    NULL on failure
 *
 */
HINTERNET WINAPI FtpFindFirstFileA(HINTERNET hConnect,
    LPCSTR lpszSearchFile, LPWIN32_FIND_DATAA lpFindFileData, DWORD dwFlags, DWORD_PTR dwContext)
{
    LPWSTR lpwzSearchFile;
    WIN32_FIND_DATAW wfd;
    LPWIN32_FIND_DATAW lpFindFileDataW;
    HINTERNET ret;
    
    lpwzSearchFile = heap_strdupAtoW(lpszSearchFile);
    lpFindFileDataW = lpFindFileData?&wfd:NULL;
    ret = FtpFindFirstFileW(hConnect, lpwzSearchFile, lpFindFileDataW, dwFlags, dwContext);
    HeapFree(GetProcessHeap(), 0, lpwzSearchFile);
    
    if (ret && lpFindFileData)
        WININET_find_data_WtoA(lpFindFileDataW, lpFindFileData);

    return ret;
}


static void AsyncFtpFindFirstFileProc(WORKREQUEST *workRequest)
{
    struct WORKREQ_FTPFINDFIRSTFILEW const *req = &workRequest->u.FtpFindFirstFileW;
    ftp_session_t *lpwfs = (ftp_session_t*) workRequest->hdr;

    TRACE("%p\n", lpwfs);

    FTP_FtpFindFirstFileW(lpwfs, req->lpszSearchFile,
       req->lpFindFileData, req->dwFlags, req->dwContext);
    HeapFree(GetProcessHeap(), 0, req->lpszSearchFile);
}

/***********************************************************************
 *           FtpFindFirstFileW (WININET.@)
 *
 * Search the specified directory
 *
 * RETURNS
 *    HINTERNET on success
 *    NULL on failure
 *
 */
HINTERNET WINAPI FtpFindFirstFileW(HINTERNET hConnect,
    LPCWSTR lpszSearchFile, LPWIN32_FIND_DATAW lpFindFileData, DWORD dwFlags, DWORD_PTR dwContext)
{
    ftp_session_t *lpwfs;
    appinfo_t *hIC = NULL;
    HINTERNET r = NULL;

    lpwfs = (ftp_session_t*) get_handle_object( hConnect );
    if (NULL == lpwfs || WH_HFTPSESSION != lpwfs->hdr.htype)
    {
        INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
        goto lend;
    }

    if (lpwfs->download_in_progress != NULL)
    {
        INTERNET_SetLastError(ERROR_FTP_TRANSFER_IN_PROGRESS);
        goto lend;
    }

    hIC = lpwfs->lpAppInfo;
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC)
    {
        WORKREQUEST workRequest;
        struct WORKREQ_FTPFINDFIRSTFILEW *req;

        workRequest.asyncproc = AsyncFtpFindFirstFileProc;
        workRequest.hdr = WININET_AddRef( &lpwfs->hdr );
        req = &workRequest.u.FtpFindFirstFileW;
        req->lpszSearchFile = (lpszSearchFile == NULL) ? NULL : heap_strdupW(lpszSearchFile);
	req->lpFindFileData = lpFindFileData;
	req->dwFlags = dwFlags;
	req->dwContext= dwContext;

	INTERNET_AsyncCall(&workRequest);
	r = NULL;
    }
    else
    {
        r = FTP_FtpFindFirstFileW(lpwfs, lpszSearchFile, lpFindFileData,
            dwFlags, dwContext);
    }
lend:
    if( lpwfs )
        WININET_Release( &lpwfs->hdr );

    return r;
}


/***********************************************************************
 *           FTP_FtpFindFirstFileW (Internal)
 *
 * Search the specified directory
 *
 * RETURNS
 *    HINTERNET on success
 *    NULL on failure
 *
 */
static HINTERNET FTP_FtpFindFirstFileW(ftp_session_t *lpwfs,
    LPCWSTR lpszSearchFile, LPWIN32_FIND_DATAW lpFindFileData, DWORD dwFlags, DWORD_PTR dwContext)
{
    INT nResCode;
    appinfo_t *hIC = NULL;
    HINTERNET hFindNext = NULL;

    TRACE("\n");

    /* Clear any error information */
    INTERNET_SetLastError(0);

    if (!FTP_InitListenSocket(lpwfs))
        goto lend;

    if (!FTP_SendType(lpwfs, INTERNET_FLAG_TRANSFER_ASCII))
        goto lend;

    if (!FTP_SendPortOrPasv(lpwfs))
        goto lend;

    if (!FTP_SendCommand(lpwfs->sndSocket, FTP_CMD_LIST, NULL,
        lpwfs->hdr.lpfnStatusCB, &lpwfs->hdr, lpwfs->hdr.dwContext))
        goto lend;

    nResCode = FTP_ReceiveResponse(lpwfs, lpwfs->hdr.dwContext);
    if (nResCode)
    {
        if (nResCode == 125 || nResCode == 150)
        {
            INT nDataSocket;

            /* Get data socket to server */
            if (FTP_GetDataSocket(lpwfs, &nDataSocket))
            {
                hFindNext = FTP_ReceiveFileList(lpwfs, nDataSocket, lpszSearchFile, lpFindFileData, dwContext);
                closesocket(nDataSocket);
                nResCode = FTP_ReceiveResponse(lpwfs, lpwfs->hdr.dwContext);
                if (nResCode != 226 && nResCode != 250)
                    INTERNET_SetLastError(ERROR_NO_MORE_FILES);
            }
        }
        else
            FTP_SetResponseError(nResCode);
    }

lend:
    if (lpwfs->lstnSocket != -1)
    {
        closesocket(lpwfs->lstnSocket);
        lpwfs->lstnSocket = -1;
    }

    hIC = lpwfs->lpAppInfo;
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC)
    {
        INTERNET_ASYNC_RESULT iar;

        if (hFindNext)
	{
            iar.dwResult = (DWORD_PTR)hFindNext;
            iar.dwError = ERROR_SUCCESS;
            SendAsyncCallback(&lpwfs->hdr, lpwfs->hdr.dwContext, INTERNET_STATUS_HANDLE_CREATED,
                &iar, sizeof(INTERNET_ASYNC_RESULT));
	}

        iar.dwResult = (DWORD_PTR)hFindNext;
        iar.dwError = hFindNext ? ERROR_SUCCESS : INTERNET_GetLastError();
        SendAsyncCallback(&lpwfs->hdr, lpwfs->hdr.dwContext, INTERNET_STATUS_REQUEST_COMPLETE,
            &iar, sizeof(INTERNET_ASYNC_RESULT));
    }

    return hFindNext;
}


/***********************************************************************
 *           FtpGetCurrentDirectoryA (WININET.@)
 *
 * Retrieves the current directory
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI FtpGetCurrentDirectoryA(HINTERNET hFtpSession, LPSTR lpszCurrentDirectory,
    LPDWORD lpdwCurrentDirectory)
{
    WCHAR *dir = NULL;
    DWORD len;
    BOOL ret;

    if(lpdwCurrentDirectory) {
        len = *lpdwCurrentDirectory;
        if(lpszCurrentDirectory)
        {
            dir = heap_alloc(len * sizeof(WCHAR));
            if (NULL == dir)
            {
                INTERNET_SetLastError(ERROR_OUTOFMEMORY);
                return FALSE;
            }
        }
    }
    ret = FtpGetCurrentDirectoryW(hFtpSession, lpszCurrentDirectory?dir:NULL, lpdwCurrentDirectory?&len:NULL);

    if (ret && lpszCurrentDirectory)
        WideCharToMultiByte(CP_ACP, 0, dir, -1, lpszCurrentDirectory, len, NULL, NULL);

    if (lpdwCurrentDirectory) *lpdwCurrentDirectory = len;
    HeapFree(GetProcessHeap(), 0, dir);
    return ret;
}


static void AsyncFtpGetCurrentDirectoryProc(WORKREQUEST *workRequest)
{
    struct WORKREQ_FTPGETCURRENTDIRECTORYW const *req = &workRequest->u.FtpGetCurrentDirectoryW;
    ftp_session_t *lpwfs = (ftp_session_t*) workRequest->hdr;

    TRACE("%p\n", lpwfs);

    FTP_FtpGetCurrentDirectoryW(lpwfs, req->lpszDirectory, req->lpdwDirectory);
}

/***********************************************************************
 *           FtpGetCurrentDirectoryW (WININET.@)
 *
 * Retrieves the current directory
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI FtpGetCurrentDirectoryW(HINTERNET hFtpSession, LPWSTR lpszCurrentDirectory,
    LPDWORD lpdwCurrentDirectory)
{
    ftp_session_t *lpwfs;
    appinfo_t *hIC = NULL;
    BOOL r = FALSE;

    TRACE("%p %p %p\n", hFtpSession, lpszCurrentDirectory, lpdwCurrentDirectory);

    lpwfs = (ftp_session_t*) get_handle_object( hFtpSession );
    if (NULL == lpwfs)
    {
        INTERNET_SetLastError(ERROR_INVALID_HANDLE);
        goto lend;
    }

    if (WH_HFTPSESSION != lpwfs->hdr.htype)
    {
        INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
        goto lend;
    }

    if (!lpdwCurrentDirectory)
    {
        INTERNET_SetLastError(ERROR_INVALID_PARAMETER);
        goto lend;
    }

    if (lpszCurrentDirectory == NULL)
    {
        INTERNET_SetLastError(ERROR_INSUFFICIENT_BUFFER);
        goto lend;
    }

    if (lpwfs->download_in_progress != NULL)
    {
        INTERNET_SetLastError(ERROR_FTP_TRANSFER_IN_PROGRESS);
        goto lend;
    }

    hIC = lpwfs->lpAppInfo;
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC)
    {
        WORKREQUEST workRequest;
        struct WORKREQ_FTPGETCURRENTDIRECTORYW *req;

        workRequest.asyncproc = AsyncFtpGetCurrentDirectoryProc;
        workRequest.hdr = WININET_AddRef( &lpwfs->hdr );
        req = &workRequest.u.FtpGetCurrentDirectoryW;
	req->lpszDirectory = lpszCurrentDirectory;
	req->lpdwDirectory = lpdwCurrentDirectory;

	r = res_to_le(INTERNET_AsyncCall(&workRequest));
    }
    else
    {
        r = FTP_FtpGetCurrentDirectoryW(lpwfs, lpszCurrentDirectory,
            lpdwCurrentDirectory);
    }

lend:
    if( lpwfs )
        WININET_Release( &lpwfs->hdr );

    return r;
}


/***********************************************************************
 *           FTP_FtpGetCurrentDirectoryW (Internal)
 *
 * Retrieves the current directory
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
static BOOL FTP_FtpGetCurrentDirectoryW(ftp_session_t *lpwfs, LPWSTR lpszCurrentDirectory,
	LPDWORD lpdwCurrentDirectory)
{
    INT nResCode;
    appinfo_t *hIC = NULL;
    DWORD bSuccess = FALSE;

    /* Clear any error information */
    INTERNET_SetLastError(0);

    hIC = lpwfs->lpAppInfo;
    if (!FTP_SendCommand(lpwfs->sndSocket, FTP_CMD_PWD, NULL,
        lpwfs->hdr.lpfnStatusCB, &lpwfs->hdr, lpwfs->hdr.dwContext))
        goto lend;

    nResCode = FTP_ReceiveResponse(lpwfs, lpwfs->hdr.dwContext);
    if (nResCode)
    {
        if (nResCode == 257) /* Extract directory name */
        {
            DWORD firstpos, lastpos, len;
            LPWSTR lpszResponseBuffer = heap_strdupAtoW(INTERNET_GetResponseBuffer());

            for (firstpos = 0, lastpos = 0; lpszResponseBuffer[lastpos]; lastpos++)
            {
                if ('"' == lpszResponseBuffer[lastpos])
                {
                    if (!firstpos)
                        firstpos = lastpos;
                    else
                        break;
                }
            }
            len = lastpos - firstpos;
            if (*lpdwCurrentDirectory >= len)
            {
                memcpy(lpszCurrentDirectory, &lpszResponseBuffer[firstpos + 1], len * sizeof(WCHAR));
                lpszCurrentDirectory[len - 1] = 0;
                *lpdwCurrentDirectory = len;
                bSuccess = TRUE;
            }
            else INTERNET_SetLastError(ERROR_INSUFFICIENT_BUFFER);

            HeapFree(GetProcessHeap(), 0, lpszResponseBuffer);
        }
        else
            FTP_SetResponseError(nResCode);
    }

lend:
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC)
    {
        INTERNET_ASYNC_RESULT iar;

        iar.dwResult = bSuccess;
        iar.dwError = bSuccess ? ERROR_SUCCESS : ERROR_INTERNET_EXTENDED_ERROR;
        SendAsyncCallback(&lpwfs->hdr, lpwfs->hdr.dwContext, INTERNET_STATUS_REQUEST_COMPLETE,
            &iar, sizeof(INTERNET_ASYNC_RESULT));
    }

    return bSuccess;
}


/***********************************************************************
 *           FTPFILE_Destroy(internal)
 *
 * Closes the file transfer handle. This also 'cleans' the data queue of
 * the 'transfer complete' message (this is a bit of a hack though :-/ )
 *
 */
static void FTPFILE_Destroy(object_header_t *hdr)
{
    ftp_file_t *lpwh = (ftp_file_t*) hdr;
    ftp_session_t *lpwfs = lpwh->lpFtpSession;
    INT nResCode;

    TRACE("\n");

    if (lpwh->cache_file_handle != INVALID_HANDLE_VALUE)
        CloseHandle(lpwh->cache_file_handle);

    HeapFree(GetProcessHeap(), 0, lpwh->cache_file);

    if (!lpwh->session_deleted)
        lpwfs->download_in_progress = NULL;

    if (lpwh->nDataSocket != -1)
        closesocket(lpwh->nDataSocket);

    nResCode = FTP_ReceiveResponse(lpwfs, lpwfs->hdr.dwContext);
    if (nResCode > 0 && nResCode != 226) WARN("server reports failed transfer\n");

    WININET_Release(&lpwh->lpFtpSession->hdr);
}

static DWORD FTPFILE_QueryOption(object_header_t *hdr, DWORD option, void *buffer, DWORD *size, BOOL unicode)
{
    switch(option) {
    case INTERNET_OPTION_HANDLE_TYPE:
        TRACE("INTERNET_OPTION_HANDLE_TYPE\n");

        if (*size < sizeof(ULONG))
            return ERROR_INSUFFICIENT_BUFFER;

        *size = sizeof(DWORD);
        *(DWORD*)buffer = INTERNET_HANDLE_TYPE_FTP_FILE;
        return ERROR_SUCCESS;
    case INTERNET_OPTION_DATAFILE_NAME:
    {
        DWORD required;
        ftp_file_t *file = (ftp_file_t *)hdr;

        TRACE("INTERNET_OPTION_DATAFILE_NAME\n");

        if (!file->cache_file)
        {
            *size = 0;
            return ERROR_INTERNET_ITEM_NOT_FOUND;
        }
        if (unicode)
        {
            required = (lstrlenW(file->cache_file) + 1) * sizeof(WCHAR);
            if (*size < required)
                return ERROR_INSUFFICIENT_BUFFER;

            *size = required;
            memcpy(buffer, file->cache_file, *size);
            return ERROR_SUCCESS;
        }
        else
        {
            required = WideCharToMultiByte(CP_ACP, 0, file->cache_file, -1, NULL, 0, NULL, NULL);
            if (required > *size)
                return ERROR_INSUFFICIENT_BUFFER;

            *size = WideCharToMultiByte(CP_ACP, 0, file->cache_file, -1, buffer, *size, NULL, NULL);
            return ERROR_SUCCESS;
        }
    }
    }
    return INET_QueryOption(hdr, option, buffer, size, unicode);
}

static DWORD FTPFILE_ReadFile(object_header_t *hdr, void *buffer, DWORD size, DWORD *read)
{
    ftp_file_t *file = (ftp_file_t*)hdr;
    int res;
    DWORD error;

    if (file->nDataSocket == -1)
        return ERROR_INTERNET_DISCONNECTED;

    /* FIXME: FTP should use NETCON_ stuff */
    res = recv(file->nDataSocket, buffer, size, MSG_WAITALL);
    *read = res>0 ? res : 0;

    error = res >= 0 ? ERROR_SUCCESS : INTERNET_ERROR_BASE; /* FIXME */
    if (error == ERROR_SUCCESS && file->cache_file)
    {
        DWORD bytes_written;

        if (!WriteFile(file->cache_file_handle, buffer, *read, &bytes_written, NULL))
            WARN("WriteFile failed: %u\n", GetLastError());
    }
    return error;
}

static DWORD FTPFILE_ReadFileExA(object_header_t *hdr, INTERNET_BUFFERSA *buffers,
    DWORD flags, DWORD_PTR context)
{
    return FTPFILE_ReadFile(hdr, buffers->lpvBuffer, buffers->dwBufferLength, &buffers->dwBufferLength);
}

static DWORD FTPFILE_ReadFileExW(object_header_t *hdr, INTERNET_BUFFERSW *buffers,
    DWORD flags, DWORD_PTR context)
{
    return FTPFILE_ReadFile(hdr, buffers->lpvBuffer, buffers->dwBufferLength, &buffers->dwBufferLength);
}

static DWORD FTPFILE_WriteFile(object_header_t *hdr, const void *buffer, DWORD size, DWORD *written)
{
    ftp_file_t *lpwh = (ftp_file_t*) hdr;
    int res;

    res = send(lpwh->nDataSocket, buffer, size, 0);

    *written = res>0 ? res : 0;
    return res >= 0 ? ERROR_SUCCESS : sock_get_error(errno);
}

static void FTP_ReceiveRequestData(ftp_file_t *file, BOOL first_notif)
{
    INTERNET_ASYNC_RESULT iar;
    BYTE buffer[4096];
    int available;

    TRACE("%p\n", file);

    available = recv(file->nDataSocket, buffer, sizeof(buffer), MSG_PEEK);

    if(available != -1) {
        iar.dwResult = (DWORD_PTR)file->hdr.hInternet;
        iar.dwError = first_notif ? 0 : available;
    }else {
        iar.dwResult = 0;
        iar.dwError = INTERNET_GetLastError();
    }

    INTERNET_SendCallback(&file->hdr, file->hdr.dwContext, INTERNET_STATUS_REQUEST_COMPLETE, &iar,
                          sizeof(INTERNET_ASYNC_RESULT));
}

static void FTPFILE_AsyncQueryDataAvailableProc(WORKREQUEST *workRequest)
{
    ftp_file_t *file = (ftp_file_t*)workRequest->hdr;

    FTP_ReceiveRequestData(file, FALSE);
}

static DWORD FTPFILE_QueryDataAvailable(object_header_t *hdr, DWORD *available, DWORD flags, DWORD_PTR ctx)
{
    ftp_file_t *file = (ftp_file_t*) hdr;
    int retval, unread = 0;

    TRACE("(%p %p %x %lx)\n", file, available, flags, ctx);

#ifdef FIONREAD
    retval = ioctlsocket(file->nDataSocket, FIONREAD, &unread);
    if (!retval)
        TRACE("%d bytes of queued, but unread data\n", unread);
#else
    FIXME("FIONREAD not available\n");
#endif

    *available = unread;

    if(!unread) {
        BYTE byte;

        *available = 0;

        retval = recv(file->nDataSocket, &byte, 1, MSG_PEEK);
        if(retval > 0) {
            WORKREQUEST workRequest;

            *available = 0;
            workRequest.asyncproc = FTPFILE_AsyncQueryDataAvailableProc;
            workRequest.hdr = WININET_AddRef( &file->hdr );

            INTERNET_AsyncCall(&workRequest);

            return ERROR_IO_PENDING;
        }
    }

    return ERROR_SUCCESS;
}


static const object_vtbl_t FTPFILEVtbl = {
    FTPFILE_Destroy,
    NULL,
    FTPFILE_QueryOption,
    NULL,
    FTPFILE_ReadFile,
    FTPFILE_ReadFileExA,
    FTPFILE_ReadFileExW,
    FTPFILE_WriteFile,
    FTPFILE_QueryDataAvailable,
    NULL
};

/***********************************************************************
 *           FTP_FtpOpenFileW (Internal)
 *
 * Open a remote file for writing or reading
 *
 * RETURNS
 *    HINTERNET handle on success
 *    NULL on failure
 *
 */
static HINTERNET FTP_FtpOpenFileW(ftp_session_t *lpwfs,
	LPCWSTR lpszFileName, DWORD fdwAccess, DWORD dwFlags,
	DWORD_PTR dwContext)
{
    INT nDataSocket;
    BOOL bSuccess = FALSE;
    ftp_file_t *lpwh = NULL;
    appinfo_t *hIC = NULL;

    TRACE("\n");

    /* Clear any error information */
    INTERNET_SetLastError(0);

    if (GENERIC_READ == fdwAccess)
    {
        /* Set up socket to retrieve data */
        bSuccess = FTP_SendRetrieve(lpwfs, lpszFileName, dwFlags);
    }
    else if (GENERIC_WRITE == fdwAccess)
    {
        /* Set up socket to send data */
        bSuccess = FTP_SendStore(lpwfs, lpszFileName, dwFlags);
    }

    /* Get data socket to server */
    if (bSuccess && FTP_GetDataSocket(lpwfs, &nDataSocket))
    {
        lpwh = alloc_object(&lpwfs->hdr, &FTPFILEVtbl, sizeof(ftp_file_t));
        lpwh->hdr.htype = WH_HFILE;
        lpwh->hdr.dwFlags = dwFlags;
        lpwh->hdr.dwContext = dwContext;
        lpwh->nDataSocket = nDataSocket;
        lpwh->cache_file = NULL;
        lpwh->cache_file_handle = INVALID_HANDLE_VALUE;
        lpwh->session_deleted = FALSE;

        WININET_AddRef( &lpwfs->hdr );
        lpwh->lpFtpSession = lpwfs;
        list_add_head( &lpwfs->hdr.children, &lpwh->hdr.entry );
	
	/* Indicate that a download is currently in progress */
	lpwfs->download_in_progress = lpwh;
    }

    if (lpwfs->lstnSocket != -1)
    {
        closesocket(lpwfs->lstnSocket);
        lpwfs->lstnSocket = -1;
    }

    if (bSuccess && fdwAccess == GENERIC_READ)
    {
        WCHAR filename[MAX_PATH + 1];
        URL_COMPONENTSW uc;
        DWORD len;

        memset(&uc, 0, sizeof(uc));
        uc.dwStructSize = sizeof(uc);
        uc.nScheme      = INTERNET_SCHEME_FTP;
        uc.lpszHostName = lpwfs->servername;
        uc.nPort        = lpwfs->serverport;
        uc.lpszUserName = lpwfs->lpszUserName;
        uc.lpszUrlPath  = heap_strdupW(lpszFileName);

        if (!InternetCreateUrlW(&uc, 0, NULL, &len) && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            WCHAR *url = heap_alloc(len * sizeof(WCHAR));

            if (url && InternetCreateUrlW(&uc, 0, url, &len) && CreateUrlCacheEntryW(url, 0, NULL, filename, 0))
            {
                lpwh->cache_file = heap_strdupW(filename);
                lpwh->cache_file_handle = CreateFileW(filename, GENERIC_WRITE, FILE_SHARE_READ,
                                                      NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                if (lpwh->cache_file_handle == INVALID_HANDLE_VALUE)
                {
                    WARN("Could not create cache file: %u\n", GetLastError());
                    HeapFree(GetProcessHeap(), 0, lpwh->cache_file);
                    lpwh->cache_file = NULL;
                }
            }
            HeapFree(GetProcessHeap(), 0, url);
        }
        HeapFree(GetProcessHeap(), 0, uc.lpszUrlPath);
    }

    hIC = lpwfs->lpAppInfo;
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC)
    {
        INTERNET_ASYNC_RESULT iar;

	if (lpwh)
	{
            iar.dwResult = (DWORD_PTR)lpwh->hdr.hInternet;
            iar.dwError = ERROR_SUCCESS;
            SendAsyncCallback(&lpwfs->hdr, lpwfs->hdr.dwContext, INTERNET_STATUS_HANDLE_CREATED,
                &iar, sizeof(INTERNET_ASYNC_RESULT));
	}

        if(bSuccess) {
            FTP_ReceiveRequestData(lpwh, TRUE);
        }else {
            iar.dwResult = 0;
            iar.dwError = INTERNET_GetLastError();
            SendAsyncCallback(&lpwfs->hdr, lpwfs->hdr.dwContext, INTERNET_STATUS_REQUEST_COMPLETE,
                    &iar, sizeof(INTERNET_ASYNC_RESULT));
        }
    }

    if(!bSuccess) {
        if(lpwh)
            WININET_Release( &lpwh->hdr );
        return FALSE;
    }

    return lpwh->hdr.hInternet;
}


/***********************************************************************
 *           FtpOpenFileA (WININET.@)
 *
 * Open a remote file for writing or reading
 *
 * RETURNS
 *    HINTERNET handle on success
 *    NULL on failure
 *
 */
HINTERNET WINAPI FtpOpenFileA(HINTERNET hFtpSession,
    LPCSTR lpszFileName, DWORD fdwAccess, DWORD dwFlags,
    DWORD_PTR dwContext)
{
    LPWSTR lpwzFileName;
    HINTERNET ret;

    lpwzFileName = heap_strdupAtoW(lpszFileName);
    ret = FtpOpenFileW(hFtpSession, lpwzFileName, fdwAccess, dwFlags, dwContext);
    HeapFree(GetProcessHeap(), 0, lpwzFileName);
    return ret;
}


static void AsyncFtpOpenFileProc(WORKREQUEST *workRequest)
{
    struct WORKREQ_FTPOPENFILEW const *req = &workRequest->u.FtpOpenFileW;
    ftp_session_t *lpwfs = (ftp_session_t*) workRequest->hdr;

    TRACE("%p\n", lpwfs);

    FTP_FtpOpenFileW(lpwfs, req->lpszFilename,
        req->dwAccess, req->dwFlags, req->dwContext);
    HeapFree(GetProcessHeap(), 0, req->lpszFilename);
}

/***********************************************************************
 *           FtpOpenFileW (WININET.@)
 *
 * Open a remote file for writing or reading
 *
 * RETURNS
 *    HINTERNET handle on success
 *    NULL on failure
 *
 */
HINTERNET WINAPI FtpOpenFileW(HINTERNET hFtpSession,
    LPCWSTR lpszFileName, DWORD fdwAccess, DWORD dwFlags,
    DWORD_PTR dwContext)
{
    ftp_session_t *lpwfs;
    appinfo_t *hIC = NULL;
    HINTERNET r = NULL;

    TRACE("(%p,%s,0x%08x,0x%08x,0x%08lx)\n", hFtpSession,
        debugstr_w(lpszFileName), fdwAccess, dwFlags, dwContext);

    lpwfs = (ftp_session_t*) get_handle_object( hFtpSession );
    if (!lpwfs)
    {
        INTERNET_SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    if (WH_HFTPSESSION != lpwfs->hdr.htype)
    {
        INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
        goto lend;
    }

    if ((!lpszFileName) ||
        ((fdwAccess != GENERIC_READ) && (fdwAccess != GENERIC_WRITE)) ||
        ((dwFlags & FTP_CONDITION_MASK) > FTP_TRANSFER_TYPE_BINARY))
    {
        INTERNET_SetLastError(ERROR_INVALID_PARAMETER);
        goto lend;
    }

    if (lpwfs->download_in_progress != NULL)
    {
        INTERNET_SetLastError(ERROR_FTP_TRANSFER_IN_PROGRESS);
        goto lend;
    }

    hIC = lpwfs->lpAppInfo;
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC)
    {
        WORKREQUEST workRequest;
        struct WORKREQ_FTPOPENFILEW *req;

        workRequest.asyncproc = AsyncFtpOpenFileProc;
        workRequest.hdr = WININET_AddRef( &lpwfs->hdr );
        req = &workRequest.u.FtpOpenFileW;
	req->lpszFilename = heap_strdupW(lpszFileName);
	req->dwAccess = fdwAccess;
	req->dwFlags = dwFlags;
	req->dwContext = dwContext;

	INTERNET_AsyncCall(&workRequest);
	r = NULL;
    }
    else
    {
	r = FTP_FtpOpenFileW(lpwfs, lpszFileName, fdwAccess, dwFlags, dwContext);
    }

lend:
    WININET_Release( &lpwfs->hdr );

    return r;
}


/***********************************************************************
 *           FtpGetFileA (WININET.@)
 *
 * Retrieve file from the FTP server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI FtpGetFileA(HINTERNET hInternet, LPCSTR lpszRemoteFile, LPCSTR lpszNewFile,
    BOOL fFailIfExists, DWORD dwLocalFlagsAttribute, DWORD dwInternetFlags,
    DWORD_PTR dwContext)
{
    LPWSTR lpwzRemoteFile;
    LPWSTR lpwzNewFile;
    BOOL ret;
    
    lpwzRemoteFile = heap_strdupAtoW(lpszRemoteFile);
    lpwzNewFile = heap_strdupAtoW(lpszNewFile);
    ret = FtpGetFileW(hInternet, lpwzRemoteFile, lpwzNewFile, fFailIfExists,
        dwLocalFlagsAttribute, dwInternetFlags, dwContext);
    HeapFree(GetProcessHeap(), 0, lpwzRemoteFile);
    HeapFree(GetProcessHeap(), 0, lpwzNewFile);
    return ret;
}


static void AsyncFtpGetFileProc(WORKREQUEST *workRequest)
{
    struct WORKREQ_FTPGETFILEW const *req = &workRequest->u.FtpGetFileW;
    ftp_session_t *lpwfs = (ftp_session_t*) workRequest->hdr;

    TRACE("%p\n", lpwfs);

    FTP_FtpGetFileW(lpwfs, req->lpszRemoteFile,
             req->lpszNewFile, req->fFailIfExists,
             req->dwLocalFlagsAttribute, req->dwFlags, req->dwContext);
    HeapFree(GetProcessHeap(), 0, req->lpszRemoteFile);
    HeapFree(GetProcessHeap(), 0, req->lpszNewFile);
}


/***********************************************************************
 *           FtpGetFileW (WININET.@)
 *
 * Retrieve file from the FTP server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI FtpGetFileW(HINTERNET hInternet, LPCWSTR lpszRemoteFile, LPCWSTR lpszNewFile,
    BOOL fFailIfExists, DWORD dwLocalFlagsAttribute, DWORD dwInternetFlags,
    DWORD_PTR dwContext)
{
    ftp_session_t *lpwfs;
    appinfo_t *hIC = NULL;
    BOOL r = FALSE;

    if (!lpszRemoteFile || !lpszNewFile)
    {
        INTERNET_SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    lpwfs = (ftp_session_t*) get_handle_object( hInternet );
    if (!lpwfs)
    {
        INTERNET_SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    if (WH_HFTPSESSION != lpwfs->hdr.htype)
    {
        INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
        goto lend;
    }

    if ((dwInternetFlags & FTP_CONDITION_MASK) > FTP_TRANSFER_TYPE_BINARY)
    {
        INTERNET_SetLastError(ERROR_INVALID_PARAMETER);
        goto lend;
    }

    if (lpwfs->download_in_progress != NULL)
    {
        INTERNET_SetLastError(ERROR_FTP_TRANSFER_IN_PROGRESS);
        goto lend;
    }
    
    hIC = lpwfs->lpAppInfo;
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC)
    {
        WORKREQUEST workRequest;
        struct WORKREQ_FTPGETFILEW *req;

        workRequest.asyncproc = AsyncFtpGetFileProc;
        workRequest.hdr = WININET_AddRef( &lpwfs->hdr );
        req = &workRequest.u.FtpGetFileW;
        req->lpszRemoteFile = heap_strdupW(lpszRemoteFile);
        req->lpszNewFile = heap_strdupW(lpszNewFile);
	req->dwLocalFlagsAttribute = dwLocalFlagsAttribute;
	req->fFailIfExists = fFailIfExists;
	req->dwFlags = dwInternetFlags;
	req->dwContext = dwContext;

	r = res_to_le(INTERNET_AsyncCall(&workRequest));
    }
    else
    {
        r = FTP_FtpGetFileW(lpwfs, lpszRemoteFile, lpszNewFile,
           fFailIfExists, dwLocalFlagsAttribute, dwInternetFlags, dwContext);
    }

lend:
    WININET_Release( &lpwfs->hdr );

    return r;
}


/***********************************************************************
 *           FTP_FtpGetFileW (Internal)
 *
 * Retrieve file from the FTP server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
static BOOL FTP_FtpGetFileW(ftp_session_t *lpwfs, LPCWSTR lpszRemoteFile, LPCWSTR lpszNewFile,
	BOOL fFailIfExists, DWORD dwLocalFlagsAttribute, DWORD dwInternetFlags,
	DWORD_PTR dwContext)
{
    BOOL bSuccess = FALSE;
    HANDLE hFile;
    appinfo_t *hIC = NULL;

    TRACE("lpszRemoteFile(%s) lpszNewFile(%s)\n", debugstr_w(lpszRemoteFile), debugstr_w(lpszNewFile));

    /* Clear any error information */
    INTERNET_SetLastError(0);

    /* Ensure we can write to lpszNewfile by opening it */
    hFile = CreateFileW(lpszNewFile, GENERIC_WRITE, 0, 0, fFailIfExists ?
        CREATE_NEW : CREATE_ALWAYS, dwLocalFlagsAttribute, 0);
    if (INVALID_HANDLE_VALUE == hFile)
        return FALSE;

    /* Set up socket to retrieve data */
    if (FTP_SendRetrieve(lpwfs, lpszRemoteFile, dwInternetFlags))
    {
        INT nDataSocket;

        /* Get data socket to server */
        if (FTP_GetDataSocket(lpwfs, &nDataSocket))
        {
            INT nResCode;

            /* Receive data */
            FTP_RetrieveFileData(lpwfs, nDataSocket, hFile);
            closesocket(nDataSocket);

            nResCode = FTP_ReceiveResponse(lpwfs, dwContext);
            if (nResCode)
            {
                if (nResCode == 226)
                    bSuccess = TRUE;
                else
                    FTP_SetResponseError(nResCode);
            }
        }
    }

    if (lpwfs->lstnSocket != -1)
    {
        closesocket(lpwfs->lstnSocket);
        lpwfs->lstnSocket = -1;
    }

    CloseHandle(hFile);

    hIC = lpwfs->lpAppInfo;
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC)
    {
        INTERNET_ASYNC_RESULT iar;

        iar.dwResult = (DWORD)bSuccess;
        iar.dwError = bSuccess ? ERROR_SUCCESS : INTERNET_GetLastError();
        SendAsyncCallback(&lpwfs->hdr, lpwfs->hdr.dwContext, INTERNET_STATUS_REQUEST_COMPLETE,
            &iar, sizeof(INTERNET_ASYNC_RESULT));
    }

    if (!bSuccess) DeleteFileW(lpszNewFile);
    return bSuccess;
}

/***********************************************************************
 *           FtpGetFileSize  (WININET.@)
 */
DWORD WINAPI FtpGetFileSize( HINTERNET hFile, LPDWORD lpdwFileSizeHigh )
{
    FIXME("(%p, %p)\n", hFile, lpdwFileSizeHigh);

    if (lpdwFileSizeHigh)
        *lpdwFileSizeHigh = 0;

    return 0;
}

/***********************************************************************
 *           FtpDeleteFileA  (WININET.@)
 *
 * Delete a file on the ftp server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI FtpDeleteFileA(HINTERNET hFtpSession, LPCSTR lpszFileName)
{
    LPWSTR lpwzFileName;
    BOOL ret;
    
    lpwzFileName = heap_strdupAtoW(lpszFileName);
    ret = FtpDeleteFileW(hFtpSession, lpwzFileName);
    HeapFree(GetProcessHeap(), 0, lpwzFileName);
    return ret;
}

static void AsyncFtpDeleteFileProc(WORKREQUEST *workRequest)
{
    struct WORKREQ_FTPDELETEFILEW const *req = &workRequest->u.FtpDeleteFileW;
    ftp_session_t *lpwfs = (ftp_session_t*) workRequest->hdr;

    TRACE("%p\n", lpwfs);

    FTP_FtpDeleteFileW(lpwfs, req->lpszFilename);
    HeapFree(GetProcessHeap(), 0, req->lpszFilename);
}

/***********************************************************************
 *           FtpDeleteFileW  (WININET.@)
 *
 * Delete a file on the ftp server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI FtpDeleteFileW(HINTERNET hFtpSession, LPCWSTR lpszFileName)
{
    ftp_session_t *lpwfs;
    appinfo_t *hIC = NULL;
    BOOL r = FALSE;

    lpwfs = (ftp_session_t*) get_handle_object( hFtpSession );
    if (!lpwfs)
    {
        INTERNET_SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    if (WH_HFTPSESSION != lpwfs->hdr.htype)
    {
        INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
        goto lend;
    }

    if (lpwfs->download_in_progress != NULL)
    {
        INTERNET_SetLastError(ERROR_FTP_TRANSFER_IN_PROGRESS);
        goto lend;
    }

    if (!lpszFileName)
    {
        INTERNET_SetLastError(ERROR_INVALID_PARAMETER);
        goto lend;
    }

    hIC = lpwfs->lpAppInfo;
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC)
    {
        WORKREQUEST workRequest;
        struct WORKREQ_FTPDELETEFILEW *req;

        workRequest.asyncproc = AsyncFtpDeleteFileProc;
        workRequest.hdr = WININET_AddRef( &lpwfs->hdr );
        req = &workRequest.u.FtpDeleteFileW;
        req->lpszFilename = heap_strdupW(lpszFileName);

	r = res_to_le(INTERNET_AsyncCall(&workRequest));
    }
    else
    {
        r = FTP_FtpDeleteFileW(lpwfs, lpszFileName);
    }

lend:
    WININET_Release( &lpwfs->hdr );

    return r;
}

/***********************************************************************
 *           FTP_FtpDeleteFileW  (Internal)
 *
 * Delete a file on the ftp server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL FTP_FtpDeleteFileW(ftp_session_t *lpwfs, LPCWSTR lpszFileName)
{
    INT nResCode;
    BOOL bSuccess = FALSE;
    appinfo_t *hIC = NULL;

    TRACE("%p\n", lpwfs);

    /* Clear any error information */
    INTERNET_SetLastError(0);

    if (!FTP_SendCommand(lpwfs->sndSocket, FTP_CMD_DELE, lpszFileName, 0, 0, 0))
        goto lend;

    nResCode = FTP_ReceiveResponse(lpwfs, lpwfs->hdr.dwContext);
    if (nResCode)
    {
        if (nResCode == 250)
            bSuccess = TRUE;
        else
            FTP_SetResponseError(nResCode);
    }
lend:
    hIC = lpwfs->lpAppInfo;
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC)
    {
        INTERNET_ASYNC_RESULT iar;

        iar.dwResult = (DWORD)bSuccess;
        iar.dwError = bSuccess ? ERROR_SUCCESS : INTERNET_GetLastError();
        SendAsyncCallback(&lpwfs->hdr, lpwfs->hdr.dwContext, INTERNET_STATUS_REQUEST_COMPLETE,
            &iar, sizeof(INTERNET_ASYNC_RESULT));
    }

    return bSuccess;
}


/***********************************************************************
 *           FtpRemoveDirectoryA  (WININET.@)
 *
 * Remove a directory on the ftp server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI FtpRemoveDirectoryA(HINTERNET hFtpSession, LPCSTR lpszDirectory)
{
    LPWSTR lpwzDirectory;
    BOOL ret;
    
    lpwzDirectory = heap_strdupAtoW(lpszDirectory);
    ret = FtpRemoveDirectoryW(hFtpSession, lpwzDirectory);
    HeapFree(GetProcessHeap(), 0, lpwzDirectory);
    return ret;
}

static void AsyncFtpRemoveDirectoryProc(WORKREQUEST *workRequest)
{
    struct WORKREQ_FTPREMOVEDIRECTORYW const *req = &workRequest->u.FtpRemoveDirectoryW;
    ftp_session_t *lpwfs = (ftp_session_t*) workRequest->hdr;

    TRACE("%p\n", lpwfs);

    FTP_FtpRemoveDirectoryW(lpwfs, req->lpszDirectory);
    HeapFree(GetProcessHeap(), 0, req->lpszDirectory);
}

/***********************************************************************
 *           FtpRemoveDirectoryW  (WININET.@)
 *
 * Remove a directory on the ftp server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI FtpRemoveDirectoryW(HINTERNET hFtpSession, LPCWSTR lpszDirectory)
{
    ftp_session_t *lpwfs;
    appinfo_t *hIC = NULL;
    BOOL r = FALSE;

    lpwfs = (ftp_session_t*) get_handle_object( hFtpSession );
    if (!lpwfs)
    {
        INTERNET_SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    if (WH_HFTPSESSION != lpwfs->hdr.htype)
    {
        INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
        goto lend;
    }

    if (lpwfs->download_in_progress != NULL)
    {
        INTERNET_SetLastError(ERROR_FTP_TRANSFER_IN_PROGRESS);
        goto lend;
    }

    if (!lpszDirectory)
    {
        INTERNET_SetLastError(ERROR_INVALID_PARAMETER);
        goto lend;
    }

    hIC = lpwfs->lpAppInfo;
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC)
    {
        WORKREQUEST workRequest;
        struct WORKREQ_FTPREMOVEDIRECTORYW *req;

        workRequest.asyncproc = AsyncFtpRemoveDirectoryProc;
        workRequest.hdr = WININET_AddRef( &lpwfs->hdr );
        req = &workRequest.u.FtpRemoveDirectoryW;
        req->lpszDirectory = heap_strdupW(lpszDirectory);

	r = res_to_le(INTERNET_AsyncCall(&workRequest));
    }
    else
    {
        r = FTP_FtpRemoveDirectoryW(lpwfs, lpszDirectory);
    }

lend:
    WININET_Release( &lpwfs->hdr );

    return r;
}

/***********************************************************************
 *           FTP_FtpRemoveDirectoryW  (Internal)
 *
 * Remove a directory on the ftp server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL FTP_FtpRemoveDirectoryW(ftp_session_t *lpwfs, LPCWSTR lpszDirectory)
{
    INT nResCode;
    BOOL bSuccess = FALSE;
    appinfo_t *hIC = NULL;

    TRACE("\n");

    /* Clear any error information */
    INTERNET_SetLastError(0);

    if (!FTP_SendCommand(lpwfs->sndSocket, FTP_CMD_RMD, lpszDirectory, 0, 0, 0))
        goto lend;

    nResCode = FTP_ReceiveResponse(lpwfs, lpwfs->hdr.dwContext);
    if (nResCode)
    {
        if (nResCode == 250)
            bSuccess = TRUE;
        else
            FTP_SetResponseError(nResCode);
    }

lend:
    hIC = lpwfs->lpAppInfo;
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC)
    {
        INTERNET_ASYNC_RESULT iar;

        iar.dwResult = (DWORD)bSuccess;
        iar.dwError = bSuccess ? ERROR_SUCCESS : INTERNET_GetLastError();
        SendAsyncCallback(&lpwfs->hdr, lpwfs->hdr.dwContext, INTERNET_STATUS_REQUEST_COMPLETE,
            &iar, sizeof(INTERNET_ASYNC_RESULT));
    }

    return bSuccess;
}


/***********************************************************************
 *           FtpRenameFileA  (WININET.@)
 *
 * Rename a file on the ftp server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI FtpRenameFileA(HINTERNET hFtpSession, LPCSTR lpszSrc, LPCSTR lpszDest)
{
    LPWSTR lpwzSrc;
    LPWSTR lpwzDest;
    BOOL ret;
    
    lpwzSrc = heap_strdupAtoW(lpszSrc);
    lpwzDest = heap_strdupAtoW(lpszDest);
    ret = FtpRenameFileW(hFtpSession, lpwzSrc, lpwzDest);
    HeapFree(GetProcessHeap(), 0, lpwzSrc);
    HeapFree(GetProcessHeap(), 0, lpwzDest);
    return ret;
}

static void AsyncFtpRenameFileProc(WORKREQUEST *workRequest)
{
    struct WORKREQ_FTPRENAMEFILEW const *req = &workRequest->u.FtpRenameFileW;
    ftp_session_t *lpwfs = (ftp_session_t*) workRequest->hdr;

    TRACE("%p\n", lpwfs);

    FTP_FtpRenameFileW(lpwfs, req->lpszSrcFile, req->lpszDestFile);
    HeapFree(GetProcessHeap(), 0, req->lpszSrcFile);
    HeapFree(GetProcessHeap(), 0, req->lpszDestFile);
}

/***********************************************************************
 *           FtpRenameFileW  (WININET.@)
 *
 * Rename a file on the ftp server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI FtpRenameFileW(HINTERNET hFtpSession, LPCWSTR lpszSrc, LPCWSTR lpszDest)
{
    ftp_session_t *lpwfs;
    appinfo_t *hIC = NULL;
    BOOL r = FALSE;

    lpwfs = (ftp_session_t*) get_handle_object( hFtpSession );
    if (!lpwfs)
    {
        INTERNET_SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    if (WH_HFTPSESSION != lpwfs->hdr.htype)
    {
        INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
        goto lend;
    }

    if (lpwfs->download_in_progress != NULL)
    {
        INTERNET_SetLastError(ERROR_FTP_TRANSFER_IN_PROGRESS);
        goto lend;
    }

    if (!lpszSrc || !lpszDest)
    {
        INTERNET_SetLastError(ERROR_INVALID_PARAMETER);
        goto lend;
    }

    hIC = lpwfs->lpAppInfo;
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC)
    {
        WORKREQUEST workRequest;
        struct WORKREQ_FTPRENAMEFILEW *req;

        workRequest.asyncproc = AsyncFtpRenameFileProc;
        workRequest.hdr = WININET_AddRef( &lpwfs->hdr );
        req = &workRequest.u.FtpRenameFileW;
        req->lpszSrcFile = heap_strdupW(lpszSrc);
        req->lpszDestFile = heap_strdupW(lpszDest);

	r = res_to_le(INTERNET_AsyncCall(&workRequest));
    }
    else
    {
        r = FTP_FtpRenameFileW(lpwfs, lpszSrc, lpszDest);
    }

lend:
    WININET_Release( &lpwfs->hdr );

    return r;
}

/***********************************************************************
 *           FTP_FtpRenameFileW  (Internal)
 *
 * Rename a file on the ftp server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL FTP_FtpRenameFileW(ftp_session_t *lpwfs, LPCWSTR lpszSrc, LPCWSTR lpszDest)
{
    INT nResCode;
    BOOL bSuccess = FALSE;
    appinfo_t *hIC = NULL;

    TRACE("\n");

    /* Clear any error information */
    INTERNET_SetLastError(0);

    if (!FTP_SendCommand(lpwfs->sndSocket, FTP_CMD_RNFR, lpszSrc, 0, 0, 0))
        goto lend;

    nResCode = FTP_ReceiveResponse(lpwfs, lpwfs->hdr.dwContext);
    if (nResCode == 350)
    {
        if (!FTP_SendCommand(lpwfs->sndSocket, FTP_CMD_RNTO, lpszDest, 0, 0, 0))
            goto lend;

        nResCode = FTP_ReceiveResponse(lpwfs, lpwfs->hdr.dwContext);
    }

    if (nResCode == 250)
        bSuccess = TRUE;
    else
        FTP_SetResponseError(nResCode);

lend:
    hIC = lpwfs->lpAppInfo;
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC)
    {
        INTERNET_ASYNC_RESULT iar;

        iar.dwResult = (DWORD)bSuccess;
        iar.dwError = bSuccess ? ERROR_SUCCESS : INTERNET_GetLastError();
        SendAsyncCallback(&lpwfs->hdr, lpwfs->hdr.dwContext, INTERNET_STATUS_REQUEST_COMPLETE,
            &iar, sizeof(INTERNET_ASYNC_RESULT));
    }

    return bSuccess;
}

/***********************************************************************
 *           FtpCommandA  (WININET.@)
 */
BOOL WINAPI FtpCommandA( HINTERNET hConnect, BOOL fExpectResponse, DWORD dwFlags,
                         LPCSTR lpszCommand, DWORD_PTR dwContext, HINTERNET* phFtpCommand )
{
    BOOL r;
    WCHAR *cmdW;

    TRACE("%p %d 0x%08x %s 0x%08lx %p\n", hConnect, fExpectResponse, dwFlags,
          debugstr_a(lpszCommand), dwContext, phFtpCommand);

    if (fExpectResponse)
    {
        FIXME("data connection not supported\n");
        return FALSE;
    }

    if (!lpszCommand || !lpszCommand[0])
    {
        INTERNET_SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (!(cmdW = heap_strdupAtoW(lpszCommand)))
    {
        INTERNET_SetLastError(ERROR_OUTOFMEMORY);
        return FALSE;
    }

    r = FtpCommandW(hConnect, fExpectResponse, dwFlags, cmdW, dwContext, phFtpCommand);

    HeapFree(GetProcessHeap(), 0, cmdW);
    return r;
}

/***********************************************************************
 *           FtpCommandW  (WININET.@)
 */
BOOL WINAPI FtpCommandW( HINTERNET hConnect, BOOL fExpectResponse, DWORD dwFlags,
                         LPCWSTR lpszCommand, DWORD_PTR dwContext, HINTERNET* phFtpCommand )
{
    BOOL r = FALSE;
    ftp_session_t *lpwfs;
    LPSTR cmd = NULL;
    DWORD len, nBytesSent= 0;
    INT nResCode, nRC = 0;

    TRACE("%p %d 0x%08x %s 0x%08lx %p\n", hConnect, fExpectResponse, dwFlags,
           debugstr_w(lpszCommand), dwContext, phFtpCommand);

    if (!lpszCommand || !lpszCommand[0])
    {
        INTERNET_SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (fExpectResponse)
    {
        FIXME("data connection not supported\n");
        return FALSE;
    }

    lpwfs = (ftp_session_t*) get_handle_object( hConnect );
    if (!lpwfs)
    {
        INTERNET_SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    if (WH_HFTPSESSION != lpwfs->hdr.htype)
    {
        INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
        goto lend;
    }

    if (lpwfs->download_in_progress != NULL)
    {
        INTERNET_SetLastError(ERROR_FTP_TRANSFER_IN_PROGRESS);
        goto lend;
    }

    len = WideCharToMultiByte(CP_ACP, 0, lpszCommand, -1, NULL, 0, NULL, NULL) + strlen(szCRLF);
    if ((cmd = heap_alloc(len)))
        WideCharToMultiByte(CP_ACP, 0, lpszCommand, -1, cmd, len, NULL, NULL);
    else
    {
        INTERNET_SetLastError(ERROR_OUTOFMEMORY);
        goto lend;
    }

    strcat(cmd, szCRLF);
    len--;

    TRACE("Sending (%s) len(%d)\n", cmd, len);
    while ((nBytesSent < len) && (nRC != -1))
    {
        nRC = send(lpwfs->sndSocket, cmd + nBytesSent, len - nBytesSent, 0);
        if (nRC != -1)
        {
            nBytesSent += nRC;
            TRACE("Sent %d bytes\n", nRC);
        }
    }

    if (nBytesSent)
    {
        nResCode = FTP_ReceiveResponse(lpwfs, lpwfs->hdr.dwContext);
        if (nResCode > 0 && nResCode < 400)
            r = TRUE;
        else
            FTP_SetResponseError(nResCode);
    }

lend:
    WININET_Release( &lpwfs->hdr );
    HeapFree(GetProcessHeap(), 0, cmd);
    return r;
}


/***********************************************************************
 *           FTPSESSION_Destroy (internal)
 *
 * Deallocate session handle
 */
static void FTPSESSION_Destroy(object_header_t *hdr)
{
    ftp_session_t *lpwfs = (ftp_session_t*) hdr;

    TRACE("\n");

    WININET_Release(&lpwfs->lpAppInfo->hdr);

    HeapFree(GetProcessHeap(), 0, lpwfs->lpszPassword);
    HeapFree(GetProcessHeap(), 0, lpwfs->lpszUserName);
    HeapFree(GetProcessHeap(), 0, lpwfs->servername);
}

static void FTPSESSION_CloseConnection(object_header_t *hdr)
{
    ftp_session_t *lpwfs = (ftp_session_t*) hdr;

    TRACE("\n");

    SendAsyncCallback(&lpwfs->hdr, lpwfs->hdr.dwContext,
                      INTERNET_STATUS_CLOSING_CONNECTION, 0, 0);

    if (lpwfs->download_in_progress != NULL)
        lpwfs->download_in_progress->session_deleted = TRUE;

     if (lpwfs->sndSocket != -1)
         closesocket(lpwfs->sndSocket);

     if (lpwfs->lstnSocket != -1)
         closesocket(lpwfs->lstnSocket);

    if (lpwfs->pasvSocket != -1)
        closesocket(lpwfs->pasvSocket);

    SendAsyncCallback(&lpwfs->hdr, lpwfs->hdr.dwContext,
                      INTERNET_STATUS_CONNECTION_CLOSED, 0, 0);
}

static DWORD FTPSESSION_QueryOption(object_header_t *hdr, DWORD option, void *buffer, DWORD *size, BOOL unicode)
{
    switch(option) {
    case INTERNET_OPTION_HANDLE_TYPE:
        TRACE("INTERNET_OPTION_HANDLE_TYPE\n");

        if (*size < sizeof(ULONG))
            return ERROR_INSUFFICIENT_BUFFER;

        *size = sizeof(DWORD);
        *(DWORD*)buffer = INTERNET_HANDLE_TYPE_CONNECT_FTP;
        return ERROR_SUCCESS;
    }

    return INET_QueryOption(hdr, option, buffer, size, unicode);
}

static const object_vtbl_t FTPSESSIONVtbl = {
    FTPSESSION_Destroy,
    FTPSESSION_CloseConnection,
    FTPSESSION_QueryOption,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};


/***********************************************************************
 *           FTP_Connect (internal)
 *
 * Connect to a ftp server
 *
 * RETURNS
 *   HINTERNET a session handle on success
 *   NULL on failure
 *
 * NOTES:
 *
 * Windows uses 'anonymous' as the username, when given a NULL username
 * and a NULL password. The password is first looked up in:
 *
 * HKCU\Software\Microsoft\Windows\CurrentVersion\Internet Settings\EmailName
 *
 * If this entry is not present it uses the current username as the password.
 *
 */

HINTERNET FTP_Connect(appinfo_t *hIC, LPCWSTR lpszServerName,
	INTERNET_PORT nServerPort, LPCWSTR lpszUserName,
	LPCWSTR lpszPassword, DWORD dwFlags, DWORD_PTR dwContext,
	DWORD dwInternalFlags)
{
    static const WCHAR szKey[] = {'S','o','f','t','w','a','r','e','\\',
                                   'M','i','c','r','o','s','o','f','t','\\',
                                   'W','i','n','d','o','w','s','\\',
                                   'C','u','r','r','e','n','t','V','e','r','s','i','o','n','\\',
                                   'I','n','t','e','r','n','e','t',' ','S','e','t','t','i','n','g','s',0};
    static const WCHAR szValue[] = {'E','m','a','i','l','N','a','m','e',0};
    static const WCHAR szDefaultUsername[] = {'a','n','o','n','y','m','o','u','s','\0'};
    static const WCHAR szEmpty[] = {'\0'};
    struct sockaddr_in socketAddr;
    INT nsocket = -1;
    UINT sock_namelen;
    BOOL bSuccess = FALSE;
    ftp_session_t *lpwfs = NULL;
    char szaddr[INET_ADDRSTRLEN];

    TRACE("%p  Server(%s) Port(%d) User(%s) Paswd(%s)\n",
	    hIC, debugstr_w(lpszServerName),
	    nServerPort, debugstr_w(lpszUserName), debugstr_w(lpszPassword));

    assert( hIC->hdr.htype == WH_HINIT );

    if ((!lpszUserName || !*lpszUserName) && lpszPassword && *lpszPassword)
    {
	INTERNET_SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    
    lpwfs = alloc_object(&hIC->hdr, &FTPSESSIONVtbl, sizeof(ftp_session_t));
    if (NULL == lpwfs)
    {
        INTERNET_SetLastError(ERROR_OUTOFMEMORY);
        return NULL;
    }

    if (nServerPort == INTERNET_INVALID_PORT_NUMBER)
        lpwfs->serverport = INTERNET_DEFAULT_FTP_PORT;
    else
        lpwfs->serverport = nServerPort;

    lpwfs->hdr.htype = WH_HFTPSESSION;
    lpwfs->hdr.dwFlags = dwFlags;
    lpwfs->hdr.dwContext = dwContext;
    lpwfs->hdr.dwInternalFlags |= dwInternalFlags;
    lpwfs->download_in_progress = NULL;
    lpwfs->sndSocket = -1;
    lpwfs->lstnSocket = -1;
    lpwfs->pasvSocket = -1;

    WININET_AddRef( &hIC->hdr );
    lpwfs->lpAppInfo = hIC;
    list_add_head( &hIC->hdr.children, &lpwfs->hdr.entry );

    if(hIC->proxy && hIC->accessType == INTERNET_OPEN_TYPE_PROXY) {
        if(strchrW(hIC->proxy, ' '))
            FIXME("Several proxies not implemented.\n");
        if(hIC->proxyBypass)
            FIXME("Proxy bypass is ignored.\n");
    }
    if (!lpszUserName || !strlenW(lpszUserName)) {
        HKEY key;
        WCHAR szPassword[MAX_PATH];
        DWORD len = sizeof(szPassword);

        lpwfs->lpszUserName = heap_strdupW(szDefaultUsername);

        RegOpenKeyW(HKEY_CURRENT_USER, szKey, &key);
        if (RegQueryValueExW(key, szValue, NULL, NULL, (LPBYTE)szPassword, &len)) {
            /* Nothing in the registry, get the username and use that as the password */
            if (!GetUserNameW(szPassword, &len)) {
                /* Should never get here, but use an empty password as failsafe */
                strcpyW(szPassword, szEmpty);
            }
        }
        RegCloseKey(key);

        TRACE("Password used for anonymous ftp : (%s)\n", debugstr_w(szPassword));
        lpwfs->lpszPassword = heap_strdupW(szPassword);
    }
    else {
        lpwfs->lpszUserName = heap_strdupW(lpszUserName);
        lpwfs->lpszPassword = heap_strdupW(lpszPassword ? lpszPassword : szEmpty);
    }
    lpwfs->servername = heap_strdupW(lpszServerName);
    
    /* Don't send a handle created callback if this handle was created with InternetOpenUrl */
    if (!(lpwfs->hdr.dwInternalFlags & INET_OPENURL))
    {
        INTERNET_ASYNC_RESULT iar;

        iar.dwResult = (DWORD_PTR)lpwfs->hdr.hInternet;
        iar.dwError = ERROR_SUCCESS;

        SendAsyncCallback(&hIC->hdr, dwContext,
                      INTERNET_STATUS_HANDLE_CREATED, &iar,
                      sizeof(INTERNET_ASYNC_RESULT));
    }
        
    SendAsyncCallback(&hIC->hdr, dwContext, INTERNET_STATUS_RESOLVING_NAME,
        (LPWSTR) lpszServerName, (strlenW(lpszServerName)+1) * sizeof(WCHAR));

    sock_namelen = sizeof(socketAddr);
    if (!GetAddress(lpszServerName, lpwfs->serverport, (struct sockaddr *)&socketAddr, &sock_namelen))
    {
	INTERNET_SetLastError(ERROR_INTERNET_NAME_NOT_RESOLVED);
        goto lerror;
    }

    if (socketAddr.sin_family != AF_INET)
    {
        WARN("unsupported address family %d\n", socketAddr.sin_family);
        INTERNET_SetLastError(ERROR_INTERNET_CANNOT_CONNECT);
        goto lerror;
    }

    inet_ntop(socketAddr.sin_family, &socketAddr.sin_addr, szaddr, sizeof(szaddr));
    SendAsyncCallback(&hIC->hdr, dwContext, INTERNET_STATUS_NAME_RESOLVED,
                      szaddr, strlen(szaddr)+1);

    nsocket = socket(AF_INET,SOCK_STREAM,0);
    if (nsocket == -1)
    {
	INTERNET_SetLastError(ERROR_INTERNET_CANNOT_CONNECT);
        goto lerror;
    }

    SendAsyncCallback(&hIC->hdr, dwContext, INTERNET_STATUS_CONNECTING_TO_SERVER,
                      szaddr, strlen(szaddr)+1);

    if (connect(nsocket, (struct sockaddr *)&socketAddr, sock_namelen) < 0)
    {
	ERR("Unable to connect (%s)\n", strerror(errno));
	INTERNET_SetLastError(ERROR_INTERNET_CANNOT_CONNECT);
	closesocket(nsocket);
    }
    else
    {
        TRACE("Connected to server\n");
	lpwfs->sndSocket = nsocket;
        SendAsyncCallback(&hIC->hdr, dwContext, INTERNET_STATUS_CONNECTED_TO_SERVER,
                          szaddr, strlen(szaddr)+1);

	sock_namelen = sizeof(lpwfs->socketAddress);
	getsockname(nsocket, (struct sockaddr *) &lpwfs->socketAddress, &sock_namelen);

        if (FTP_ConnectToHost(lpwfs))
        {
            TRACE("Successfully logged into server\n");
            bSuccess = TRUE;
        }
    }

lerror:
    if (!bSuccess)
    {
        if(lpwfs)
            WININET_Release( &lpwfs->hdr );
        return NULL;
    }

    return lpwfs->hdr.hInternet;
}


/***********************************************************************
 *           FTP_ConnectToHost (internal)
 *
 * Connect to a ftp server
 *
 * RETURNS
 *   TRUE on success
 *   NULL on failure
 *
 */
static BOOL FTP_ConnectToHost(ftp_session_t *lpwfs)
{
    INT nResCode;
    BOOL bSuccess = FALSE;

    TRACE("\n");
    FTP_ReceiveResponse(lpwfs, lpwfs->hdr.dwContext);

    if (!FTP_SendCommand(lpwfs->sndSocket, FTP_CMD_USER, lpwfs->lpszUserName, 0, 0, 0))
        goto lend;

    nResCode = FTP_ReceiveResponse(lpwfs, lpwfs->hdr.dwContext);
    if (nResCode)
    {
        /* Login successful... */
        if (nResCode == 230)
            bSuccess = TRUE;
        /* User name okay, need password... */
        else if (nResCode == 331)
            bSuccess = FTP_SendPassword(lpwfs);
        /* Need account for login... */
        else if (nResCode == 332)
            bSuccess = FTP_SendAccount(lpwfs);
        else
            FTP_SetResponseError(nResCode);
    }

    TRACE("Returning %d\n", bSuccess);
lend:
    return bSuccess;
}


/***********************************************************************
 *           FTP_SendCommandA (internal)
 *
 * Send command to server
 *
 * RETURNS
 *   TRUE on success
 *   NULL on failure
 *
 */
static BOOL FTP_SendCommandA(INT nSocket, FTP_COMMAND ftpCmd, LPCSTR lpszParam,
	INTERNET_STATUS_CALLBACK lpfnStatusCB, object_header_t *hdr, DWORD_PTR dwContext)
{
    	DWORD len;
	CHAR *buf;
	DWORD nBytesSent = 0;
	int nRC = 0;
	DWORD dwParamLen;

	TRACE("%d: (%s) %d\n", ftpCmd, lpszParam, nSocket);

	if (lpfnStatusCB)
        {
            lpfnStatusCB(hdr->hInternet, dwContext, INTERNET_STATUS_SENDING_REQUEST, NULL, 0);
        }

	dwParamLen = lpszParam?strlen(lpszParam)+1:0;
	len = dwParamLen + strlen(szFtpCommands[ftpCmd]) + strlen(szCRLF);
	if (NULL == (buf = heap_alloc(len+1)))
	{
	    INTERNET_SetLastError(ERROR_OUTOFMEMORY);
	    return FALSE;
	}
	sprintf(buf, "%s%s%s%s", szFtpCommands[ftpCmd], dwParamLen ? " " : "",
		dwParamLen ? lpszParam : "", szCRLF);

	TRACE("Sending (%s) len(%d)\n", buf, len);
	while((nBytesSent < len) && (nRC != -1))
	{
		nRC = send(nSocket, buf+nBytesSent, len - nBytesSent, 0);
		nBytesSent += nRC;
	}

	HeapFree(GetProcessHeap(), 0, (LPVOID)buf);

	if (lpfnStatusCB)
        {
            lpfnStatusCB(hdr->hInternet, dwContext, INTERNET_STATUS_REQUEST_SENT,
                         &nBytesSent, sizeof(DWORD));
        }

	TRACE("Sent %d bytes\n", nBytesSent);
	return (nRC != -1);
}

/***********************************************************************
 *           FTP_SendCommand (internal)
 *
 * Send command to server
 *
 * RETURNS
 *   TRUE on success
 *   NULL on failure
 *
 */
static BOOL FTP_SendCommand(INT nSocket, FTP_COMMAND ftpCmd, LPCWSTR lpszParam,
	INTERNET_STATUS_CALLBACK lpfnStatusCB, object_header_t *hdr, DWORD_PTR dwContext)
{
    BOOL ret;
    LPSTR lpszParamA = heap_strdupWtoA(lpszParam);
    ret = FTP_SendCommandA(nSocket, ftpCmd, lpszParamA, lpfnStatusCB, hdr, dwContext);
    HeapFree(GetProcessHeap(), 0, lpszParamA);
    return ret;
}

/***********************************************************************
 *           FTP_ReceiveResponse (internal)
 *
 * Receive response from server
 *
 * RETURNS
 *   Reply code on success
 *   0 on failure
 *
 */
INT FTP_ReceiveResponse(ftp_session_t *lpwfs, DWORD_PTR dwContext)
{
    LPSTR lpszResponse = INTERNET_GetResponseBuffer();
    DWORD nRecv;
    INT rc = 0;
    char firstprefix[5];
    BOOL multiline = FALSE;

    TRACE("socket(%d)\n", lpwfs->sndSocket);

    SendAsyncCallback(&lpwfs->hdr, dwContext, INTERNET_STATUS_RECEIVING_RESPONSE, NULL, 0);

    while(1)
    {
	if (!INTERNET_GetNextLine(lpwfs->sndSocket, &nRecv))
	    goto lerror;

        if (nRecv >= 3)
	{
	    if(!multiline)
	    {
	        if(lpszResponse[3] != '-')
		    break;
		else
		{  /* Start of multiline response.  Loop until we get "nnn " */
		    multiline = TRUE;
		    memcpy(firstprefix, lpszResponse, 3);
		    firstprefix[3] = ' ';
		    firstprefix[4] = '\0';
		}
	    }
	    else
	    {
	        if(!memcmp(firstprefix, lpszResponse, 4))
		    break;
	    }
	}
    }

    if (nRecv >= 3)
    {
        rc = atoi(lpszResponse);

        SendAsyncCallback(&lpwfs->hdr, dwContext, INTERNET_STATUS_RESPONSE_RECEIVED,
		    &nRecv, sizeof(DWORD));
    }

lerror:
    TRACE("return %d\n", rc);
    return rc;
}


/***********************************************************************
 *           FTP_SendPassword (internal)
 *
 * Send password to ftp server
 *
 * RETURNS
 *   TRUE on success
 *   NULL on failure
 *
 */
static BOOL FTP_SendPassword(ftp_session_t *lpwfs)
{
    INT nResCode;
    BOOL bSuccess = FALSE;

    TRACE("\n");
    if (!FTP_SendCommand(lpwfs->sndSocket, FTP_CMD_PASS, lpwfs->lpszPassword, 0, 0, 0))
        goto lend;

    nResCode = FTP_ReceiveResponse(lpwfs, lpwfs->hdr.dwContext);
    if (nResCode)
    {
        TRACE("Received reply code %d\n", nResCode);
        /* Login successful... */
        if (nResCode == 230)
            bSuccess = TRUE;
        /* Command not implemented, superfluous at the server site... */
        /* Need account for login... */
        else if (nResCode == 332)
            bSuccess = FTP_SendAccount(lpwfs);
        else
            FTP_SetResponseError(nResCode);
    }

lend:
    TRACE("Returning %d\n", bSuccess);
    return bSuccess;
}


/***********************************************************************
 *           FTP_SendAccount (internal)
 *
 *
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 */
static BOOL FTP_SendAccount(ftp_session_t *lpwfs)
{
    INT nResCode;
    BOOL bSuccess = FALSE;

    TRACE("\n");
    if (!FTP_SendCommand(lpwfs->sndSocket, FTP_CMD_ACCT, szNoAccount, 0, 0, 0))
        goto lend;

    nResCode = FTP_ReceiveResponse(lpwfs, lpwfs->hdr.dwContext);
    if (nResCode)
        bSuccess = TRUE;
    else
        FTP_SetResponseError(nResCode);

lend:
    return bSuccess;
}


/***********************************************************************
 *           FTP_SendStore (internal)
 *
 * Send request to upload file to ftp server
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 */
static BOOL FTP_SendStore(ftp_session_t *lpwfs, LPCWSTR lpszRemoteFile, DWORD dwType)
{
    INT nResCode;
    BOOL bSuccess = FALSE;

    TRACE("\n");
    if (!FTP_InitListenSocket(lpwfs))
        goto lend;

    if (!FTP_SendType(lpwfs, dwType))
        goto lend;

    if (!FTP_SendPortOrPasv(lpwfs))
        goto lend;

    if (!FTP_SendCommand(lpwfs->sndSocket, FTP_CMD_STOR, lpszRemoteFile, 0, 0, 0))
	    goto lend;
    nResCode = FTP_ReceiveResponse(lpwfs, lpwfs->hdr.dwContext);
    if (nResCode)
    {
        if (nResCode == 150 || nResCode == 125)
            bSuccess = TRUE;
	else
            FTP_SetResponseError(nResCode);
    }

lend:
    if (!bSuccess && lpwfs->lstnSocket != -1)
    {
        closesocket(lpwfs->lstnSocket);
        lpwfs->lstnSocket = -1;
    }

    return bSuccess;
}


/***********************************************************************
 *           FTP_InitListenSocket (internal)
 *
 * Create a socket to listen for server response
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 */
static BOOL FTP_InitListenSocket(ftp_session_t *lpwfs)
{
    BOOL bSuccess = FALSE;
    socklen_t namelen = sizeof(lpwfs->lstnSocketAddress);

    TRACE("\n");

    lpwfs->lstnSocket = socket(PF_INET, SOCK_STREAM, 0);
    if (lpwfs->lstnSocket == -1)
    {
        TRACE("Unable to create listening socket\n");
            goto lend;
    }

    /* We obtain our ip addr from the name of the command channel socket */
    lpwfs->lstnSocketAddress = lpwfs->socketAddress;

    /* and get the system to assign us a port */
    lpwfs->lstnSocketAddress.sin_port = htons(0);

    if (bind(lpwfs->lstnSocket,(struct sockaddr *) &lpwfs->lstnSocketAddress, sizeof(lpwfs->lstnSocketAddress)) == -1)
    {
        TRACE("Unable to bind socket\n");
        goto lend;
    }

    if (listen(lpwfs->lstnSocket, MAX_BACKLOG) == -1)
    {
        TRACE("listen failed\n");
        goto lend;
    }

    if (getsockname(lpwfs->lstnSocket, (struct sockaddr *) &lpwfs->lstnSocketAddress, &namelen) != -1)
        bSuccess = TRUE;

lend:
    if (!bSuccess && lpwfs->lstnSocket != -1)
    {
        closesocket(lpwfs->lstnSocket);
        lpwfs->lstnSocket = -1;
    }

    return bSuccess;
}


/***********************************************************************
 *           FTP_SendType (internal)
 *
 * Tell server type of data being transferred
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 * W98SE doesn't cache the type that's currently set
 * (i.e. it sends it always),
 * so we probably don't want to do that either.
 */
static BOOL FTP_SendType(ftp_session_t *lpwfs, DWORD dwType)
{
    INT nResCode;
    WCHAR type[] = { 'I','\0' };
    BOOL bSuccess = FALSE;

    TRACE("\n");
    if (dwType & INTERNET_FLAG_TRANSFER_ASCII)
        type[0] = 'A';

    if (!FTP_SendCommand(lpwfs->sndSocket, FTP_CMD_TYPE, type, 0, 0, 0))
        goto lend;

    nResCode = FTP_ReceiveResponse(lpwfs, lpwfs->hdr.dwContext)/100;
    if (nResCode)
    {
        if (nResCode == 2)
            bSuccess = TRUE;
	else
            FTP_SetResponseError(nResCode);
    }

lend:
    return bSuccess;
}


#if 0  /* FIXME: should probably be used for FtpGetFileSize */
/***********************************************************************
 *           FTP_GetFileSize (internal)
 *
 * Retrieves from the server the size of the given file
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 */
static BOOL FTP_GetFileSize(ftp_session_t *lpwfs, LPCWSTR lpszRemoteFile, DWORD *dwSize)
{
    INT nResCode;
    BOOL bSuccess = FALSE;

    TRACE("\n");

    if (!FTP_SendCommand(lpwfs->sndSocket, FTP_CMD_SIZE, lpszRemoteFile, 0, 0, 0))
        goto lend;

    nResCode = FTP_ReceiveResponse(lpwfs, lpwfs->hdr.dwContext);
    if (nResCode)
    {
        if (nResCode == 213) {
	    /* Now parses the output to get the actual file size */
	    int i;
	    LPSTR lpszResponseBuffer = INTERNET_GetResponseBuffer();

	    for (i = 0; (lpszResponseBuffer[i] != ' ') && (lpszResponseBuffer[i] != '\0'); i++) ;
	    if (lpszResponseBuffer[i] == '\0') return FALSE;
	    *dwSize = atol(&(lpszResponseBuffer[i + 1]));
	    
            bSuccess = TRUE;
	} else {
            FTP_SetResponseError(nResCode);
	}
    }

lend:
    return bSuccess;
}
#endif


/***********************************************************************
 *           FTP_SendPort (internal)
 *
 * Tell server which port to use
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 */
static BOOL FTP_SendPort(ftp_session_t *lpwfs)
{
    static const WCHAR szIPFormat[] = {'%','d',',','%','d',',','%','d',',','%','d',',','%','d',',','%','d','\0'};
    INT nResCode;
    WCHAR szIPAddress[64];
    BOOL bSuccess = FALSE;
    TRACE("\n");

    sprintfW(szIPAddress, szIPFormat,
	 lpwfs->lstnSocketAddress.sin_addr.s_addr&0x000000FF,
        (lpwfs->lstnSocketAddress.sin_addr.s_addr&0x0000FF00)>>8,
        (lpwfs->lstnSocketAddress.sin_addr.s_addr&0x00FF0000)>>16,
        (lpwfs->lstnSocketAddress.sin_addr.s_addr&0xFF000000)>>24,
        lpwfs->lstnSocketAddress.sin_port & 0xFF,
        (lpwfs->lstnSocketAddress.sin_port & 0xFF00)>>8);

    if (!FTP_SendCommand(lpwfs->sndSocket, FTP_CMD_PORT, szIPAddress, 0, 0, 0))
        goto lend;

    nResCode = FTP_ReceiveResponse(lpwfs, lpwfs->hdr.dwContext);
    if (nResCode)
    {
        if (nResCode == 200)
            bSuccess = TRUE;
        else
            FTP_SetResponseError(nResCode);
    }

lend:
    return bSuccess;
}


/***********************************************************************
 *           FTP_DoPassive (internal)
 *
 * Tell server that we want to do passive transfers
 * and connect data socket
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 */
static BOOL FTP_DoPassive(ftp_session_t *lpwfs)
{
    INT nResCode;
    BOOL bSuccess = FALSE;

    TRACE("\n");
    if (!FTP_SendCommand(lpwfs->sndSocket, FTP_CMD_PASV, NULL, 0, 0, 0))
        goto lend;

    nResCode = FTP_ReceiveResponse(lpwfs, lpwfs->hdr.dwContext);
    if (nResCode)
    {
        if (nResCode == 227)
	{
	    LPSTR lpszResponseBuffer = INTERNET_GetResponseBuffer();
	    LPSTR p;
	    int f[6];
	    int i;
	    char *pAddr, *pPort;
	    INT nsocket = -1;
	    struct sockaddr_in dataSocketAddress;

	    p = lpszResponseBuffer+4; /* skip status code */
	    while (*p != '\0' && (*p < '0' || *p > '9')) p++;

	    if (*p == '\0')
	    {
		ERR("no address found in response, aborting\n");
		goto lend;
	    }

	    if (sscanf(p, "%d,%d,%d,%d,%d,%d",  &f[0], &f[1], &f[2], &f[3],
				    		&f[4], &f[5]) != 6)
	    {
		ERR("unknown response address format '%s', aborting\n", p);
		goto lend;
	    }
	    for (i=0; i < 6; i++)
		f[i] = f[i] & 0xff;

	    dataSocketAddress = lpwfs->socketAddress;
	    pAddr = (char *)&(dataSocketAddress.sin_addr.s_addr);
	    pPort = (char *)&(dataSocketAddress.sin_port);
            pAddr[0] = f[0];
            pAddr[1] = f[1];
            pAddr[2] = f[2];
            pAddr[3] = f[3];
	    pPort[0] = f[4];
	    pPort[1] = f[5];

            nsocket = socket(AF_INET,SOCK_STREAM,0);
            if (nsocket == -1)
                goto lend;

	    if (connect(nsocket, (struct sockaddr *)&dataSocketAddress, sizeof(dataSocketAddress)))
            {
	        ERR("can't connect passive FTP data port.\n");
                closesocket(nsocket);
	        goto lend;
            }
	    lpwfs->pasvSocket = nsocket;
            bSuccess = TRUE;
	}
        else
            FTP_SetResponseError(nResCode);
    }

lend:
    return bSuccess;
}


static BOOL FTP_SendPortOrPasv(ftp_session_t *lpwfs)
{
    if (lpwfs->hdr.dwFlags & INTERNET_FLAG_PASSIVE)
    {
        if (!FTP_DoPassive(lpwfs))
            return FALSE;
    }
    else
    {
	if (!FTP_SendPort(lpwfs))
            return FALSE;
    }
    return TRUE;
}


/***********************************************************************
 *           FTP_GetDataSocket (internal)
 *
 * Either accepts an incoming data socket connection from the server
 * or just returns the already opened socket after a PASV command
 * in case of passive FTP.
 *
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 */
static BOOL FTP_GetDataSocket(ftp_session_t *lpwfs, LPINT nDataSocket)
{
    struct sockaddr_in saddr;
    socklen_t addrlen = sizeof(struct sockaddr);

    TRACE("\n");
    if (lpwfs->hdr.dwFlags & INTERNET_FLAG_PASSIVE)
    {
	*nDataSocket = lpwfs->pasvSocket;
	lpwfs->pasvSocket = -1;
    }
    else
    {
        *nDataSocket = accept(lpwfs->lstnSocket, (struct sockaddr *) &saddr, &addrlen);
        closesocket(lpwfs->lstnSocket);
        lpwfs->lstnSocket = -1;
    }
    return *nDataSocket != -1;
}


/***********************************************************************
 *           FTP_SendData (internal)
 *
 * Send data to the server
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 */
static BOOL FTP_SendData(ftp_session_t *lpwfs, INT nDataSocket, HANDLE hFile)
{
    BY_HANDLE_FILE_INFORMATION fi;
    DWORD nBytesRead = 0;
    DWORD nBytesSent = 0;
    DWORD nTotalSent = 0;
    DWORD nBytesToSend, nLen;
    int nRC = 1;
    time_t s_long_time, e_long_time;
    LONG nSeconds;
    CHAR *lpszBuffer;

    TRACE("\n");
    lpszBuffer = heap_alloc_zero(sizeof(CHAR)*DATA_PACKET_SIZE);

    /* Get the size of the file. */
    GetFileInformationByHandle(hFile, &fi);
    time(&s_long_time);

    do
    {
        nBytesToSend = nBytesRead - nBytesSent;

        if (nBytesToSend <= 0)
        {
            /* Read data from file. */
            nBytesSent = 0;
            if (!ReadFile(hFile, lpszBuffer, DATA_PACKET_SIZE, &nBytesRead, 0))
            ERR("Failed reading from file\n");

            if (nBytesRead > 0)
                nBytesToSend = nBytesRead;
            else
                break;
        }

        nLen = DATA_PACKET_SIZE < nBytesToSend ?
            DATA_PACKET_SIZE : nBytesToSend;
        nRC  = send(nDataSocket, lpszBuffer, nLen, 0);

        if (nRC != -1)
        {
            nBytesSent += nRC;
            nTotalSent += nRC;
        }

        /* Do some computation to display the status. */
        time(&e_long_time);
        nSeconds = e_long_time - s_long_time;
        if( nSeconds / 60 > 0 )
        {
            TRACE( "%d bytes of %d bytes (%d%%) in %d min %d sec estimated remaining time %d sec\n",
            nTotalSent, fi.nFileSizeLow, nTotalSent*100/fi.nFileSizeLow, nSeconds / 60,
            nSeconds % 60, (fi.nFileSizeLow - nTotalSent) * nSeconds / nTotalSent );
        }
        else
        {
            TRACE( "%d bytes of %d bytes (%d%%) in %d sec estimated remaining time %d sec\n",
            nTotalSent, fi.nFileSizeLow, nTotalSent*100/fi.nFileSizeLow, nSeconds,
            (fi.nFileSizeLow - nTotalSent) * nSeconds / nTotalSent);
        }
    } while (nRC != -1);

    TRACE("file transfer complete!\n");

    HeapFree(GetProcessHeap(), 0, lpszBuffer);

    return nTotalSent;
}


/***********************************************************************
 *           FTP_SendRetrieve (internal)
 *
 * Send request to retrieve a file
 *
 * RETURNS
 *   Number of bytes to be received on success
 *   0 on failure
 *
 */
static BOOL FTP_SendRetrieve(ftp_session_t *lpwfs, LPCWSTR lpszRemoteFile, DWORD dwType)
{
    INT nResCode;
    BOOL ret;

    TRACE("\n");
    if (!(ret = FTP_InitListenSocket(lpwfs)))
        goto lend;

    if (!(ret = FTP_SendType(lpwfs, dwType)))
        goto lend;

    if (!(ret = FTP_SendPortOrPasv(lpwfs)))
        goto lend;

    if (!(ret = FTP_SendCommand(lpwfs->sndSocket, FTP_CMD_RETR, lpszRemoteFile, 0, 0, 0)))
        goto lend;

    nResCode = FTP_ReceiveResponse(lpwfs, lpwfs->hdr.dwContext);
    if ((nResCode != 125) && (nResCode != 150)) {
	/* That means that we got an error getting the file. */
        FTP_SetResponseError(nResCode);
	ret = FALSE;
    }

lend:
    if (!ret && lpwfs->lstnSocket != -1)
    {
        closesocket(lpwfs->lstnSocket);
        lpwfs->lstnSocket = -1;
    }

    return ret;
}


/***********************************************************************
 *           FTP_RetrieveData  (internal)
 *
 * Retrieve data from server
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 */
static BOOL FTP_RetrieveFileData(ftp_session_t *lpwfs, INT nDataSocket, HANDLE hFile)
{
    DWORD nBytesWritten;
    INT nRC = 0;
    CHAR *lpszBuffer;

    TRACE("\n");

    lpszBuffer = heap_alloc_zero(sizeof(CHAR)*DATA_PACKET_SIZE);
    if (NULL == lpszBuffer)
    {
        INTERNET_SetLastError(ERROR_OUTOFMEMORY);
        return FALSE;
    }

    while (nRC != -1)
    {
        nRC = recv(nDataSocket, lpszBuffer, DATA_PACKET_SIZE, 0);
        if (nRC != -1)
        {
            /* other side closed socket. */
            if (nRC == 0)
                goto recv_end;
            WriteFile(hFile, lpszBuffer, nRC, &nBytesWritten, NULL);
        }
    }

    TRACE("Data transfer complete\n");

recv_end:
    HeapFree(GetProcessHeap(), 0, lpszBuffer);

    return  (nRC != -1);
}

/***********************************************************************
 *           FTPFINDNEXT_Destroy (internal)
 *
 * Deallocate session handle
 */
static void FTPFINDNEXT_Destroy(object_header_t *hdr)
{
    LPWININETFTPFINDNEXTW lpwfn = (LPWININETFTPFINDNEXTW) hdr;
    DWORD i;

    TRACE("\n");

    WININET_Release(&lpwfn->lpFtpSession->hdr);

    for (i = 0; i < lpwfn->size; i++)
    {
        HeapFree(GetProcessHeap(), 0, lpwfn->lpafp[i].lpszName);
    }

    HeapFree(GetProcessHeap(), 0, lpwfn->lpafp);
}

static DWORD FTPFINDNEXT_FindNextFileProc(WININETFTPFINDNEXTW *find, LPVOID data)
{
    WIN32_FIND_DATAW *find_data = data;
    DWORD res = ERROR_SUCCESS;

    TRACE("index(%d) size(%d)\n", find->index, find->size);

    ZeroMemory(find_data, sizeof(WIN32_FIND_DATAW));

    if (find->index < find->size) {
        FTP_ConvertFileProp(&find->lpafp[find->index], find_data);
        find->index++;

        TRACE("Name: %s\nSize: %d\n", debugstr_w(find_data->cFileName), find_data->nFileSizeLow);
    }else {
        res = ERROR_NO_MORE_FILES;
    }

    if (find->hdr.dwFlags & INTERNET_FLAG_ASYNC)
    {
        INTERNET_ASYNC_RESULT iar;

        iar.dwResult = (res == ERROR_SUCCESS);
        iar.dwError = res;

        INTERNET_SendCallback(&find->hdr, find->hdr.dwContext,
                              INTERNET_STATUS_REQUEST_COMPLETE, &iar,
                              sizeof(INTERNET_ASYNC_RESULT));
    }

    return res;
}

static void FTPFINDNEXT_AsyncFindNextFileProc(WORKREQUEST *workRequest)
{
    struct WORKREQ_FTPFINDNEXTW *req = &workRequest->u.FtpFindNextW;

    FTPFINDNEXT_FindNextFileProc((WININETFTPFINDNEXTW*)workRequest->hdr, req->lpFindFileData);
}

static DWORD FTPFINDNEXT_QueryOption(object_header_t *hdr, DWORD option, void *buffer, DWORD *size, BOOL unicode)
{
    switch(option) {
    case INTERNET_OPTION_HANDLE_TYPE:
        TRACE("INTERNET_OPTION_HANDLE_TYPE\n");

        if (*size < sizeof(ULONG))
            return ERROR_INSUFFICIENT_BUFFER;

        *size = sizeof(DWORD);
        *(DWORD*)buffer = INTERNET_HANDLE_TYPE_FTP_FIND;
        return ERROR_SUCCESS;
    }

    return INET_QueryOption(hdr, option, buffer, size, unicode);
}

static DWORD FTPFINDNEXT_FindNextFileW(object_header_t *hdr, void *data)
{
    WININETFTPFINDNEXTW *find = (WININETFTPFINDNEXTW*)hdr;

    if (find->lpFtpSession->lpAppInfo->hdr.dwFlags & INTERNET_FLAG_ASYNC)
    {
        WORKREQUEST workRequest;
        struct WORKREQ_FTPFINDNEXTW *req;

        workRequest.asyncproc = FTPFINDNEXT_AsyncFindNextFileProc;
        workRequest.hdr = WININET_AddRef( &find->hdr );
        req = &workRequest.u.FtpFindNextW;
        req->lpFindFileData = data;

	INTERNET_AsyncCall(&workRequest);

        return ERROR_SUCCESS;
    }

    return FTPFINDNEXT_FindNextFileProc(find, data);
}

static const object_vtbl_t FTPFINDNEXTVtbl = {
    FTPFINDNEXT_Destroy,
    NULL,
    FTPFINDNEXT_QueryOption,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    FTPFINDNEXT_FindNextFileW
};

/***********************************************************************
 *           FTP_ReceiveFileList (internal)
 *
 * Read file list from server
 *
 * RETURNS
 *   Handle to file list on success
 *   NULL on failure
 *
 */
static HINTERNET FTP_ReceiveFileList(ftp_session_t *lpwfs, INT nSocket, LPCWSTR lpszSearchFile,
	LPWIN32_FIND_DATAW lpFindFileData, DWORD_PTR dwContext)
{
    DWORD dwSize = 0;
    LPFILEPROPERTIESW lpafp = NULL;
    LPWININETFTPFINDNEXTW lpwfn = NULL;

    TRACE("(%p,%d,%s,%p,%08lx)\n", lpwfs, nSocket, debugstr_w(lpszSearchFile), lpFindFileData, dwContext);

    if (FTP_ParseDirectory(lpwfs, nSocket, lpszSearchFile, &lpafp, &dwSize))
    {
        if(lpFindFileData)
            FTP_ConvertFileProp(lpafp, lpFindFileData);

        lpwfn = alloc_object(&lpwfs->hdr, &FTPFINDNEXTVtbl, sizeof(WININETFTPFINDNEXTW));
        if (lpwfn)
        {
            lpwfn->hdr.htype = WH_HFTPFINDNEXT;
            lpwfn->hdr.dwContext = dwContext;
            lpwfn->index = 1; /* Next index is 1 since we return index 0 */
            lpwfn->size = dwSize;
            lpwfn->lpafp = lpafp;

            WININET_AddRef( &lpwfs->hdr );
            lpwfn->lpFtpSession = lpwfs;
            list_add_head( &lpwfs->hdr.children, &lpwfn->hdr.entry );
        }
    }

    TRACE("Matched %d files\n", dwSize);
    return lpwfn ? lpwfn->hdr.hInternet : NULL;
}


/***********************************************************************
 *           FTP_ConvertFileProp (internal)
 *
 * Converts FILEPROPERTIESW struct to WIN32_FIND_DATAA
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 */
static BOOL FTP_ConvertFileProp(LPFILEPROPERTIESW lpafp, LPWIN32_FIND_DATAW lpFindFileData)
{
    BOOL bSuccess = FALSE;

    ZeroMemory(lpFindFileData, sizeof(WIN32_FIND_DATAW));

    if (lpafp)
    {
        SystemTimeToFileTime( &lpafp->tmLastModified, &lpFindFileData->ftLastAccessTime );
	lpFindFileData->ftLastWriteTime = lpFindFileData->ftLastAccessTime;
	lpFindFileData->ftCreationTime = lpFindFileData->ftLastAccessTime;
	
        /* Not all fields are filled in */
        lpFindFileData->nFileSizeHigh = 0; /* We do not handle files bigger than 0xFFFFFFFF bytes yet :-) */
        lpFindFileData->nFileSizeLow = lpafp->nSize;

	if (lpafp->bIsDirectory)
	    lpFindFileData->dwFileAttributes |= FILE_ATTRIBUTE_DIRECTORY;

        if (lpafp->lpszName)
            lstrcpynW(lpFindFileData->cFileName, lpafp->lpszName, MAX_PATH);

	bSuccess = TRUE;
    }

    return bSuccess;
}

/***********************************************************************
 *           FTP_ParseNextFile (internal)
 *
 * Parse the next line in file listing
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 */
static BOOL FTP_ParseNextFile(INT nSocket, LPCWSTR lpszSearchFile, LPFILEPROPERTIESW lpfp)
{
    static const char szSpace[] = " \t";
    DWORD nBufLen;
    char *pszLine;
    char *pszToken;
    char *pszTmp;
    BOOL found = FALSE;
    int i;
    
    lpfp->lpszName = NULL;
    do {
        if(!(pszLine = INTERNET_GetNextLine(nSocket, &nBufLen)))
            return FALSE;
    
        pszToken = strtok(pszLine, szSpace);
        /* ls format
         * <Permissions> <NoLinks> <owner>   <group> <size> <date>  <time or year> <filename>
         *
         * For instance:
         * drwx--s---     2         pcarrier  ens     512    Sep 28  1995           pcarrier
         */
        if(!isdigit(pszToken[0]) && 10 == strlen(pszToken)) {
            if(!FTP_ParsePermission(pszToken, lpfp))
                lpfp->bIsDirectory = FALSE;
            for(i=0; i<=3; i++) {
              if(!(pszToken = strtok(NULL, szSpace)))
                  break;
            }
            if(!pszToken) continue;
            if(lpfp->bIsDirectory) {
                TRACE("Is directory\n");
                lpfp->nSize = 0;
            }
            else {
                TRACE("Size: %s\n", pszToken);
                lpfp->nSize = atol(pszToken);
            }
            
            lpfp->tmLastModified.wSecond = 0;
            lpfp->tmLastModified.wMinute = 0;
            lpfp->tmLastModified.wHour   = 0;
            lpfp->tmLastModified.wDay    = 0;
            lpfp->tmLastModified.wMonth  = 0;
            lpfp->tmLastModified.wYear   = 0;
            
            /* Determine month */
            pszToken = strtok(NULL, szSpace);
            if(!pszToken) continue;
            if(strlen(pszToken) >= 3) {
                pszToken[3] = 0;
                if((pszTmp = StrStrIA(szMonths, pszToken)))
                    lpfp->tmLastModified.wMonth = ((pszTmp - szMonths) / 3)+1;
            }
            /* Determine day */
            pszToken = strtok(NULL, szSpace);
            if(!pszToken) continue;
            lpfp->tmLastModified.wDay = atoi(pszToken);
            /* Determine time or year */
            pszToken = strtok(NULL, szSpace);
            if(!pszToken) continue;
            if((pszTmp = strchr(pszToken, ':'))) {
                SYSTEMTIME curr_time;
                *pszTmp = 0;
                pszTmp++;
                lpfp->tmLastModified.wMinute = atoi(pszTmp);
                lpfp->tmLastModified.wHour = atoi(pszToken);
                GetLocalTime( &curr_time );
                lpfp->tmLastModified.wYear = curr_time.wYear;
            }
            else {
                lpfp->tmLastModified.wYear = atoi(pszToken);
                lpfp->tmLastModified.wHour = 12;
            }
            TRACE("Mod time: %02d:%02d:%02d  %04d/%02d/%02d\n",
                  lpfp->tmLastModified.wHour, lpfp->tmLastModified.wMinute, lpfp->tmLastModified.wSecond,
                  lpfp->tmLastModified.wYear, lpfp->tmLastModified.wMonth, lpfp->tmLastModified.wDay);

            pszToken = strtok(NULL, szSpace);
            if(!pszToken) continue;
            lpfp->lpszName = heap_strdupAtoW(pszToken);
            TRACE("File: %s\n", debugstr_w(lpfp->lpszName));
        }
        /* NT way of parsing ... :
            
                07-13-03  08:55PM       <DIR>          sakpatch
                05-09-03  06:02PM             12656686 2003-04-21bgm_cmd_e.rgz
        */
        else if(isdigit(pszToken[0]) && 8 == strlen(pszToken)) {
            int mon, mday, year, hour, min;
            lpfp->permissions = 0xFFFF; /* No idea, put full permission :-) */
            
            sscanf(pszToken, "%d-%d-%d", &mon, &mday, &year);
            lpfp->tmLastModified.wDay   = mday;
            lpfp->tmLastModified.wMonth = mon;
            lpfp->tmLastModified.wYear  = year;

            /* Hacky and bad Y2K protection :-) */
            if (lpfp->tmLastModified.wYear < 70) lpfp->tmLastModified.wYear += 2000;

            pszToken = strtok(NULL, szSpace);
            if(!pszToken) continue;
            sscanf(pszToken, "%d:%d", &hour, &min);
            lpfp->tmLastModified.wHour   = hour;
            lpfp->tmLastModified.wMinute = min;
            if((pszToken[5] == 'P') && (pszToken[6] == 'M')) {
                lpfp->tmLastModified.wHour += 12;
            }
            lpfp->tmLastModified.wSecond = 0;

            TRACE("Mod time: %02d:%02d:%02d  %04d/%02d/%02d\n",
                  lpfp->tmLastModified.wHour, lpfp->tmLastModified.wMinute, lpfp->tmLastModified.wSecond,
                  lpfp->tmLastModified.wYear, lpfp->tmLastModified.wMonth, lpfp->tmLastModified.wDay);

            pszToken = strtok(NULL, szSpace);
            if(!pszToken) continue;
            if(!strcasecmp(pszToken, "<DIR>")) {
                lpfp->bIsDirectory = TRUE;
                lpfp->nSize = 0;
                TRACE("Is directory\n");
            }
            else {
                lpfp->bIsDirectory = FALSE;
                lpfp->nSize = atol(pszToken);
                TRACE("Size: %d\n", lpfp->nSize);
            }
            
            pszToken = strtok(NULL, szSpace);
            if(!pszToken) continue;
            lpfp->lpszName = heap_strdupAtoW(pszToken);
            TRACE("Name: %s\n", debugstr_w(lpfp->lpszName));
        }
        /* EPLF format - http://cr.yp.to/ftp/list/eplf.html */
        else if(pszToken[0] == '+') {
            FIXME("EPLF Format not implemented\n");
        }
        
        if(lpfp->lpszName) {
            if((lpszSearchFile == NULL) ||
	       (PathMatchSpecW(lpfp->lpszName, lpszSearchFile))) {
                found = TRUE;
                TRACE("Matched: %s\n", debugstr_w(lpfp->lpszName));
            }
            else {
                HeapFree(GetProcessHeap(), 0, lpfp->lpszName);
                lpfp->lpszName = NULL;
            }
        }
    } while(!found);
    return TRUE;
}

/***********************************************************************
 *           FTP_ParseDirectory (internal)
 *
 * Parse string of directory information
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 */
static BOOL FTP_ParseDirectory(ftp_session_t *lpwfs, INT nSocket, LPCWSTR lpszSearchFile,
    LPFILEPROPERTIESW *lpafp, LPDWORD dwfp)
{
    BOOL bSuccess = TRUE;
    INT sizeFilePropArray = 500;/*20; */
    INT indexFilePropArray = -1;

    TRACE("\n");

    /* Allocate initial file properties array */
    *lpafp = heap_alloc_zero(sizeof(FILEPROPERTIESW)*(sizeFilePropArray));
    if (!*lpafp)
        return FALSE;

    do {
        if (indexFilePropArray+1 >= sizeFilePropArray)
        {
            LPFILEPROPERTIESW tmpafp;
            
            sizeFilePropArray *= 2;
            tmpafp = heap_realloc_zero(*lpafp, sizeof(FILEPROPERTIESW)*sizeFilePropArray);
            if (NULL == tmpafp)
            {
                bSuccess = FALSE;
                break;
            }

            *lpafp = tmpafp;
        }
        indexFilePropArray++;
    } while (FTP_ParseNextFile(nSocket, lpszSearchFile, &(*lpafp)[indexFilePropArray]));

    if (bSuccess && indexFilePropArray)
    {
        if (indexFilePropArray < sizeFilePropArray - 1)
        {
            LPFILEPROPERTIESW tmpafp;

            tmpafp = heap_realloc(*lpafp, sizeof(FILEPROPERTIESW)*indexFilePropArray);
            if (NULL != tmpafp)
                *lpafp = tmpafp;
        }
        *dwfp = indexFilePropArray;
    }
    else
    {
        HeapFree(GetProcessHeap(), 0, *lpafp);
        INTERNET_SetLastError(ERROR_NO_MORE_FILES);
        bSuccess = FALSE;
    }

    return bSuccess;
}


/***********************************************************************
 *           FTP_ParsePermission (internal)
 *
 * Parse permission string of directory information
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 */
static BOOL FTP_ParsePermission(LPCSTR lpszPermission, LPFILEPROPERTIESW lpfp)
{
    BOOL bSuccess = TRUE;
    unsigned short nPermission = 0;
    INT nPos = 1;
    INT nLast  = 9;

    TRACE("\n");
    if ((*lpszPermission != 'd') && (*lpszPermission != '-') && (*lpszPermission != 'l'))
    {
        bSuccess = FALSE;
        return bSuccess;
    }

    lpfp->bIsDirectory = (*lpszPermission == 'd');
    do
    {
        switch (nPos)
        {
            case 1:
                nPermission |= (*(lpszPermission+1) == 'r' ? 1 : 0) << 8;
                break;
            case 2:
                nPermission |= (*(lpszPermission+2) == 'w' ? 1 : 0) << 7;
                break;
            case 3:
                nPermission |= (*(lpszPermission+3) == 'x' ? 1 : 0) << 6;
                break;
            case 4:
                nPermission |= (*(lpszPermission+4) == 'r' ? 1 : 0) << 5;
                break;
            case 5:
                nPermission |= (*(lpszPermission+5) == 'w' ? 1 : 0) << 4;
                break;
            case 6:
                nPermission |= (*(lpszPermission+6) == 'x' ? 1 : 0) << 3;
                break;
            case 7:
                nPermission |= (*(lpszPermission+7) == 'r' ? 1 : 0) << 2;
                break;
            case 8:
                nPermission |= (*(lpszPermission+8) == 'w' ? 1 : 0) << 1;
                break;
            case 9:
                nPermission |= (*(lpszPermission+9) == 'x' ? 1 : 0);
                break;
        }
        nPos++;
    }while (nPos <= nLast);

    lpfp->permissions = nPermission;
    return bSuccess;
}


/***********************************************************************
 *           FTP_SetResponseError (internal)
 *
 * Set the appropriate error code for a given response from the server
 *
 * RETURNS
 *
 */
static DWORD FTP_SetResponseError(DWORD dwResponse)
{
    DWORD dwCode = 0;

    switch(dwResponse)
    {
    case 425: /* Cannot open data connection. */
        dwCode = ERROR_INTERNET_CANNOT_CONNECT;
        break;

    case 426: /* Connection closed, transer aborted. */
        dwCode = ERROR_INTERNET_CONNECTION_ABORTED;
        break;

    case 530: /* Not logged in. Login incorrect. */
        dwCode = ERROR_INTERNET_LOGIN_FAILURE;
        break;

    case 421: /* Service not available - Server may be shutting down. */
    case 450: /* File action not taken. File may be busy. */
    case 451: /* Action aborted. Server error. */
    case 452: /* Action not taken. Insufficient storage space on server. */
    case 500: /* Syntax error. Command unrecognized. */
    case 501: /* Syntax error. Error in parameters or arguments. */
    case 502: /* Command not implemented. */
    case 503: /* Bad sequence of commands. */
    case 504: /* Command not implemented for that parameter. */
    case 532: /* Need account for storing files */
    case 550: /* File action not taken. File not found or no access. */
    case 551: /* Requested action aborted. Page type unknown */
    case 552: /* Action aborted. Exceeded storage allocation */
    case 553: /* Action not taken. File name not allowed. */

    default:
        dwCode = ERROR_INTERNET_EXTENDED_ERROR;
        break;
    }

    INTERNET_SetLastError(dwCode);
    return dwCode;
}
