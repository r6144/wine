/*
 * Wine Conformance Test EXE
 *
 * Copyright 2003, 2004 Jakob Eriksson   (for Solid Form Sweden AB)
 * Copyright 2003 Dimitrie O. Paun
 * Copyright 2003 Ferenc Wagner
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
 *
 * This program is dedicated to Anna Lindh,
 * Swedish Minister of Foreign Affairs.
 * Anna was murdered September 11, 2003.
 *
 */

#include "config.h"
#include "wine/port.h"

#define COBJMACROS
#include <stdio.h>
#include <assert.h>
#include <windows.h>
#include <mshtml.h>

#include "winetest.h"
#include "resource.h"

struct wine_test
{
    char *name;
    int resource;
    int subtest_count;
    char **subtests;
    char *exename;
    char *maindllpath;
};

char *tag = NULL;
char *email = NULL;
BOOL aborting = FALSE;
static struct wine_test *wine_tests;
static int nr_of_files, nr_of_tests;
static int nr_native_dlls;
static const char whitespace[] = " \t\r\n";
static const char testexe[] = "_test.exe";
static char build_id[64];

/* filters for running only specific tests */
static char *filters[64];
static unsigned int nb_filters = 0;

/* Needed to check for .NET dlls */
static HMODULE hmscoree;
static HRESULT (WINAPI *pLoadLibraryShim)(LPCWSTR, LPCWSTR, LPVOID, HMODULE *);

/* For SxS DLLs e.g. msvcr90 */
static HANDLE (WINAPI *pCreateActCtxA)(PACTCTXA);
static BOOL (WINAPI *pActivateActCtx)(HANDLE, ULONG_PTR *);
static BOOL (WINAPI *pDeactivateActCtx)(DWORD, ULONG_PTR);
static void (WINAPI *pReleaseActCtx)(HANDLE);

/* To store the current PATH setting (related to .NET only provided dlls) */
static char *curpath;

/* check if test is being filtered out */
static BOOL test_filtered_out( LPCSTR module, LPCSTR testname )
{
    char *p, dllname[MAX_PATH];
    unsigned int i, len;

    strcpy( dllname, module );
    CharLowerA( dllname );
    p = strstr( dllname, testexe );
    if (p) *p = 0;
    len = strlen(dllname);

    if (!nb_filters) return FALSE;
    for (i = 0; i < nb_filters; i++)
    {
        if (!strncmp( dllname, filters[i], len ))
        {
            if (!filters[i][len]) return FALSE;
            if (filters[i][len] != ':') continue;
            if (!testname || !strcmp( testname, &filters[i][len+1] )) return FALSE;
        }
    }
    return TRUE;
}

static char * get_file_version(char * file_name)
{
    static char version[32];
    DWORD size;
    DWORD handle;

    size = GetFileVersionInfoSizeA(file_name, &handle);
    if (size) {
        char * data = heap_alloc(size);
        if (data) {
            if (GetFileVersionInfoA(file_name, handle, size, data)) {
                static char backslash[] = "\\";
                VS_FIXEDFILEINFO *pFixedVersionInfo;
                UINT len;
                if (VerQueryValueA(data, backslash, (LPVOID *)&pFixedVersionInfo, &len)) {
                    sprintf(version, "%d.%d.%d.%d",
                            pFixedVersionInfo->dwFileVersionMS >> 16,
                            pFixedVersionInfo->dwFileVersionMS & 0xffff,
                            pFixedVersionInfo->dwFileVersionLS >> 16,
                            pFixedVersionInfo->dwFileVersionLS & 0xffff);
                } else
                    sprintf(version, "version not available");
            } else
                sprintf(version, "unknown");
            heap_free(data);
        } else
            sprintf(version, "failed");
    } else
        sprintf(version, "version not available");

    return version;
}

static int running_under_wine (void)
{
    HMODULE module = GetModuleHandleA("ntdll.dll");

    if (!module) return 0;
    return (GetProcAddress(module, "wine_server_call") != NULL);
}

static int check_mount_mgr(void)
{
    if (running_under_wine())
    {
        HANDLE handle = CreateFileA( "\\\\.\\MountPointManager", GENERIC_READ,
                                     FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0 );
        if (handle == INVALID_HANDLE_VALUE) return FALSE;
        CloseHandle( handle );
    }
    return TRUE;
}

static int check_display_driver(void)
{
    if (running_under_wine())
    {
        HWND hwnd = CreateWindowA( "STATIC", "", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0,
                                   0, 0, GetModuleHandleA(0), 0 );
        if (!hwnd) return FALSE;
        DestroyWindow( hwnd );
    }
    return TRUE;
}

static int running_on_visible_desktop (void)
{
    HWND desktop;
    HMODULE huser32 = GetModuleHandle("user32.dll");
    HWINSTA (WINAPI *pGetProcessWindowStation)(void);
    BOOL (WINAPI *pGetUserObjectInformationA)(HANDLE,INT,LPVOID,DWORD,LPDWORD);

    pGetProcessWindowStation = (void *)GetProcAddress(huser32, "GetProcessWindowStation");
    pGetUserObjectInformationA = (void *)GetProcAddress(huser32, "GetUserObjectInformationA");

    desktop = GetDesktopWindow();
    if (!GetWindowLongPtrW(desktop, GWLP_WNDPROC)) /* Win9x */
        return IsWindowVisible(desktop);

    if (pGetProcessWindowStation && pGetUserObjectInformationA)
    {
        DWORD len;
        HWINSTA wstation;
        USEROBJECTFLAGS uoflags;

        wstation = (HWINSTA)pGetProcessWindowStation();
        assert(pGetUserObjectInformationA(wstation, UOI_FLAGS, &uoflags, sizeof(uoflags), &len));
        return (uoflags.dwFlags & WSF_VISIBLE) != 0;
    }
    return IsWindowVisible(desktop);
}

/* check for native dll when running under wine */
static BOOL is_native_dll( HMODULE module )
{
    static const char fakedll_signature[] = "Wine placeholder DLL";
    const IMAGE_DOS_HEADER *dos;

    if (!running_under_wine()) return FALSE;
    if (!((ULONG_PTR)module & 1)) return FALSE;  /* not loaded as datafile */
    /* builtin dlls can't be loaded as datafile, so we must have native or fake dll */
    dos = (const IMAGE_DOS_HEADER *)((const char *)module - 1);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return FALSE;
    if (dos->e_lfanew >= sizeof(*dos) + sizeof(fakedll_signature) &&
        !memcmp( dos + 1, fakedll_signature, sizeof(fakedll_signature) )) return FALSE;
    return TRUE;
}

static void print_version (void)
{
#ifdef __i386__
    static const char platform[] = "i386";
#elif defined(__x86_64__)
    static const char platform[] = "x86_64";
#elif defined(__sparc__)
    static const char platform[] = "sparc";
#elif defined(__ALPHA__)
    static const char platform[] = "alpha";
#elif defined(__powerpc__)
    static const char platform[] = "powerpc";
#else
# error CPU unknown
#endif
    OSVERSIONINFOEX ver;
    BOOL ext, wow64;
    int is_win2k3_r2;
    const char *(CDECL *wine_get_build_id)(void);
    void (CDECL *wine_get_host_version)( const char **sysname, const char **release );
    BOOL (WINAPI *pIsWow64Process)(HANDLE hProcess, PBOOL Wow64Process);
    BOOL (WINAPI *pGetProductInfo)(DWORD, DWORD, DWORD, DWORD, DWORD *);

    ver.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    if (!(ext = GetVersionEx ((OSVERSIONINFO *) &ver)))
    {
	ver.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	if (!GetVersionEx ((OSVERSIONINFO *) &ver))
	    report (R_FATAL, "Can't get OS version.");
    }
    pIsWow64Process = (void *)GetProcAddress(GetModuleHandleA("kernel32.dll"),"IsWow64Process");
    if (!pIsWow64Process || !pIsWow64Process( GetCurrentProcess(), &wow64 )) wow64 = FALSE;

    xprintf ("    Platform=%s%s\n", platform, wow64 ? " (WOW64)" : "");
    xprintf ("    bRunningUnderWine=%d\n", running_under_wine ());
    xprintf ("    bRunningOnVisibleDesktop=%d\n", running_on_visible_desktop ());
    xprintf ("    Submitter=%s\n", email );
    xprintf ("    dwMajorVersion=%u\n    dwMinorVersion=%u\n"
             "    dwBuildNumber=%u\n    PlatformId=%u\n    szCSDVersion=%s\n",
             ver.dwMajorVersion, ver.dwMinorVersion, ver.dwBuildNumber,
             ver.dwPlatformId, ver.szCSDVersion);

    wine_get_build_id = (void *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "wine_get_build_id");
    wine_get_host_version = (void *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "wine_get_host_version");
    if (wine_get_build_id) xprintf( "    WineBuild=%s\n", wine_get_build_id() );
    if (wine_get_host_version)
    {
        const char *sysname, *release;
        wine_get_host_version( &sysname, &release );
        xprintf( "    Host system=%s\n    Host version=%s\n", sysname, release );
    }
    is_win2k3_r2 = GetSystemMetrics(SM_SERVERR2);
    if(is_win2k3_r2)
        xprintf("    R2 build number=%d\n", is_win2k3_r2);

    if (!ext) return;

    xprintf ("    wServicePackMajor=%d\n    wServicePackMinor=%d\n"
             "    wSuiteMask=%d\n    wProductType=%d\n    wReserved=%d\n",
             ver.wServicePackMajor, ver.wServicePackMinor, ver.wSuiteMask,
             ver.wProductType, ver.wReserved);

    pGetProductInfo = (void *)GetProcAddress(GetModuleHandleA("kernel32.dll"),"GetProductInfo");
    if (pGetProductInfo && !running_under_wine())
    {
        DWORD prodtype = 0;

        pGetProductInfo(ver.dwMajorVersion, ver.dwMinorVersion, ver.wServicePackMajor, ver.wServicePackMinor, &prodtype);
        xprintf("    dwProductInfo=%u\n", prodtype);
    }
}

static inline int is_dot_dir(const char* x)
{
    return ((x[0] == '.') && ((x[1] == 0) || ((x[1] == '.') && (x[2] == 0))));
}

static void remove_dir (const char *dir)
{
    HANDLE  hFind;
    WIN32_FIND_DATA wfd;
    char path[MAX_PATH];
    size_t dirlen = strlen (dir);

    /* Make sure the directory exists before going further */
    memcpy (path, dir, dirlen);
    strcpy (path + dirlen++, "\\*");
    hFind = FindFirstFile (path, &wfd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        char *lp = wfd.cFileName;

        if (!lp[0]) lp = wfd.cAlternateFileName; /* ? FIXME not (!lp) ? */
        if (is_dot_dir (lp)) continue;
        strcpy (path + dirlen, lp);
        if (FILE_ATTRIBUTE_DIRECTORY & wfd.dwFileAttributes)
            remove_dir(path);
        else if (!DeleteFile (path))
            report (R_WARNING, "Can't delete file %s: error %d",
                    path, GetLastError ());
    } while (FindNextFile (hFind, &wfd));
    FindClose (hFind);
    if (!RemoveDirectory (dir))
        report (R_WARNING, "Can't remove directory %s: error %d",
                dir, GetLastError ());
}

static const char* get_test_source_file(const char* test, const char* subtest)
{
    static const char* special_dirs[][2] = {
	{ 0, 0 }
    };
    static char buffer[MAX_PATH];
    int i, len = strlen(test);

    if (len > 4 && !strcmp( test + len - 4, ".exe" ))
    {
        len = sprintf(buffer, "programs/%s", test) - 4;
        buffer[len] = 0;
    }
    else len = sprintf(buffer, "dlls/%s", test);

    for (i = 0; special_dirs[i][0]; i++) {
	if (strcmp(test, special_dirs[i][0]) == 0) {
            strcpy( buffer, special_dirs[i][1] );
            len = strlen(buffer);
	    break;
	}
    }

    sprintf(buffer + len, "/tests/%s.c", subtest);
    return buffer;
}

static void* extract_rcdata (LPCTSTR name, LPCTSTR type, DWORD* size)
{
    HRSRC rsrc;
    HGLOBAL hdl;
    LPVOID addr;
    
    if (!(rsrc = FindResource (NULL, name, type)) ||
        !(*size = SizeofResource (0, rsrc)) ||
        !(hdl = LoadResource (0, rsrc)) ||
        !(addr = LockResource (hdl)))
        return NULL;
    return addr;
}

/* Fills in the name and exename fields */
static void
extract_test (struct wine_test *test, const char *dir, LPTSTR res_name)
{
    BYTE* code;
    DWORD size;
    char *exepos;
    HANDLE hfile;
    DWORD written;

    code = extract_rcdata (res_name, "TESTRES", &size);
    if (!code) report (R_FATAL, "Can't find test resource %s: %d",
                       res_name, GetLastError ());
    test->name = heap_strdup( res_name );
    test->exename = strmake (NULL, "%s\\%s", dir, test->name);
    exepos = strstr (test->name, testexe);
    if (!exepos) report (R_FATAL, "Not an .exe file: %s", test->name);
    *exepos = 0;
    test->name = heap_realloc (test->name, exepos - test->name + 1);
    report (R_STEP, "Extracting: %s", test->name);

    hfile = CreateFileA(test->exename, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hfile == INVALID_HANDLE_VALUE)
        report (R_FATAL, "Failed to open file %s.", test->exename);

    if (!WriteFile(hfile, code, size, &written, NULL))
        report (R_FATAL, "Failed to write file %s.", test->exename);

    CloseHandle(hfile);
}

static DWORD wait_process( HANDLE process, DWORD timeout )
{
    DWORD wait, diff = 0, start = GetTickCount();
    MSG msg;

    while (diff < timeout)
    {
        wait = MsgWaitForMultipleObjects( 1, &process, FALSE, timeout - diff, QS_ALLINPUT );
        if (wait != WAIT_OBJECT_0 + 1) return wait;
        while (PeekMessageA( &msg, 0, 0, 0, PM_REMOVE )) DispatchMessage( &msg );
        diff = GetTickCount() - start;
    }
    return WAIT_TIMEOUT;
}

static void append_path( const char *path)
{
    char *newpath;

    newpath = heap_alloc(strlen(curpath) + 1 + strlen(path) + 1);
    strcpy(newpath, curpath);
    strcat(newpath, ";");
    strcat(newpath, path);
    SetEnvironmentVariableA("PATH", newpath);

    heap_free(newpath);
}

/* Run a command for MS milliseconds.  If OUT != NULL, also redirect
   stdout to there.

   Return the exit status, -2 if can't create process or the return
   value of WaitForSingleObject.
 */
static int
run_ex (char *cmd, HANDLE out_file, const char *tempdir, DWORD ms)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    DWORD wait, status;

    GetStartupInfo (&si);
    si.dwFlags    = STARTF_USESTDHANDLES;
    si.hStdInput  = GetStdHandle( STD_INPUT_HANDLE );
    si.hStdOutput = out_file ? out_file : GetStdHandle( STD_OUTPUT_HANDLE );
    si.hStdError  = out_file ? out_file : GetStdHandle( STD_ERROR_HANDLE );

    if (!CreateProcessA (NULL, cmd, NULL, NULL, TRUE, CREATE_DEFAULT_ERROR_MODE,
                         NULL, tempdir, &si, &pi))
        return -2;

    CloseHandle (pi.hThread);
    status = wait_process( pi.hProcess, ms );
    switch (status)
    {
    case WAIT_OBJECT_0:
        GetExitCodeProcess (pi.hProcess, &status);
        CloseHandle (pi.hProcess);
        return status;
    case WAIT_FAILED:
        report (R_ERROR, "Wait for '%s' failed: %d", cmd, GetLastError ());
        break;
    case WAIT_TIMEOUT:
        break;
    default:
        report (R_ERROR, "Wait returned %d", status);
        break;
    }
    if (!TerminateProcess (pi.hProcess, 257))
        report (R_ERROR, "TerminateProcess failed: %d", GetLastError ());
    wait = wait_process( pi.hProcess, 5000 );
    switch (wait)
    {
    case WAIT_OBJECT_0:
        break;
    case WAIT_FAILED:
        report (R_ERROR, "Wait for termination of '%s' failed: %d", cmd, GetLastError ());
        break;
    case WAIT_TIMEOUT:
        report (R_ERROR, "Can't kill process '%s'", cmd);
        break;
    default:
        report (R_ERROR, "Waiting for termination: %d", wait);
        break;
    }
    CloseHandle (pi.hProcess);
    return status;
}

static DWORD
get_subtests (const char *tempdir, struct wine_test *test, LPTSTR res_name)
{
    char *cmd;
    HANDLE subfile;
    DWORD err, total;
    char buffer[8192], *index;
    static const char header[] = "Valid test names:";
    int status, allocated;
    char tmpdir[MAX_PATH], subname[MAX_PATH];
    SECURITY_ATTRIBUTES sa;

    test->subtest_count = 0;

    if (!GetTempPathA( MAX_PATH, tmpdir ) ||
        !GetTempFileNameA( tmpdir, "sub", 0, subname ))
        report (R_FATAL, "Can't name subtests file.");

    /* make handle inheritable */
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    subfile = CreateFileA( subname, GENERIC_READ|GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           &sa, CREATE_ALWAYS, 0, NULL );

    if ((subfile == INVALID_HANDLE_VALUE) &&
        (GetLastError() == ERROR_INVALID_PARAMETER)) {
        /* FILE_SHARE_DELETE not supported on win9x */
        subfile = CreateFileA( subname, GENERIC_READ|GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           &sa, CREATE_ALWAYS, 0, NULL );
    }
    if (subfile == INVALID_HANDLE_VALUE) {
        err = GetLastError();
        report (R_ERROR, "Can't open subtests output of %s: %u",
                test->name, GetLastError());
        goto quit;
    }

    cmd = strmake (NULL, "%s --list", test->exename);
    if (test->maindllpath) {
        /* We need to add the path (to the main dll) to PATH */
        append_path(test->maindllpath);
    }
    status = run_ex (cmd, subfile, tempdir, 5000);
    err = GetLastError();
    if (test->maindllpath) {
        /* Restore PATH again */
        SetEnvironmentVariableA("PATH", curpath);
    }
    heap_free (cmd);

    if (status == -2)
    {
        report (R_ERROR, "Cannot run %s error %u", test->exename, err);
        goto quit;
    }

    SetFilePointer( subfile, 0, NULL, FILE_BEGIN );
    ReadFile( subfile, buffer, sizeof(buffer), &total, NULL );
    CloseHandle( subfile );
    if (sizeof buffer == total) {
        report (R_ERROR, "Subtest list of %s too big.",
                test->name, sizeof buffer);
        err = ERROR_OUTOFMEMORY;
        goto quit;
    }
    buffer[total] = 0;

    index = strstr (buffer, header);
    if (!index) {
        report (R_ERROR, "Can't parse subtests output of %s",
                test->name);
        err = ERROR_INTERNAL_ERROR;
        goto quit;
    }
    index += sizeof header;

    allocated = 10;
    test->subtests = heap_alloc (allocated * sizeof(char*));
    index = strtok (index, whitespace);
    while (index) {
        if (test->subtest_count == allocated) {
            allocated *= 2;
            test->subtests = heap_realloc (test->subtests,
                                           allocated * sizeof(char*));
        }
        if (!test_filtered_out( test->name, index ))
            test->subtests[test->subtest_count++] = heap_strdup(index);
        index = strtok (NULL, whitespace);
    }
    test->subtests = heap_realloc (test->subtests,
                                   test->subtest_count * sizeof(char*));
    err = 0;

 quit:
    if (!DeleteFileA (subname))
        report (R_WARNING, "Can't delete file '%s': %u", subname, GetLastError());
    return err;
}

static void
run_test (struct wine_test* test, const char* subtest, HANDLE out_file, const char *tempdir)
{
    int status;
    const char* file = get_test_source_file(test->name, subtest);
    char *cmd = strmake (NULL, "%s %s", test->exename, subtest);

    xprintf ("%s:%s start %s -\n", test->name, subtest, file);
    status = run_ex (cmd, out_file, tempdir, 120000);
    heap_free (cmd);
    xprintf ("%s:%s done (%d)\n", test->name, subtest, status);
}

static BOOL CALLBACK
EnumTestFileProc (HMODULE hModule, LPCTSTR lpszType,
                  LPTSTR lpszName, LONG_PTR lParam)
{
    if (!test_filtered_out( lpszName, NULL )) (*(int*)lParam)++;
    return TRUE;
}

static const struct clsid_mapping
{
    const char *name;
    CLSID clsid;
} clsid_list[] =
{
    {"oledb32", {0xc8b522d1, 0x5cf3, 0x11ce, {0xad, 0xe5, 0x00, 0xaa, 0x00, 0x44, 0x77, 0x3d}}},
    {NULL, {0, 0, 0, {0,0,0,0,0,0,0,0}}}
};


static BOOL get_main_clsid(const char *name, CLSID *clsid)
{
    const struct clsid_mapping *mapping;

    for(mapping = clsid_list; mapping->name; mapping++)
    {
        if(!strcasecmp(name, mapping->name))
        {
            *clsid = mapping->clsid;
            return TRUE;
        }
    }
    return FALSE;
}

static HMODULE load_com_dll(const char *name, char **path, char *filename)
{
    HMODULE dll = NULL;
    HKEY hkey;
    char keyname[100];
    char dllname[MAX_PATH];
    char *p;
    CLSID clsid;

    if(!get_main_clsid(name, &clsid)) return NULL;

    sprintf(keyname, "CLSID\\{%08x-%04x-%04x-%02x%2x-%02x%2x%02x%2x%02x%2x}\\InprocServer32",
            clsid.Data1, clsid.Data2, clsid.Data3, clsid.Data4[0], clsid.Data4[1],
            clsid.Data4[2], clsid.Data4[3], clsid.Data4[4], clsid.Data4[5],
            clsid.Data4[6], clsid.Data4[7]);

    if(RegOpenKeyA(HKEY_CLASSES_ROOT, keyname, &hkey) == ERROR_SUCCESS)
    {
        LONG size = sizeof(dllname);
        if(RegQueryValueA(hkey, NULL, dllname, &size) == ERROR_SUCCESS)
        {
            if ((dll = LoadLibraryExA(dllname, NULL, LOAD_LIBRARY_AS_DATAFILE)))
            {
                strcpy( filename, dllname );
                p = strrchr(dllname, '\\');
                if (p) *p = 0;
                *path = heap_strdup( dllname );
            }
        }
        RegCloseKey(hkey);
    }

    return dll;
}

static void get_dll_path(HMODULE dll, char **path, char *filename)
{
    char dllpath[MAX_PATH];

    GetModuleFileNameA(dll, dllpath, MAX_PATH);
    strcpy(filename, dllpath);
    *strrchr(dllpath, '\\') = '\0';
    *path = heap_strdup( dllpath );
}

static BOOL CALLBACK
extract_test_proc (HMODULE hModule, LPCTSTR lpszType,
                   LPTSTR lpszName, LONG_PTR lParam)
{
    const char *tempdir = (const char *)lParam;
    char dllname[MAX_PATH];
    char filename[MAX_PATH];
    WCHAR dllnameW[MAX_PATH];
    HMODULE dll;
    DWORD err;
    HANDLE actctx;
    ULONG_PTR cookie;

    if (aborting) return TRUE;
    if (test_filtered_out( lpszName, NULL )) return TRUE;

    CharLowerA(lpszName);
    extract_test (&wine_tests[nr_of_files], tempdir, lpszName);

    /* Check if the main dll is present on this system */
    strcpy(dllname, lpszName);
    *strstr(dllname, testexe) = 0;

    if (pCreateActCtxA != NULL && pActivateActCtx != NULL &&
        pDeactivateActCtx != NULL && pReleaseActCtx != NULL)
    {
        ACTCTXA actctxinfo;
        memset(&actctxinfo, 0, sizeof(ACTCTXA));
        actctxinfo.cbSize = sizeof(ACTCTXA);
        actctxinfo.dwFlags = ACTCTX_FLAG_RESOURCE_NAME_VALID;
        actctxinfo.lpSource = wine_tests[nr_of_files].exename;
        actctxinfo.lpResourceName = CREATEPROCESS_MANIFEST_RESOURCE_ID;
        actctx = pCreateActCtxA(&actctxinfo);
        if (actctx != INVALID_HANDLE_VALUE &&
            ! pActivateActCtx(actctx, &cookie))
        {
            pReleaseActCtx(actctx);
            actctx = INVALID_HANDLE_VALUE;
        }
    } else actctx = INVALID_HANDLE_VALUE;

    wine_tests[nr_of_files].maindllpath = NULL;
    strcpy(filename, dllname);
    dll = LoadLibraryExA(dllname, NULL, LOAD_LIBRARY_AS_DATAFILE);

    if (!dll) dll = load_com_dll(dllname, &wine_tests[nr_of_files].maindllpath, filename);

    if (!dll && pLoadLibraryShim)
    {
        MultiByteToWideChar(CP_ACP, 0, dllname, -1, dllnameW, MAX_PATH);
        if (SUCCEEDED( pLoadLibraryShim(dllnameW, NULL, NULL, &dll) ) && dll)
        {
            get_dll_path(dll, &wine_tests[nr_of_files].maindllpath, filename);
            FreeLibrary(dll);
            dll = LoadLibraryExA(filename, NULL, LOAD_LIBRARY_AS_DATAFILE);
        }
        else dll = 0;
    }

    if (!dll)
    {
        xprintf ("    %s=dll is missing\n", dllname);
        if (actctx != INVALID_HANDLE_VALUE)
        {
            pDeactivateActCtx(0, cookie);
            pReleaseActCtx(actctx);
        }
        return TRUE;
    }
    if (is_native_dll(dll))
    {
        FreeLibrary(dll);
        xprintf ("    %s=load error Configured as native\n", dllname);
        nr_native_dlls++;
        if (actctx != INVALID_HANDLE_VALUE)
        {
            pDeactivateActCtx(0, cookie);
            pReleaseActCtx(actctx);
        }
        return TRUE;
    }
    FreeLibrary(dll);

    if (!(err = get_subtests( tempdir, &wine_tests[nr_of_files], lpszName )))
    {
        xprintf ("    %s=%s\n", dllname, get_file_version(filename));
        nr_of_tests += wine_tests[nr_of_files].subtest_count;
        nr_of_files++;
    }
    else
    {
        xprintf ("    %s=load error %u\n", dllname, err);
    }

    if (actctx != INVALID_HANDLE_VALUE)
    {
        pDeactivateActCtx(0, cookie);
        pReleaseActCtx(actctx);
    }
    return TRUE;
}

static char *
run_tests (char *logname, char *outdir)
{
    int i;
    char *strres, *eol, *nextline;
    DWORD strsize;
    SECURITY_ATTRIBUTES sa;
    char tmppath[MAX_PATH], tempdir[MAX_PATH+4];
    DWORD needed;
    HMODULE kernel32;

    /* Get the current PATH only once */
    needed = GetEnvironmentVariableA("PATH", NULL, 0);
    curpath = heap_alloc(needed);
    GetEnvironmentVariableA("PATH", curpath, needed);

    SetErrorMode (SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);

    if (!GetTempPathA( MAX_PATH, tmppath ))
        report (R_FATAL, "Can't name temporary dir (check %%TEMP%%).");

    if (!logname) {
        static char tmpname[MAX_PATH];
        if (!GetTempFileNameA( tmppath, "res", 0, tmpname ))
            report (R_FATAL, "Can't name logfile.");
        logname = tmpname;
    }
    report (R_OUT, logname);

    /* make handle inheritable */
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    logfile = CreateFileA( logname, GENERIC_READ|GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           &sa, CREATE_ALWAYS, 0, NULL );

    if ((logfile == INVALID_HANDLE_VALUE) &&
        (GetLastError() == ERROR_INVALID_PARAMETER)) {
        /* FILE_SHARE_DELETE not supported on win9x */
        logfile = CreateFileA( logname, GENERIC_READ|GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           &sa, CREATE_ALWAYS, 0, NULL );
    }
    if (logfile == INVALID_HANDLE_VALUE)
        report (R_FATAL, "Could not open logfile: %u", GetLastError());

    /* try stable path for ZoneAlarm */
    if (!outdir) {
        strcpy( tempdir, tmppath );
        strcat( tempdir, "wct" );

        if (!CreateDirectoryA( tempdir, NULL ))
        {
            if (!GetTempFileNameA( tmppath, "wct", 0, tempdir ))
                report (R_FATAL, "Can't name temporary dir (check %%TEMP%%).");
            DeleteFileA( tempdir );
            if (!CreateDirectoryA( tempdir, NULL ))
                report (R_FATAL, "Could not create directory: %s", tempdir);
        }
    }
    else
        strcpy( tempdir, outdir);

    report (R_DIR, tempdir);

    xprintf ("Version 4\n");
    xprintf ("Tests from build %s\n", build_id[0] ? build_id : "-" );
    xprintf ("Archive: -\n");  /* no longer used */
    xprintf ("Tag: %s\n", tag);
    xprintf ("Build info:\n");
    strres = extract_rcdata ("BUILD_INFO", "STRINGRES", &strsize);
    while (strres) {
        eol = memchr (strres, '\n', strsize);
        if (!eol) {
            nextline = NULL;
            eol = strres + strsize;
        } else {
            strsize -= eol - strres + 1;
            nextline = strsize?eol+1:NULL;
            if (eol > strres && *(eol-1) == '\r') eol--;
        }
        xprintf ("    %.*s\n", eol-strres, strres);
        strres = nextline;
    }
    xprintf ("Operating system version:\n");
    print_version ();
    xprintf ("Dll info:\n" );

    report (R_STATUS, "Counting tests");
    if (!EnumResourceNames (NULL, "TESTRES", EnumTestFileProc, (LPARAM)&nr_of_files))
        report (R_FATAL, "Can't enumerate test files: %d",
                GetLastError ());
    wine_tests = heap_alloc (nr_of_files * sizeof wine_tests[0]);

    /* Do this only once during extraction (and version checking) */
    hmscoree = LoadLibraryA("mscoree.dll");
    pLoadLibraryShim = NULL;
    if (hmscoree)
        pLoadLibraryShim = (void *)GetProcAddress(hmscoree, "LoadLibraryShim");
    kernel32 = GetModuleHandleA("kernel32.dll");
    pCreateActCtxA = (void *)GetProcAddress(kernel32, "CreateActCtxA");
    pActivateActCtx = (void *)GetProcAddress(kernel32, "ActivateActCtx");
    pDeactivateActCtx = (void *)GetProcAddress(kernel32, "DeactivateActCtx");
    pReleaseActCtx = (void *)GetProcAddress(kernel32, "ReleaseActCtx");

    report (R_STATUS, "Extracting tests");
    report (R_PROGRESS, 0, nr_of_files);
    nr_of_files = 0;
    nr_of_tests = 0;
    if (!EnumResourceNames (NULL, "TESTRES", extract_test_proc, (LPARAM)tempdir))
        report (R_FATAL, "Can't enumerate test files: %d",
                GetLastError ());

    FreeLibrary(hmscoree);

    if (aborting) return logname;

    xprintf ("Test output:\n" );

    report (R_DELTA, 0, "Extracting: Done");

    if (nr_native_dlls)
        report( R_WARNING, "Some dlls are configured as native, you won't be able to submit results." );

    report (R_STATUS, "Running tests");
    report (R_PROGRESS, 1, nr_of_tests);
    for (i = 0; i < nr_of_files; i++) {
        struct wine_test *test = wine_tests + i;
        int j;

        if (aborting) break;

        if (test->maindllpath) {
            /* We need to add the path (to the main dll) to PATH */
            append_path(test->maindllpath);
        }

	for (j = 0; j < test->subtest_count; j++) {
            if (aborting) break;
            report (R_STEP, "Running: %s:%s", test->name,
                    test->subtests[j]);
	    run_test (test, test->subtests[j], logfile, tempdir);
        }

        if (test->maindllpath) {
            /* Restore PATH again */
            SetEnvironmentVariableA("PATH", curpath);
        }
    }
    report (R_DELTA, 0, "Running: Done");

    report (R_STATUS, "Cleaning up");
    CloseHandle( logfile );
    logfile = 0;
    if (!outdir)
        remove_dir (tempdir);
    heap_free(wine_tests);
    heap_free(curpath);

    return logname;
}

static BOOL WINAPI ctrl_handler(DWORD ctrl_type)
{
    if (ctrl_type == CTRL_C_EVENT) {
        printf("Ignoring Ctrl-C, use Ctrl-Break if you really want to terminate\n");
        return TRUE;
    }

    return FALSE;
}


static BOOL CALLBACK
extract_only_proc (HMODULE hModule, LPCTSTR lpszType, LPTSTR lpszName, LONG_PTR lParam)
{
    const char *target_dir = (const char *)lParam;
    char filename[MAX_PATH];

    if (test_filtered_out( lpszName, NULL )) return TRUE;

    strcpy(filename, lpszName);
    CharLowerA(filename);

    extract_test( &wine_tests[nr_of_files], target_dir, filename );
    nr_of_files++;
    return TRUE;
}

static void extract_only (const char *target_dir)
{
    BOOL res;

    report (R_DIR, target_dir);
    res = CreateDirectoryA( target_dir, NULL );
    if (!res && GetLastError() != ERROR_ALREADY_EXISTS)
        report (R_FATAL, "Could not create directory: %s (%d)", target_dir, GetLastError ());

    nr_of_files = 0;
    report (R_STATUS, "Counting tests");
    if (!EnumResourceNames (NULL, "TESTRES", EnumTestFileProc, (LPARAM)&nr_of_files))
        report (R_FATAL, "Can't enumerate test files: %d", GetLastError ());

    wine_tests = heap_alloc (nr_of_files * sizeof wine_tests[0] );

    report (R_STATUS, "Extracting tests");
    report (R_PROGRESS, 0, nr_of_files);
    nr_of_files = 0;
    if (!EnumResourceNames (NULL, "TESTRES", extract_only_proc, (LPARAM)target_dir))
        report (R_FATAL, "Can't enumerate test files: %d", GetLastError ());

    report (R_DELTA, 0, "Extracting: Done");
}

static void
usage (void)
{
    fprintf (stderr,
"Usage: winetest [OPTION]... [TESTS]\n\n"
" --help    print this message and exit\n"
" --version print the build version and exit\n"
" -c        console mode, no GUI\n"
" -d DIR    Use DIR as temp directory (default: %%TEMP%%\\wct)\n"
" -e        preserve the environment\n"
" -h        print this message and exit\n"
" -m MAIL   an email address to enable developers to contact you\n"
" -p        shutdown when the tests are done\n"
" -q        quiet mode, no output at all\n"
" -o FILE   put report into FILE, do not submit\n"
" -s FILE   submit FILE, do not run tests\n"
" -t TAG    include TAG of characters [-.0-9a-zA-Z] in the report\n"
" -x DIR    Extract tests to DIR (default: .\\wct) and exit\n");
}

int main( int argc, char *argv[] )
{
    char *logname = NULL, *outdir = NULL;
    const char *extract = NULL;
    const char *cp, *submit = NULL;
    int reset_env = 1;
    int poweroff = 0;
    int interactive = 1;
    int i;

    if (!LoadStringA( 0, IDS_BUILD_ID, build_id, sizeof(build_id) )) build_id[0] = 0;

    for (i = 1; i < argc && argv[i]; i++)
    {
        if (!strcmp(argv[i], "--help")) {
            usage ();
            exit (0);
        }
        else if (!strcmp(argv[i], "--version")) {
            printf("%-12.12s\n", build_id[0] ? build_id : "unknown");
            exit (0);
        }
        else if ((argv[i][0] != '-' && argv[i][0] != '/') || argv[i][2]) {
            if (nb_filters == sizeof(filters)/sizeof(filters[0]))
            {
                report (R_ERROR, "Too many test filters specified");
                exit (2);
            }
            filters[nb_filters++] = argv[i];
        }
        else switch (argv[i][1]) {
        case 'c':
            report (R_TEXTMODE);
            interactive = 0;
            break;
        case 'e':
            reset_env = 0;
            break;
        case 'h':
        case '?':
            usage ();
            exit (0);
        case 'm':
            if (!(email = argv[++i]))
            {
                usage();
                exit( 2 );
            }
            break;
        case 'p':
            poweroff = 1;
            break;
        case 'q':
            report (R_QUIET);
            interactive = 0;
            break;
        case 's':
            if (!(submit = argv[++i]))
            {
                usage();
                exit( 2 );
            }
            if (tag)
                report (R_WARNING, "ignoring tag for submission");
            send_file (submit);
            break;
        case 'o':
            if (!(logname = argv[++i]))
            {
                usage();
                exit( 2 );
            }
            break;
        case 't':
            if (!(tag = argv[++i]))
            {
                usage();
                exit( 2 );
            }
            if (strlen (tag) > MAXTAGLEN)
                report (R_FATAL, "tag is too long (maximum %d characters)",
                        MAXTAGLEN);
            cp = findbadtagchar (tag);
            if (cp) {
                report (R_ERROR, "invalid char in tag: %c", *cp);
                usage ();
                exit (2);
            }
            break;
        case 'x':
            report (R_TEXTMODE);
            if (!(extract = argv[++i]))
                extract = ".\\wct";

            extract_only (extract);
            break;
        case 'd':
            outdir = argv[++i];
            break;
        default:
            report (R_ERROR, "invalid option: -%c", argv[i][1]);
            usage ();
            exit (2);
        }
    }
    if (!submit && !extract) {
        report (R_STATUS, "Starting up");

        if (!running_on_visible_desktop ())
            report (R_FATAL, "Tests must be run on a visible desktop");

        if (!check_mount_mgr())
            report (R_FATAL, "Mount manager not running, most likely your WINEPREFIX wasn't created correctly.");

        if (!check_display_driver())
            report (R_FATAL, "Unable to create a window, the display driver is not working.");

        SetConsoleCtrlHandler(ctrl_handler, TRUE);

        if (reset_env)
        {
            SetEnvironmentVariableA( "WINETEST_PLATFORM", running_under_wine () ? "wine" : "windows" );
            SetEnvironmentVariableA( "WINETEST_DEBUG", "1" );
            SetEnvironmentVariableA( "WINETEST_INTERACTIVE", "0" );
            SetEnvironmentVariableA( "WINETEST_REPORT_SUCCESS", "0" );
        }

        if (!nb_filters)  /* don't submit results when filtering */
        {
            while (!tag) {
                if (!interactive)
                    report (R_FATAL, "Please specify a tag (-t option) if "
                            "running noninteractive!");
                if (guiAskTag () == IDABORT) exit (1);
            }
            report (R_TAG);

            while (!email) {
                if (!interactive)
                    report (R_FATAL, "Please specify an email address (-m option) to enable developers\n"
                            "    to contact you about your report if necessary.");
                if (guiAskEmail () == IDABORT) exit (1);
            }

            if (!build_id[0])
                report( R_WARNING, "You won't be able to submit results without a valid build id.\n"
                        "To submit results, winetest needs to be built from a git checkout." );
        }

        if (!logname) {
            logname = run_tests (NULL, outdir);
            if (aborting) {
                DeleteFileA(logname);
                exit (0);
            }
            if (build_id[0] && !nb_filters && !nr_native_dlls &&
                report (R_ASK, MB_YESNO, "Do you want to submit the test results?") == IDYES)
                if (!send_file (logname) && !DeleteFileA(logname))
                    report (R_WARNING, "Can't remove logfile: %u", GetLastError());
        } else run_tests (logname, outdir);
        report (R_STATUS, "Finished");
    }
    if (poweroff)
    {
        HANDLE hToken;
        TOKEN_PRIVILEGES npr;

        /* enable the shutdown privilege for the current process */
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken))
        {
            LookupPrivilegeValueA(0, SE_SHUTDOWN_NAME, &npr.Privileges[0].Luid);
            npr.PrivilegeCount = 1;
            npr.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            AdjustTokenPrivileges(hToken, FALSE, &npr, 0, 0, 0);
            CloseHandle(hToken);
        }
        ExitWindowsEx(EWX_SHUTDOWN | EWX_POWEROFF | EWX_FORCEIFHUNG, SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER);
    }
    exit (0);
}
