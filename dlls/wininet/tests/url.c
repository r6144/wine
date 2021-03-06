/*
 * Wininet - URL tests
 *
 * Copyright 2002 Aric Stewart
 * Copyright 2004 Mike McCormack
 * Copyright 2005 Hans Leidekker
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "winnls.h"
#include "wininet.h"

#include "wine/test.h"

#define TEST_URL "http://www.winehq.org/site/about#hi"
#define TEST_URL3 "file:///C:/Program%20Files/Atmel/AVR%20Tools/STK500/STK500.xml"

#define CREATE_URL1 "http://username:password@www.winehq.org/site/about"
#define CREATE_URL2 "http://username@www.winehq.org/site/about"
#define CREATE_URL3 "http://username:"
#define CREATE_URL4 "http://www.winehq.org/site/about"
#define CREATE_URL5 "http://"
#define CREATE_URL6 "nhttp://username:password@www.winehq.org:80/site/about"
#define CREATE_URL7 "http://username:password@www.winehq.org:42/site/about"
#define CREATE_URL8 "https://username:password@www.winehq.org/site/about"
#define CREATE_URL9 "about:blank"
#define CREATE_URL10 "about://host/blank"
#define CREATE_URL11 "about:"
#define CREATE_URL12 "http://www.winehq.org:65535"
#define CREATE_URL13 "http://localhost/?test=123"

static void copy_compsA(
    URL_COMPONENTSA *src, 
    URL_COMPONENTSA *dst, 
    DWORD scheLen,
    DWORD hostLen,
    DWORD userLen,
    DWORD passLen,
    DWORD pathLen,
    DWORD extrLen )
{
    *dst = *src;
    dst->dwSchemeLength    = scheLen;
    dst->dwHostNameLength  = hostLen;
    dst->dwUserNameLength  = userLen;
    dst->dwPasswordLength  = passLen;
    dst->dwUrlPathLength   = pathLen;
    dst->dwExtraInfoLength = extrLen;
    SetLastError(0xfaceabad);
}

static void zero_compsA(
    URL_COMPONENTSA *dst, 
    DWORD scheLen,
    DWORD hostLen,
    DWORD userLen,
    DWORD passLen,
    DWORD pathLen,
    DWORD extrLen )
{
    ZeroMemory(dst, sizeof(URL_COMPONENTSA));
    dst->dwStructSize = sizeof(URL_COMPONENTSA);
    dst->dwSchemeLength    = scheLen;
    dst->dwHostNameLength  = hostLen;
    dst->dwUserNameLength  = userLen;
    dst->dwPasswordLength  = passLen;
    dst->dwUrlPathLength   = pathLen;
    dst->dwExtraInfoLength = extrLen;
    SetLastError(0xfaceabad);
}

typedef struct {
    const char *url;
    int scheme_off;
    int scheme_len;
    INTERNET_SCHEME scheme;
    int host_off;
    int host_len;
    int host_skip_broken;
    INTERNET_PORT port;
    int user_off;
    int user_len;
    int pass_off;
    int pass_len;
    int path_off;
    int path_len;
    int extra_off;
    int extra_len;
    const char *exp_scheme;
    const char *exp_hostname;
    const char *exp_username;
    const char *exp_password;
    const char *exp_urlpath;
    const char *exp_extrainfo;
} crack_url_test_t;

static const crack_url_test_t crack_url_tests[] = {
    {"http://www.winehq.org/site/about#hi",
        0, 4, INTERNET_SCHEME_HTTP, 7, 14, -1, 80, -1, 0, -1, 0, 21, 11, 32, 3,
        "http", "www.winehq.org", "", "", "/site/about", "#hi"},
    {"http://www.myserver.com/myscript.php?arg=1",
        0, 4, INTERNET_SCHEME_HTTP, 7, 16, -1, 80, -1, 0, -1, 0, 23, 13, 36, 6,
        "http", "www.myserver.com", "", "", "/myscript.php", "?arg=1"},
    {"http://www.winehq.org?test=123",
        0, 4, INTERNET_SCHEME_HTTP, 7, 14, 23, 80, -1, 0, -1, 0, 21, 0, 21, 9,
        "http", "www.winehq.org", "", "", "", "?test=123"},
    {"file:///C:/Program%20Files/Atmel/AVR%20Tools/STK500/STK500.xml",
        0, 4, INTERNET_SCHEME_FILE, -1, 0, -1, 0, -1, 0, -1, 0, 7, 55, -1, 0,
        "file", "", "", "", "C:\\Program Files\\Atmel\\AVR Tools\\STK500\\STK500.xml", ""},
    {"fide:///C:/Program%20Files/Atmel/AVR%20Tools/STK500/STK500.xml",
        0, 4, INTERNET_SCHEME_UNKNOWN, 7, 0, -1, 0, -1, 0, -1, 0, 7, 55, -1, 0,
        "fide", "", "", "", "/C:/Program%20Files/Atmel/AVR%20Tools/STK500/STK500.xml", ""},
    {"file://C:/Program%20Files/Atmel/AVR%20Tools/STK500/STK500.xml",
        0, 4, INTERNET_SCHEME_FILE, -1, 0, -1, 0, -1, 0, -1, 0, 7, 54, -1, 0,
        "file", "", "", "", "C:\\Program%20Files\\Atmel\\AVR%20Tools\\STK500\\STK500.xml", ""},
    {"file://C:/Program%20Files/Atmel/..",
        0, 4, INTERNET_SCHEME_FILE, -1, 0, -1, 0, -1, 0, -1, 0, 7, 27, -1, 0,
        "file", "", "", "", "C:\\Program%20Files\\Atmel\\..\\", ""},
    {"file://C:/Program%20Files/Atmel/../Asdf.xml",
        0, 4, INTERNET_SCHEME_FILE, -1, 0, -1, 0, -1, 0, -1, 0, 7, 36, -1, 0,
        "file", "", "", "", "C:\\Program%20Files\\Atmel\\..\\Asdf.xml", ""},
    {"file:///C:/Program%20Files/Atmel/..",
        0, 4, INTERNET_SCHEME_FILE, -1, 0, -1, 0, -1, 0, -1, 0, 7, 28, -1, 0,
        "file", "", "", "", "C:\\Program Files\\Atmel\\..\\", ""},
    {"file:///C:/Program%20Files/Atmel/../Asdf.xml",
        0, 4, INTERNET_SCHEME_FILE, -1, 0, -1, 0, -1, 0, -1, 0, 7, 37, -1, 0,
        "file", "", "", "", "C:\\Program Files\\Atmel\\..\\Asdf.xml", ""},
    {"file://C:/Program%20Files/Atmel/.",
        0, 4, INTERNET_SCHEME_FILE, -1, 0, -1, 0, -1, 0, -1, 0, 7, 26, -1, 0,
        "file", "", "", "", "C:\\Program%20Files\\Atmel\\.\\", ""},
    {"file://C:/Program%20Files/Atmel/./Asdf.xml",
        0, 4, INTERNET_SCHEME_FILE, -1, 0, -1, 0, -1, 0, -1, 0, 7, 35, -1, 0,
        "file", "", "", "", "C:\\Program%20Files\\Atmel\\.\\Asdf.xml", ""},
    {"file:///C:/Program%20Files/Atmel/.",
        0, 4, INTERNET_SCHEME_FILE, -1, 0, -1, 0, -1, 0, -1, 0, 7, 27, -1, 0,
        "file", "", "", "", "C:\\Program Files\\Atmel\\.\\", ""},
    {"file:///C:/Program%20Files/Atmel/./Asdf.xml",
        0, 4, INTERNET_SCHEME_FILE, -1, 0, -1, 0, -1, 0, -1, 0, 7, 36, -1, 0,
        "file", "", "", "", "C:\\Program Files\\Atmel\\.\\Asdf.xml", ""},
};

static const WCHAR *w_str_of(const char *str)
{
    static WCHAR buf[512];
    MultiByteToWideChar(CP_ACP, 0, str, -1, buf, sizeof(buf)/sizeof(buf[0]));
    return buf;
}

static void test_crack_url(const crack_url_test_t *test)
{
    WCHAR buf[INTERNET_MAX_URL_LENGTH];
    URL_COMPONENTSW urlw;
    URL_COMPONENTSA url;
    char scheme[32], hostname[1024], username[1024];
    char password[1024], extrainfo[1024], urlpath[1024];
    BOOL b;

    /* test InternetCrackUrlA with NULL buffers */
    zero_compsA(&url, 1, 1, 1, 1, 1, 1);

    b = InternetCrackUrlA(test->url, strlen(test->url), 0, &url);
    ok(b, "InternetCrackUrl failed with error %d\n", GetLastError());

    if(test->scheme_off == -1)
        ok(!url.lpszScheme, "[%s] url.lpszScheme = %p, expected NULL\n", test->url, url.lpszScheme);
    else
        ok(url.lpszScheme == test->url+test->scheme_off, "[%s] url.lpszScheme = %p, expected %p\n",
           test->url, url.lpszScheme, test->url+test->scheme_off);
    ok(url.dwSchemeLength == test->scheme_len, "[%s] url.lpszSchemeLength = %d, expected %d\n",
       test->url, url.dwSchemeLength, test->scheme_len);

    ok(url.nScheme == test->scheme, "[%s] url.nScheme = %d, expected %d\n", test->url, url.nScheme, test->scheme);

    if(test->host_off == -1)
        ok(!url.lpszHostName, "[%s] url.lpszHostName = %p, expected NULL\n", test->url, url.lpszHostName);
    else
        ok(url.lpszHostName == test->url+test->host_off, "[%s] url.lpszHostName = %p, expected %p\n",
           test->url, url.lpszHostName, test->url+test->host_off);
    if(test->host_skip_broken != -1 && url.dwHostNameLength == test->host_skip_broken) {
        win_skip("skipping broken dwHostNameLength result\n");
        return;
    }
    ok(url.dwHostNameLength == test->host_len, "[%s] url.lpszHostNameLength = %d, expected %d\n",
       test->url, url.dwHostNameLength, test->host_len);

    ok(url.nPort == test->port, "[%s] nPort = %d, expected %d\n", test->url, url.nPort, test->port);

    if(test->user_off == -1)
        ok(!url.lpszUserName, "[%s] url.lpszUserName = %p\n", test->url, url.lpszUserName);
    else
        ok(url.lpszUserName == test->url+test->user_off, "[%s] url.lpszUserName = %p, expected %p\n",
           test->url, url.lpszUserName, test->url+test->user_off);
    ok(url.dwUserNameLength == test->user_len, "[%s] url.lpszUserNameLength = %d, expected %d\n",
       test->url, url.dwUserNameLength, test->user_len);

    if(test->pass_off == -1)
        ok(!url.lpszPassword, "[%s] url.lpszPassword = %p\n", test->url, url.lpszPassword);
    else
        ok(url.lpszPassword == test->url+test->pass_off, "[%s] url.lpszPassword = %p, expected %p\n",
           test->url, url.lpszPassword, test->url+test->pass_off);
    ok(url.dwPasswordLength == test->pass_len, "[%s] url.lpszPasswordLength = %d, expected %d\n",
       test->url, url.dwPasswordLength, test->pass_len);

    if(test->path_off == -1)
        ok(!url.lpszUrlPath, "[%s] url.lpszPath = %p, expected NULL\n", test->url, url.lpszUrlPath);
    else
        ok(url.lpszUrlPath == test->url+test->path_off, "[%s] url.lpszPath = %p, expected %p\n",
           test->url, url.lpszUrlPath, test->url+test->path_off);
    ok(url.dwUrlPathLength == test->path_len, "[%s] url.lpszUrlPathLength = %d, expected %d\n",
       test->url, url.dwUrlPathLength, test->path_len);

    if(test->extra_off == -1)
        ok(!url.lpszExtraInfo, "[%s] url.lpszExtraInfo = %p, expected NULL\n", test->url, url.lpszExtraInfo);
    else
        ok(url.lpszExtraInfo == test->url+test->extra_off, "[%s] url.lpszExtraInfo = %p, expected %p\n",
           test->url, url.lpszExtraInfo, test->url+test->extra_off);
    ok(url.dwExtraInfoLength == test->extra_len, "[%s] url.lpszExtraInfoLength = %d, expected %d\n",
       test->url, url.dwExtraInfoLength, test->extra_len);

    /* test InternetCrackUrlW with NULL buffers */
    memset(&urlw, 0, sizeof(URL_COMPONENTSW));
    urlw.dwStructSize = sizeof(URL_COMPONENTSW);
    urlw.dwSchemeLength = 1;
    urlw.dwHostNameLength = 1;
    urlw.dwUserNameLength = 1;
    urlw.dwPasswordLength = 1;
    urlw.dwUrlPathLength = 1;
    urlw.dwExtraInfoLength = 1;

    MultiByteToWideChar(CP_ACP, 0, test->url, -1, buf, sizeof(buf)/sizeof(buf[0]));
    b = InternetCrackUrlW(buf, lstrlenW(buf), 0, &urlw);
    if(!b && GetLastError() == ERROR_CALL_NOT_IMPLEMENTED) {
        win_skip("InternetCrackUrlW is not implemented\n");
        return;
    }
    ok(b, "InternetCrackUrl failed with error %d\n", GetLastError());

    if(test->scheme_off == -1)
        ok(!urlw.lpszScheme, "[%s] urlw.lpszScheme = %p, expected NULL\n", test->url, urlw.lpszScheme);
    else
        ok(urlw.lpszScheme == buf+test->scheme_off, "[%s] urlw.lpszScheme = %p, expected %p\n",
           test->url, urlw.lpszScheme, buf+test->scheme_off);
    ok(urlw.dwSchemeLength == test->scheme_len, "[%s] urlw.lpszSchemeLength = %d, expected %d\n",
       test->url, urlw.dwSchemeLength, test->scheme_len);

    ok(urlw.nScheme == test->scheme, "[%s] urlw.nScheme = %d, expected %d\n", test->url, urlw.nScheme, test->scheme);

    if(test->host_off == -1) {
        ok(!urlw.lpszHostName, "[%s] urlw.lpszHostName = %p, expected NULL\n", test->url, urlw.lpszHostName);
        ok(urlw.dwHostNameLength == 0 || broken(urlw.dwHostNameLength == 1), "[%s] urlw.lpszHostNameLength = %d, expected %d\n",
           test->url, urlw.dwHostNameLength, test->host_len);
    }else {
        ok(urlw.lpszHostName == buf+test->host_off, "[%s] urlw.lpszHostName = %p, expected %p\n",
           test->url, urlw.lpszHostName, test->url+test->host_off);
        ok(urlw.dwHostNameLength == test->host_len, "[%s] urlw.lpszHostNameLength = %d, expected %d\n",
           test->url, urlw.dwHostNameLength, test->host_len);
    }

    ok(urlw.nPort == test->port, "[%s] nPort = %d, expected %d\n", test->url, urlw.nPort, test->port);

    if(test->user_off == -1) {
        ok(!urlw.lpszUserName, "[%s] urlw.lpszUserName = %p\n", test->url, urlw.lpszUserName);
        ok(urlw.dwUserNameLength == 0 || broken(urlw.dwUserNameLength == 1), "[%s] urlw.lpszUserNameLength = %d, expected %d\n",
           test->url, urlw.dwUserNameLength, test->user_len);
    }else {
        ok(urlw.lpszUserName == buf+test->user_off, "[%s] urlw.lpszUserName = %p, expected %p\n",
           test->url, urlw.lpszUserName, buf+test->user_off);
        ok(urlw.dwUserNameLength == test->user_len, "[%s] urlw.lpszUserNameLength = %d, expected %d\n",
           test->url, urlw.dwUserNameLength, test->user_len);
    }

    if(test->pass_off == -1) {
        ok(!urlw.lpszPassword, "[%s] urlw.lpszPassword = %p\n", test->url, urlw.lpszPassword);
        ok(urlw.dwPasswordLength == 0 || broken(urlw.dwPasswordLength), "[%s] urlw.lpszPasswordLength = %d, expected %d\n",
           test->url, urlw.dwPasswordLength, test->pass_len);
    }else {
        ok(urlw.lpszPassword == buf+test->pass_off, "[%s] urlw.lpszPassword = %p, expected %p\n",
           test->url, urlw.lpszPassword, buf+test->pass_off);
        ok(urlw.dwPasswordLength == test->pass_len, "[%s] urlw.lpszPasswordLength = %d, expected %d\n",
           test->url, urlw.dwPasswordLength, test->pass_len);
    }

    if(test->path_off == -1)
        ok(!urlw.lpszUrlPath, "[%s] urlw.lpszPath = %p, expected NULL\n", test->url, urlw.lpszUrlPath);
    else
        ok(urlw.lpszUrlPath == buf+test->path_off, "[%s] urlw.lpszPath = %p, expected %p\n",
           test->url, urlw.lpszUrlPath, buf+test->path_off);
    ok(urlw.dwUrlPathLength == test->path_len, "[%s] urlw.lpszUrlPathLength = %d, expected %d\n",
       test->url, urlw.dwUrlPathLength, test->path_len);

    if(test->extra_off == -1) {
        ok(!urlw.lpszExtraInfo, "[%s] url.lpszExtraInfo = %p, expected NULL\n", test->url, urlw.lpszExtraInfo);
        ok(urlw.dwExtraInfoLength == 0 || broken(urlw.dwExtraInfoLength == 1), "[%s] urlw.lpszExtraInfoLength = %d, expected %d\n",
           test->url, urlw.dwExtraInfoLength, test->extra_len);
    }else {
        ok(urlw.lpszExtraInfo == buf+test->extra_off, "[%s] urlw.lpszExtraInfo = %p, expected %p\n",
           test->url, urlw.lpszExtraInfo, buf+test->extra_off);
        ok(urlw.dwExtraInfoLength == test->extra_len, "[%s] urlw.lpszExtraInfoLength = %d, expected %d\n",
           test->url, urlw.dwExtraInfoLength, test->extra_len);
    }

    /* test InternetCrackUrlA with valid buffers */
    memset(&url, 0, sizeof(URL_COMPONENTSA));
    url.dwStructSize = sizeof(URL_COMPONENTSA);
    url.lpszScheme = scheme;
    url.dwSchemeLength = sizeof(scheme);
    url.lpszHostName = hostname;
    url.dwHostNameLength = sizeof(hostname);
    url.lpszUserName = username;
    url.dwUserNameLength = sizeof(username);
    url.lpszPassword = password;
    url.dwPasswordLength = sizeof(password);
    url.lpszUrlPath = urlpath;
    url.dwUrlPathLength = sizeof(urlpath);
    url.lpszExtraInfo = extrainfo;
    url.dwExtraInfoLength = sizeof(extrainfo);

    b = InternetCrackUrlA(test->url, strlen(test->url), 0, &url);
    ok(b, "InternetCrackUrlA failed with error %d\n", GetLastError());

    ok(url.dwSchemeLength == strlen(test->exp_scheme), "[%s] Got wrong scheme length: %d\n",
            test->url, url.dwSchemeLength);
    ok(!strcmp(scheme, test->exp_scheme), "[%s] Got wrong scheme, expected: %s, got: %s\n",
            test->url, test->exp_scheme, scheme);

    ok(url.nScheme == test->scheme, "[%s] Got wrong nScheme, expected: %d, got: %d\n",
            test->url, test->scheme, url.nScheme);

    ok(url.dwHostNameLength == strlen(test->exp_hostname), "[%s] Got wrong hostname length: %d\n",
            test->url, url.dwHostNameLength);
    ok(!strcmp(hostname, test->exp_hostname), "[%s] Got wrong hostname, expected: %s, got: %s\n",
            test->url, test->exp_hostname, hostname);

    ok(url.nPort == test->port, "[%s] Got wrong port, expected: %d, got: %d\n",
            test->url, test->port, url.nPort);

    ok(url.dwUserNameLength == strlen(test->exp_username), "[%s] Got wrong username length: %d\n",
            test->url, url.dwUserNameLength);
    ok(!strcmp(username, test->exp_username), "[%s] Got wrong username, expected: %s, got: %s\n",
            test->url, test->exp_username, username);

    ok(url.dwPasswordLength == strlen(test->exp_password), "[%s] Got wrong password length: %d\n",
            test->url, url.dwPasswordLength);
    ok(!strcmp(password, test->exp_password), "[%s] Got wrong password, expected: %s, got: %s\n",
            test->url, test->exp_password, password);

    ok(url.dwUrlPathLength == strlen(test->exp_urlpath), "[%s] Got wrong urlpath length: %d\n",
            test->url, url.dwUrlPathLength);
    ok(!strcmp(urlpath, test->exp_urlpath), "[%s] Got wrong urlpath, expected: %s, got: %s\n",
            test->url, test->exp_urlpath, urlpath);

    ok(url.dwExtraInfoLength == strlen(test->exp_extrainfo), "[%s] Got wrong extrainfo length: %d\n",
            test->url, url.dwExtraInfoLength);
    ok(!strcmp(extrainfo, test->exp_extrainfo), "[%s] Got wrong extrainfo, expected: %s, got: %s\n",
            test->url, test->exp_extrainfo, extrainfo);

    /* test InternetCrackUrlW with valid buffers */
    memset(&urlw, 0, sizeof(URL_COMPONENTSW));
    urlw.dwStructSize = sizeof(URL_COMPONENTSW);
    urlw.lpszScheme = (WCHAR*)scheme;
    urlw.dwSchemeLength = sizeof(scheme) / sizeof(WCHAR);
    urlw.lpszHostName = (WCHAR*)hostname;
    urlw.dwHostNameLength = sizeof(hostname) / sizeof(WCHAR);
    urlw.lpszUserName = (WCHAR*)username;
    urlw.dwUserNameLength = sizeof(username) / sizeof(WCHAR);
    urlw.lpszPassword = (WCHAR*)password;
    urlw.dwPasswordLength = sizeof(password) / sizeof(WCHAR);
    urlw.lpszUrlPath = (WCHAR*)urlpath;
    urlw.dwUrlPathLength = sizeof(urlpath) / sizeof(WCHAR);
    urlw.lpszExtraInfo = (WCHAR*)extrainfo;
    urlw.dwExtraInfoLength = sizeof(extrainfo) / sizeof(WCHAR);

    b = InternetCrackUrlW(buf, lstrlenW(buf), 0, &urlw);
    ok(b, "InternetCrackUrlW failed with error %d\n", GetLastError());

    ok(urlw.dwSchemeLength == strlen(test->exp_scheme), "[%s] Got wrong scheme length: %d\n",
            test->url, urlw.dwSchemeLength);
    ok(!lstrcmpW((WCHAR*)scheme, w_str_of(test->exp_scheme)), "[%s] Got wrong scheme, expected: %s, got: %s\n",
            test->url, test->exp_scheme, wine_dbgstr_w((WCHAR*)scheme));

    ok(urlw.nScheme == test->scheme, "[%s] Got wrong nScheme, expected: %d, got: %d\n",
            test->url, test->scheme, urlw.nScheme);

    ok(urlw.dwHostNameLength == strlen(test->exp_hostname), "[%s] Got wrong hostname length: %d\n",
            test->url, urlw.dwHostNameLength);
    ok(!lstrcmpW((WCHAR*)hostname, w_str_of(test->exp_hostname)), "[%s] Got wrong hostname, expected: %s, got: %s\n",
            test->url, test->exp_hostname, wine_dbgstr_w((WCHAR*)hostname));

    ok(urlw.nPort == test->port, "[%s] Got wrong port, expected: %d, got: %d\n",
            test->url, test->port, urlw.nPort);

    ok(urlw.dwUserNameLength == strlen(test->exp_username), "[%s] Got wrong username length: %d\n",
            test->url, urlw.dwUserNameLength);
    ok(!lstrcmpW((WCHAR*)username, w_str_of(test->exp_username)), "[%s] Got wrong username, expected: %s, got: %s\n",
            test->url, test->exp_username, wine_dbgstr_w((WCHAR*)username));

    ok(urlw.dwPasswordLength == strlen(test->exp_password), "[%s] Got wrong password length: %d\n",
            test->url, urlw.dwPasswordLength);
    ok(!lstrcmpW((WCHAR*)password, w_str_of(test->exp_password)), "[%s] Got wrong password, expected: %s, got: %s\n",
            test->url, test->exp_password, wine_dbgstr_w((WCHAR*)password));

    ok(urlw.dwUrlPathLength == strlen(test->exp_urlpath), "[%s] Got wrong urlpath length: %d\n",
            test->url, urlw.dwUrlPathLength);
    ok(!lstrcmpW((WCHAR*)urlpath, w_str_of(test->exp_urlpath)), "[%s] Got wrong urlpath, expected: %s, got: %s\n",
            test->url, test->exp_urlpath, wine_dbgstr_w((WCHAR*)urlpath));

    ok(urlw.dwExtraInfoLength == strlen(test->exp_extrainfo), "[%s] Got wrong extrainfo length: %d\n",
            test->url, urlw.dwExtraInfoLength);
    ok(!lstrcmpW((WCHAR*)extrainfo, w_str_of(test->exp_extrainfo)), "[%s] Got wrong extrainfo, expected: %s, got: %s\n",
            test->url, test->exp_extrainfo, wine_dbgstr_w((WCHAR*)extrainfo));
}

static void InternetCrackUrl_test(void)
{
  URL_COMPONENTSA urlSrc, urlComponents;
  char protocol[32], hostName[1024], userName[1024];
  char password[1024], extra[1024], path[1024];
  BOOL ret, firstret;
  DWORD GLE, firstGLE;

  ZeroMemory(&urlSrc, sizeof(urlSrc));
  urlSrc.dwStructSize = sizeof(urlSrc);
  urlSrc.lpszScheme = protocol;
  urlSrc.lpszHostName = hostName;
  urlSrc.lpszUserName = userName;
  urlSrc.lpszPassword = password;
  urlSrc.lpszUrlPath = path;
  urlSrc.lpszExtraInfo = extra;

  /* Tests for lpsz* members pointing to real strings while 
   * some corresponding length members are set to zero.
   * As of IE7 (wininet 7.0*?) all members are checked. So we
   * run the first test and expect the outcome to be the same
   * for the first four (scheme, hostname, username and password).
   * The last two (path and extrainfo) are the same for all versions
   * of the wininet.dll.
   */
  copy_compsA(&urlSrc, &urlComponents, 0, 1024, 1024, 1024, 2048, 1024);
  SetLastError(0xdeadbeef);
  firstret = InternetCrackUrlA(TEST_URL3, 0, ICU_DECODE, &urlComponents);
  firstGLE = GetLastError();

  copy_compsA(&urlSrc, &urlComponents, 32, 0, 1024, 1024, 2048, 1024);
  SetLastError(0xdeadbeef);
  ret = InternetCrackUrlA(TEST_URL3, 0, ICU_DECODE, &urlComponents);
  GLE = GetLastError();
  ok(ret==firstret && (GLE==firstGLE), "InternetCrackUrl returned %d with GLE=%d (expected to return %d)\n",
    ret, GetLastError(), firstret);

  copy_compsA(&urlSrc, &urlComponents, 32, 1024, 0, 1024, 2048, 1024);
  SetLastError(0xdeadbeef);
  ret = InternetCrackUrlA(TEST_URL3, 0, ICU_DECODE, &urlComponents);
  GLE = GetLastError();
  ok(ret==firstret && (GLE==firstGLE), "InternetCrackUrl returned %d with GLE=%d (expected to return %d)\n",
    ret, GetLastError(), firstret);

  copy_compsA(&urlSrc, &urlComponents, 32, 1024, 1024, 0, 2048, 1024);
  SetLastError(0xdeadbeef);
  ret = InternetCrackUrlA(TEST_URL3, 0, ICU_DECODE, &urlComponents);
  GLE = GetLastError();
  ok(ret==firstret && (GLE==firstGLE), "InternetCrackUrl returned %d with GLE=%d (expected to return %d)\n",
    ret, GetLastError(), firstret);

  copy_compsA(&urlSrc, &urlComponents, 32, 1024, 1024, 1024, 0, 1024);
  SetLastError(0xdeadbeef);
  ret = InternetCrackUrlA(TEST_URL3, 0, ICU_DECODE, &urlComponents);
  GLE = GetLastError();
  todo_wine
  ok(ret==0 && (GLE==ERROR_INVALID_HANDLE || GLE==ERROR_INSUFFICIENT_BUFFER),
     "InternetCrackUrl returned %d with GLE=%d (expected to return 0 and ERROR_INVALID_HANDLE or ERROR_INSUFFICIENT_BUFFER)\n",
    ret, GLE);

  copy_compsA(&urlSrc, &urlComponents, 32, 1024, 1024, 1024, 2048, 0);
  SetLastError(0xdeadbeef);
  ret = InternetCrackUrlA(TEST_URL3, 0, ICU_DECODE, &urlComponents);
  GLE = GetLastError();
  todo_wine
  ok(ret==0 && (GLE==ERROR_INVALID_HANDLE || GLE==ERROR_INSUFFICIENT_BUFFER),
     "InternetCrackUrl returned %d with GLE=%d (expected to return 0 and ERROR_INVALID_HANDLE or ERROR_INSUFFICIENT_BUFFER)\n",
    ret, GLE);

  copy_compsA(&urlSrc, &urlComponents, 0, 0, 0, 0, 0, 0);
  ret = InternetCrackUrlA(TEST_URL3, 0, ICU_DECODE, &urlComponents);
  GLE = GetLastError();
  todo_wine
  ok(ret==0 && GLE==ERROR_INVALID_PARAMETER,
     "InternetCrackUrl returned %d with GLE=%d (expected to return 0 and ERROR_INVALID_PARAMETER)\n",
    ret, GLE);

  copy_compsA(&urlSrc, &urlComponents, 32, 1024, 1024, 1024, 2048, 1024);
  ret = InternetCrackUrl("about://host/blank", 0,0,&urlComponents);
  ok(ret, "InternetCrackUrl failed with %d\n", GetLastError());
  ok(!strcmp(urlComponents.lpszScheme, "about"), "lpszScheme was \"%s\" instead of \"about\"\n", urlComponents.lpszScheme);
  ok(!strcmp(urlComponents.lpszHostName, "host"), "lpszHostName was \"%s\" instead of \"host\"\n", urlComponents.lpszHostName);
  ok(!strcmp(urlComponents.lpszUrlPath, "/blank"), "lpszUrlPath was \"%s\" instead of \"/blank\"\n", urlComponents.lpszUrlPath);

  /* try a NULL lpszUrl */
  SetLastError(0xdeadbeef);
  copy_compsA(&urlSrc, &urlComponents, 32, 1024, 1024, 1024, 2048, 1024);
  ret = InternetCrackUrl(NULL, 0, 0, &urlComponents);
  GLE = GetLastError();
  ok(ret == FALSE, "Expected InternetCrackUrl to fail\n");
  ok(GLE == ERROR_INVALID_PARAMETER, "Expected ERROR_INVALID_PARAMETER, got %d\n", GLE);

  /* try an empty lpszUrl, GetLastError returns 12006, whatever that means
   * we just need to fail and not return success
   */
  SetLastError(0xdeadbeef);
  copy_compsA(&urlSrc, &urlComponents, 32, 1024, 1024, 1024, 2048, 1024);
  ret = InternetCrackUrl("", 0, 0, &urlComponents);
  GLE = GetLastError();
  ok(ret == FALSE, "Expected InternetCrackUrl to fail\n");
  ok(GLE != 0xdeadbeef && GLE != ERROR_SUCCESS, "Expected GLE to represent a failure\n");

  /* Invalid Call: must set size of components structure (Windows only
   * enforces this on the InternetCrackUrlA version of the call) */
  copy_compsA(&urlSrc, &urlComponents, 0, 1024, 1024, 1024, 2048, 1024);
  SetLastError(0xdeadbeef);
  urlComponents.dwStructSize = 0;
  ret = InternetCrackUrlA(TEST_URL, 0, 0, &urlComponents);
  ok(ret == FALSE, "Expected InternetCrackUrl to fail\n");
  ok(GLE != 0xdeadbeef && GLE != ERROR_SUCCESS, "Expected GLE to represent a failure\n");

  /* Invalid Call: size of dwStructSize must be one of the "standard" sizes
   * of the URL_COMPONENTS structure (Windows only enforces this on the
   * InternetCrackUrlA version of the call) */
  copy_compsA(&urlSrc, &urlComponents, 0, 1024, 1024, 1024, 2048, 1024);
  SetLastError(0xdeadbeef);
  urlComponents.dwStructSize = sizeof(urlComponents) + 1;
  ret = InternetCrackUrlA(TEST_URL, 0, 0, &urlComponents);
  ok(ret == FALSE, "Expected InternetCrackUrl to fail\n");
  ok(GLE != 0xdeadbeef && GLE != ERROR_SUCCESS, "Expected GLE to represent a failure\n");
}

static void InternetCrackUrlW_test(void)
{
    WCHAR url[] = {
        'h','t','t','p',':','/','/','1','9','2','.','1','6','8','.','0','.','2','2','/',
        'C','F','I','D','E','/','m','a','i','n','.','c','f','m','?','C','F','S','V','R',
        '=','I','D','E','&','A','C','T','I','O','N','=','I','D','E','_','D','E','F','A',
        'U','L','T', 0 };
    static const WCHAR url2[] = { '.','.','/','R','i','t','z','.','x','m','l',0 };
    static const WCHAR url3[] = { 'h','t','t','p',':','/','/','x','.','o','r','g',0 };
    URL_COMPONENTSW comp;
    WCHAR scheme[20], host[20], user[20], pwd[20], urlpart[50], extra[50];
    DWORD error;
    BOOL r;

    urlpart[0]=0;
    scheme[0]=0;
    extra[0]=0;
    host[0]=0;
    user[0]=0;
    pwd[0]=0;
    memset(&comp, 0, sizeof comp);
    comp.dwStructSize = sizeof(comp);
    comp.lpszScheme = scheme;
    comp.dwSchemeLength = sizeof(scheme)/sizeof(scheme[0]);
    comp.lpszHostName = host;
    comp.dwHostNameLength = sizeof(host)/sizeof(host[0]);
    comp.lpszUserName = user;
    comp.dwUserNameLength = sizeof(user)/sizeof(user[0]);
    comp.lpszPassword = pwd;
    comp.dwPasswordLength = sizeof(pwd)/sizeof(pwd[0]);
    comp.lpszUrlPath = urlpart;
    comp.dwUrlPathLength = sizeof(urlpart)/sizeof(urlpart[0]);
    comp.lpszExtraInfo = extra;
    comp.dwExtraInfoLength = sizeof(extra)/sizeof(extra[0]);

    SetLastError(0xdeadbeef);
    r = InternetCrackUrlW(NULL, 0, 0, &comp );
    error = GetLastError();
    if (!r && error == ERROR_CALL_NOT_IMPLEMENTED)
    {
        win_skip("InternetCrackUrlW is not implemented\n");
        return;
    }
    ok( !r, "InternetCrackUrlW succeeded unexpectedly\n");
    ok( error == ERROR_INVALID_PARAMETER ||
        broken(error == ERROR_INTERNET_UNRECOGNIZED_SCHEME), /* IE5 */
        "expected ERROR_INVALID_PARAMETER got %u\n", error);

    if (error == ERROR_INVALID_PARAMETER)
    {
        /* Crashes on IE5 */
        SetLastError(0xdeadbeef);
        r = InternetCrackUrlW(url, 0, 0, NULL );
        error = GetLastError();
        ok( !r, "InternetCrackUrlW succeeded unexpectedly\n");
        ok( error == ERROR_INVALID_PARAMETER, "expected ERROR_INVALID_PARAMETER got %u\n", error);
    }

    r = InternetCrackUrlW(url, 0, 0, &comp );
    ok( r, "failed to crack url\n");
    ok( comp.dwSchemeLength == 4, "scheme length wrong\n");
    ok( comp.dwHostNameLength == 12, "host length wrong\n");
    ok( comp.dwUserNameLength == 0, "user length wrong\n");
    ok( comp.dwPasswordLength == 0, "password length wrong\n");
    ok( comp.dwUrlPathLength == 15, "url length wrong\n");
    ok( comp.dwExtraInfoLength == 29, "extra length wrong\n");
 
    urlpart[0]=0;
    scheme[0]=0;
    extra[0]=0;
    host[0]=0;
    user[0]=0;
    pwd[0]=0;
    memset(&comp, 0, sizeof comp);
    comp.dwStructSize = sizeof comp;
    comp.lpszHostName = host;
    comp.dwHostNameLength = sizeof(host)/sizeof(host[0]);
    comp.lpszUrlPath = urlpart;
    comp.dwUrlPathLength = sizeof(urlpart)/sizeof(urlpart[0]);

    r = InternetCrackUrlW(url, 0, 0, &comp );
    ok( r, "failed to crack url\n");
    ok( comp.dwSchemeLength == 0, "scheme length wrong\n");
    ok( comp.dwHostNameLength == 12, "host length wrong\n");
    ok( comp.dwUserNameLength == 0, "user length wrong\n");
    ok( comp.dwPasswordLength == 0, "password length wrong\n");
    ok( comp.dwUrlPathLength == 44, "url length wrong\n");
    ok( comp.dwExtraInfoLength == 0, "extra length wrong\n");

    urlpart[0]=0;
    scheme[0]=0;
    extra[0]=0;
    host[0]=0;
    user[0]=0;
    pwd[0]=0;
    memset(&comp, 0, sizeof comp);
    comp.dwStructSize = sizeof comp;
    comp.lpszHostName = host;
    comp.dwHostNameLength = sizeof(host)/sizeof(host[0]);
    comp.lpszUrlPath = urlpart;
    comp.dwUrlPathLength = sizeof(urlpart)/sizeof(urlpart[0]);
    comp.lpszExtraInfo = NULL;
    comp.dwExtraInfoLength = sizeof(extra)/sizeof(extra[0]);

    r = InternetCrackUrlW(url, 0, 0, &comp );
    ok( r, "failed to crack url\n");
    ok( comp.dwSchemeLength == 0, "scheme length wrong\n");
    ok( comp.dwHostNameLength == 12, "host length wrong\n");
    ok( comp.dwUserNameLength == 0, "user length wrong\n");
    ok( comp.dwPasswordLength == 0, "password length wrong\n");
    ok( comp.dwUrlPathLength == 15, "url length wrong\n");
    ok( comp.dwExtraInfoLength == 29, "extra length wrong\n");

    urlpart[0]=0;
    scheme[0]=0;
    extra[0]=0;
    host[0]=0;
    user[0]=0;
    pwd[0]=0;
    memset(&comp, 0, sizeof(comp));
    comp.dwStructSize = sizeof(comp);
    comp.lpszScheme = scheme;
    comp.dwSchemeLength = sizeof(scheme)/sizeof(scheme[0]);
    comp.lpszHostName = host;
    comp.dwHostNameLength = sizeof(host)/sizeof(host[0]);
    comp.lpszUserName = user;
    comp.dwUserNameLength = sizeof(user)/sizeof(user[0]);
    comp.lpszPassword = pwd;
    comp.dwPasswordLength = sizeof(pwd)/sizeof(pwd[0]);
    comp.lpszUrlPath = urlpart;
    comp.dwUrlPathLength = sizeof(urlpart)/sizeof(urlpart[0]);
    comp.lpszExtraInfo = extra;
    comp.dwExtraInfoLength = sizeof(extra)/sizeof(extra[0]);

    r = InternetCrackUrlW(url2, 0, 0, &comp);
    todo_wine {
    ok(!r, "InternetCrackUrl should have failed\n");
    ok(GetLastError() == ERROR_INTERNET_UNRECOGNIZED_SCHEME,
        "InternetCrackUrl should have failed with error ERROR_INTERNET_UNRECOGNIZED_SCHEME instead of error %d\n",
        GetLastError());
    }

    /* Test to see whether cracking a URL without a filename initializes urlpart */
    urlpart[0]=0xba;
    scheme[0]=0;
    extra[0]=0;
    host[0]=0;
    user[0]=0;
    pwd[0]=0;
    memset(&comp, 0, sizeof comp);
    comp.dwStructSize = sizeof comp;
    comp.lpszScheme = scheme;
    comp.dwSchemeLength = sizeof(scheme)/sizeof(scheme[0]);
    comp.lpszHostName = host;
    comp.dwHostNameLength = sizeof(host)/sizeof(host[0]);
    comp.lpszUserName = user;
    comp.dwUserNameLength = sizeof(user)/sizeof(user[0]);
    comp.lpszPassword = pwd;
    comp.dwPasswordLength = sizeof(pwd)/sizeof(pwd[0]);
    comp.lpszUrlPath = urlpart;
    comp.dwUrlPathLength = sizeof(urlpart)/sizeof(urlpart[0]);
    comp.lpszExtraInfo = extra;
    comp.dwExtraInfoLength = sizeof(extra)/sizeof(extra[0]);
    r = InternetCrackUrlW(url3, 0, 0, &comp );
    ok( r, "InternetCrackUrlW failed unexpectedly\n");
    ok( host[0] == 'x', "host should be x.org\n");
    ok( urlpart[0] == 0, "urlpart should be empty\n");
}

static void fill_url_components(LPURL_COMPONENTS lpUrlComponents)
{
    static CHAR http[]       = "http",
                winehq[]     = "www.winehq.org",
                username[]   = "username",
                password[]   = "password",
                site_about[] = "/site/about",
                empty[]      = "";

    lpUrlComponents->dwStructSize = sizeof(URL_COMPONENTS);
    lpUrlComponents->lpszScheme = http;
    lpUrlComponents->dwSchemeLength = strlen(lpUrlComponents->lpszScheme);
    lpUrlComponents->nScheme = INTERNET_SCHEME_HTTP;
    lpUrlComponents->lpszHostName = winehq;
    lpUrlComponents->dwHostNameLength = strlen(lpUrlComponents->lpszHostName);
    lpUrlComponents->nPort = 80;
    lpUrlComponents->lpszUserName = username;
    lpUrlComponents->dwUserNameLength = strlen(lpUrlComponents->lpszUserName);
    lpUrlComponents->lpszPassword = password;
    lpUrlComponents->dwPasswordLength = strlen(lpUrlComponents->lpszPassword);
    lpUrlComponents->lpszUrlPath = site_about;
    lpUrlComponents->dwUrlPathLength = strlen(lpUrlComponents->lpszUrlPath);
    lpUrlComponents->lpszExtraInfo = empty;
    lpUrlComponents->dwExtraInfoLength = strlen(lpUrlComponents->lpszExtraInfo);
}

static void InternetCreateUrlA_test(void)
{
	URL_COMPONENTS urlComp;
	LPSTR szUrl;
	DWORD len = -1;
	BOOL ret;
        static CHAR empty[]      = "",
                    nhttp[]      = "nhttp",
                    http[]       = "http",
                    https[]      = "https",
                    winehq[]     = "www.winehq.org",
                    localhost[]  = "localhost",
                    username[]   = "username",
                    password[]   = "password",
                    root[]       = "/",
                    site_about[] = "/site/about",
                    extra_info[] = "?test=123",
                    about[]      = "about",
                    blank[]      = "blank",
                    host[]       = "host";

	/* test NULL lpUrlComponents */
	SetLastError(0xdeadbeef);
	ret = InternetCreateUrlA(NULL, 0, NULL, &len);
	ok(!ret, "Expected failure\n");
	ok(GetLastError() == ERROR_INVALID_PARAMETER,
		"Expected ERROR_INVALID_PARAMETER, got %d\n", GetLastError());
	ok(len == -1, "Expected len -1, got %d\n", len);

	/* test zero'ed lpUrlComponents */
	ZeroMemory(&urlComp, sizeof(URL_COMPONENTS));
	SetLastError(0xdeadbeef);
	ret = InternetCreateUrlA(&urlComp, 0, NULL, &len);
	ok(!ret, "Expected failure\n");
	ok(GetLastError() == ERROR_INVALID_PARAMETER,
		"Expected ERROR_INVALID_PARAMETER, got %d\n", GetLastError());
	ok(len == -1, "Expected len -1, got %d\n", len);

	/* test valid lpUrlComponents, NULL lpdwUrlLength */
	fill_url_components(&urlComp);
	SetLastError(0xdeadbeef);
	ret = InternetCreateUrlA(&urlComp, 0, NULL, NULL);
	ok(!ret, "Expected failure\n");
	ok(GetLastError() == ERROR_INVALID_PARAMETER,
		"Expected ERROR_INVALID_PARAMETER, got %d\n", GetLastError());
	ok(len == -1, "Expected len -1, got %d\n", len);

	/* test valid lpUrlComponents, empty szUrl
	 * lpdwUrlLength is size of buffer required on exit, including
	 * the terminating null when GLE == ERROR_INSUFFICIENT_BUFFER
	 */
	SetLastError(0xdeadbeef);
	ret = InternetCreateUrlA(&urlComp, 0, NULL, &len);
	ok(!ret, "Expected failure\n");
	ok(GetLastError() == ERROR_INSUFFICIENT_BUFFER,
		"Expected ERROR_INSUFFICIENT_BUFFER, got %d\n", GetLastError());
	ok(len == 51, "Expected len 51, got %d\n", len);

	/* test correct size, NULL szUrl */
	fill_url_components(&urlComp);
	SetLastError(0xdeadbeef);
	ret = InternetCreateUrlA(&urlComp, 0, NULL, &len);
	ok(!ret, "Expected failure\n");
	ok(GetLastError() == ERROR_INSUFFICIENT_BUFFER,
		"Expected ERROR_INSUFFICIENT_BUFFER, got %d\n", GetLastError());
	ok(len == 51, "Expected len 51, got %d\n", len);

	/* test valid lpUrlComponents, alloc-ed szUrl, small size */
	SetLastError(0xdeadbeef);
	szUrl = HeapAlloc(GetProcessHeap(), 0, len);
	len -= 2;
	ret = InternetCreateUrlA(&urlComp, 0, szUrl, &len);
	ok(!ret, "Expected failure\n");
	ok(GetLastError() == ERROR_INSUFFICIENT_BUFFER,
		"Expected ERROR_INSUFFICIENT_BUFFER, got %d\n", GetLastError());
	ok(len == 51, "Expected len 51, got %d\n", len);

	/* alloc-ed szUrl, NULL lpszScheme
	 * shows that it uses nScheme instead
	 */
	SetLastError(0xdeadbeef);
	urlComp.lpszScheme = NULL;
	ret = InternetCreateUrlA(&urlComp, 0, szUrl, &len);
	ok(ret, "Expected success\n");
	ok(GetLastError() == 0xdeadbeef,
		"Expected 0xdeadbeef, got %d\n", GetLastError());
	ok(len == 50, "Expected len 50, got %d\n", len);
	ok(!strcmp(szUrl, CREATE_URL1), "Expected %s, got %s\n", CREATE_URL1, szUrl);

	/* alloc-ed szUrl, invalid nScheme
	 * any nScheme out of range seems ignored
	 */
	fill_url_components(&urlComp);
	SetLastError(0xdeadbeef);
	urlComp.nScheme = -3;
	len++;
	ret = InternetCreateUrlA(&urlComp, 0, szUrl, &len);
	ok(ret, "Expected success\n");
	ok(GetLastError() == 0xdeadbeef,
		"Expected 0xdeadbeef, got %d\n", GetLastError());
	ok(len == 50, "Expected len 50, got %d\n", len);

	/* test valid lpUrlComponents, alloc-ed szUrl */
	fill_url_components(&urlComp);
	SetLastError(0xdeadbeef);
	len = 51;
	ret = InternetCreateUrlA(&urlComp, 0, szUrl, &len);
	ok(ret, "Expected success\n");
	ok(GetLastError() == 0xdeadbeef,
		"Expected 0xdeadbeef, got %d\n", GetLastError());
	ok(len == 50, "Expected len 50, got %d\n", len);
	ok(strstr(szUrl, "80") == NULL, "Didn't expect to find 80 in szUrl\n");
	ok(!strcmp(szUrl, CREATE_URL1), "Expected %s, got %s\n", CREATE_URL1, szUrl);

	/* valid username, NULL password */
	fill_url_components(&urlComp);
	SetLastError(0xdeadbeef);
	urlComp.lpszPassword = NULL;
	len = 42;
	ret = InternetCreateUrlA(&urlComp, 0, szUrl, &len);
	ok(ret, "Expected success\n");
	ok(GetLastError() == 0xdeadbeef,
		"Expected 0xdeadbeef, got %d\n", GetLastError());
	ok(len == 41, "Expected len 41, got %d\n", len);
	ok(!strcmp(szUrl, CREATE_URL2), "Expected %s, got %s\n", CREATE_URL2, szUrl);

	/* valid username, empty password */
	fill_url_components(&urlComp);
	SetLastError(0xdeadbeef);
	urlComp.lpszPassword = empty;
	len = 51;
	ret = InternetCreateUrlA(&urlComp, 0, szUrl, &len);
	ok(ret, "Expected success\n");
	ok(GetLastError() == 0xdeadbeef,
		"Expected 0xdeadbeef, got %d\n", GetLastError());
	ok(len == 50, "Expected len 50, got %d\n", len);
	ok(!strcmp(szUrl, CREATE_URL3), "Expected %s, got %s\n", CREATE_URL3, szUrl);

	/* valid password, NULL username
	 * if password is provided, username has to exist
	 */
	fill_url_components(&urlComp);
	SetLastError(0xdeadbeef);
	urlComp.lpszUserName = NULL;
	len = 42;
	ret = InternetCreateUrlA(&urlComp, 0, szUrl, &len);
	ok(!ret, "Expected failure\n");
	ok(GetLastError() == ERROR_INVALID_PARAMETER,
		"Expected ERROR_INVALID_PARAMETER, got %d\n", GetLastError());
	ok(len == 42, "Expected len 42, got %d\n", len);
	ok(!strcmp(szUrl, CREATE_URL3), "Expected %s, got %s\n", CREATE_URL3, szUrl);

	/* valid password, empty username
	 * if password is provided, username has to exist
	 */
	fill_url_components(&urlComp);
	SetLastError(0xdeadbeef);
	urlComp.lpszUserName = empty;
	len = 51;
	ret = InternetCreateUrlA(&urlComp, 0, szUrl, &len);
	ok(ret, "Expected success\n");
	ok(GetLastError() == 0xdeadbeef,
		"Expected 0xdeadbeef, got %d\n", GetLastError());
	ok(len == 50, "Expected len 50, got %d\n", len);
	ok(!strcmp(szUrl, CREATE_URL5), "Expected %s, got %s\n", CREATE_URL5, szUrl);

	/* NULL username, NULL password */
	fill_url_components(&urlComp);
	SetLastError(0xdeadbeef);
	urlComp.lpszUserName = NULL;
	urlComp.lpszPassword = NULL;
	len = 42;
	ret = InternetCreateUrlA(&urlComp, 0, szUrl, &len);
	ok(ret, "Expected success\n");
	ok(GetLastError() == 0xdeadbeef,
		"Expected 0xdeadbeef, got %d\n", GetLastError());
	ok(len == 32, "Expected len 32, got %d\n", len);
	ok(!strcmp(szUrl, CREATE_URL4), "Expected %s, got %s\n", CREATE_URL4, szUrl);

	/* empty username, empty password */
	fill_url_components(&urlComp);
	SetLastError(0xdeadbeef);
	urlComp.lpszUserName = empty;
	urlComp.lpszPassword = empty;
	len = 51;
	ret = InternetCreateUrlA(&urlComp, 0, szUrl, &len);
	ok(ret, "Expected success\n");
	ok(GetLastError() == 0xdeadbeef,
		"Expected 0xdeadbeef, got %d\n", GetLastError());
	ok(len == 50, "Expected len 50, got %d\n", len);
	ok(!strcmp(szUrl, CREATE_URL5), "Expected %s, got %s\n", CREATE_URL5, szUrl);

	/* shows that nScheme is ignored, as the appearance of the port number
	 * depends on lpszScheme and the string copied depends on lpszScheme.
	 */
	fill_url_components(&urlComp);
	HeapFree(GetProcessHeap(), 0, szUrl);
	urlComp.lpszScheme = nhttp;
	urlComp.dwSchemeLength = strlen(urlComp.lpszScheme);
	len = strlen(CREATE_URL6) + 1;
	szUrl = HeapAlloc(GetProcessHeap(), 0, len);
	ret = InternetCreateUrlA(&urlComp, ICU_ESCAPE, szUrl, &len);
	ok(ret, "Expected success\n");
	ok(len == strlen(CREATE_URL6), "Expected len %d, got %d\n", lstrlenA(CREATE_URL6) + 1, len);
	ok(!strcmp(szUrl, CREATE_URL6), "Expected %s, got %s\n", CREATE_URL6, szUrl);

	/* if lpszScheme != "http" or nPort != 80, display nPort */
	HeapFree(GetProcessHeap(), 0, szUrl);
        urlComp.lpszScheme = http;
	urlComp.dwSchemeLength = strlen(urlComp.lpszScheme);
	urlComp.nPort = 42;
	szUrl = HeapAlloc(GetProcessHeap(), 0, ++len);
	ret = InternetCreateUrlA(&urlComp, ICU_ESCAPE, szUrl, &len);
	ok(ret, "Expected success\n");
	ok(len == 53, "Expected len 53, got %d\n", len);
	ok(strstr(szUrl, "42") != NULL, "Expected to find 42 in szUrl\n");
	ok(!strcmp(szUrl, CREATE_URL7), "Expected %s, got %s\n", CREATE_URL7, szUrl);

	HeapFree(GetProcessHeap(), 0, szUrl);

	memset(&urlComp, 0, sizeof(urlComp));
	urlComp.dwStructSize = sizeof(URL_COMPONENTS);
	urlComp.lpszScheme = http;
	urlComp.dwSchemeLength = 0;
	urlComp.nScheme = INTERNET_SCHEME_HTTP;
	urlComp.lpszHostName = winehq;
	urlComp.dwHostNameLength = 0;
	urlComp.nPort = 80;
	urlComp.lpszUserName = username;
	urlComp.dwUserNameLength = 0;
	urlComp.lpszPassword = password;
	urlComp.dwPasswordLength = 0;
	urlComp.lpszUrlPath = site_about;
	urlComp.dwUrlPathLength = 0;
	urlComp.lpszExtraInfo = empty;
	urlComp.dwExtraInfoLength = 0;
	len = strlen(CREATE_URL1);
	szUrl = HeapAlloc(GetProcessHeap(), 0, ++len);
	ret = InternetCreateUrlA(&urlComp, ICU_ESCAPE, szUrl, &len);
	ok(ret, "Expected success\n");
	ok(len == strlen(CREATE_URL1), "Expected len %d, got %d\n", lstrlenA(CREATE_URL1), len);
	ok(!strcmp(szUrl, CREATE_URL1), "Expected %s, got %s\n", CREATE_URL1, szUrl);

	HeapFree(GetProcessHeap(), 0, szUrl);

	memset(&urlComp, 0, sizeof(urlComp));
	urlComp.dwStructSize = sizeof(URL_COMPONENTS);
	urlComp.lpszScheme = https;
	urlComp.dwSchemeLength = 0;
	urlComp.nScheme = INTERNET_SCHEME_HTTP;
	urlComp.lpszHostName = winehq;
	urlComp.dwHostNameLength = 0;
	urlComp.nPort = 443;
	urlComp.lpszUserName = username;
	urlComp.dwUserNameLength = 0;
	urlComp.lpszPassword = password;
	urlComp.dwPasswordLength = 0;
	urlComp.lpszUrlPath = site_about;
	urlComp.dwUrlPathLength = 0;
	urlComp.lpszExtraInfo = empty;
	urlComp.dwExtraInfoLength = 0;
	len = strlen(CREATE_URL8);
	szUrl = HeapAlloc(GetProcessHeap(), 0, ++len);
	ret = InternetCreateUrlA(&urlComp, ICU_ESCAPE, szUrl, &len);
	ok(ret, "Expected success\n");
	ok(len == strlen(CREATE_URL8), "Expected len %d, got %d\n", lstrlenA(CREATE_URL8), len);
	ok(!strcmp(szUrl, CREATE_URL8), "Expected %s, got %s\n", CREATE_URL8, szUrl);

	HeapFree(GetProcessHeap(), 0, szUrl);

	memset(&urlComp, 0, sizeof(urlComp));
	urlComp.dwStructSize = sizeof(URL_COMPONENTS);
	urlComp.lpszScheme = about;
	urlComp.dwSchemeLength = 5;
	urlComp.lpszUrlPath = blank;
	urlComp.dwUrlPathLength = 5;
	len = strlen(CREATE_URL9);
	len++; /* work around bug in native wininet */
	szUrl = HeapAlloc(GetProcessHeap(), 0, ++len);
	ret = InternetCreateUrlA(&urlComp, ICU_ESCAPE, szUrl, &len);
	ok(ret, "Expected success\n");
	ok(len == strlen(CREATE_URL9), "Expected len %d, got %d\n", lstrlenA(CREATE_URL9), len);
	ok(!strcmp(szUrl, CREATE_URL9), "Expected %s, got %s\n", CREATE_URL9, szUrl);

	HeapFree(GetProcessHeap(), 0, szUrl);

	memset(&urlComp, 0, sizeof(urlComp));
	urlComp.dwStructSize = sizeof(URL_COMPONENTS);
	urlComp.lpszScheme = about;
	urlComp.lpszHostName = host;
	urlComp.lpszUrlPath = blank;
	len = strlen(CREATE_URL10);
	len++; /* work around bug in native wininet */
	szUrl = HeapAlloc(GetProcessHeap(), 0, ++len);
	ret = InternetCreateUrlA(&urlComp, ICU_ESCAPE, szUrl, &len);
	ok(ret, "Expected success\n");
	ok(len == strlen(CREATE_URL10), "Expected len %d, got %d\n", lstrlenA(CREATE_URL10), len);
	ok(!strcmp(szUrl, CREATE_URL10), "Expected %s, got %s\n", CREATE_URL10, szUrl);

	HeapFree(GetProcessHeap(), 0, szUrl);

	memset(&urlComp, 0, sizeof(urlComp));
	urlComp.dwStructSize = sizeof(URL_COMPONENTS);
	urlComp.nPort = 8080;
	urlComp.lpszScheme = about;
	len = strlen(CREATE_URL11);
	szUrl = HeapAlloc(GetProcessHeap(), 0, ++len);
	ret = InternetCreateUrlA(&urlComp, ICU_ESCAPE, szUrl, &len);
	ok(ret, "Expected success\n");
	ok(len == strlen(CREATE_URL11), "Expected len %d, got %d\n", lstrlenA(CREATE_URL11), len);
	ok(!strcmp(szUrl, CREATE_URL11), "Expected %s, got %s\n", CREATE_URL11, szUrl);

	HeapFree(GetProcessHeap(), 0, szUrl);

	memset(&urlComp, 0, sizeof(urlComp));
	urlComp.dwStructSize = sizeof(URL_COMPONENTS);
	urlComp.lpszScheme = http;
	urlComp.dwSchemeLength = 0;
	urlComp.nScheme = INTERNET_SCHEME_HTTP;
	urlComp.lpszHostName = winehq;
	urlComp.dwHostNameLength = 0;
	urlComp.nPort = 65535;
	len = strlen(CREATE_URL12);
	szUrl = HeapAlloc(GetProcessHeap(), 0, ++len);
	ret = InternetCreateUrlA(&urlComp, ICU_ESCAPE, szUrl, &len);
	ok(ret, "Expected success\n");
	ok(len == strlen(CREATE_URL12), "Expected len %d, got %d\n", lstrlenA(CREATE_URL12), len);
	ok(!strcmp(szUrl, CREATE_URL12), "Expected %s, got %s\n", CREATE_URL12, szUrl);

	HeapFree(GetProcessHeap(), 0, szUrl);

    memset(&urlComp, 0, sizeof(urlComp));
    urlComp.dwStructSize = sizeof(URL_COMPONENTS);
    urlComp.lpszScheme = http;
    urlComp.dwSchemeLength = strlen(urlComp.lpszScheme);
    urlComp.lpszHostName = localhost;
    urlComp.dwHostNameLength = strlen(urlComp.lpszHostName);
    urlComp.nPort = 80;
    urlComp.lpszUrlPath = root;
    urlComp.dwUrlPathLength = strlen(urlComp.lpszUrlPath);
    urlComp.lpszExtraInfo = extra_info;
    urlComp.dwExtraInfoLength = strlen(urlComp.lpszExtraInfo);
    len = 256;
    szUrl = HeapAlloc(GetProcessHeap(), 0, len);
    InternetCreateUrlA(&urlComp, ICU_ESCAPE, szUrl, &len);
    ok(ret, "Expected success\n");
    ok(len == strlen(CREATE_URL13), "Got len %u\n", len);
    ok(!strcmp(szUrl, CREATE_URL13), "Expected \"%s\", got \"%s\"\n", CREATE_URL13, szUrl);

    HeapFree(GetProcessHeap(), 0, szUrl);
}

START_TEST(url)
{
    int i;

    if(!GetProcAddress(GetModuleHandleA("wininet.dll"), "InternetGetCookieExW")) {
        win_skip("Too old IE (older than 6.0)\n");
        return;
    }

    for(i=0; i < sizeof(crack_url_tests)/sizeof(*crack_url_tests); i++)
        test_crack_url(crack_url_tests+i);

    InternetCrackUrl_test();
    InternetCrackUrlW_test();
    InternetCreateUrlA_test();
}
