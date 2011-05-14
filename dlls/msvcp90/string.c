/*
 * Copyright 2010 Piotr Caban for CodeWeavers
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

#include <stdarg.h>

#include "msvcp90.h"
#include "stdio.h"

#include "windef.h"
#include "winbase.h"
#include "wine/debug.h"
WINE_DEFAULT_DEBUG_CHANNEL(msvcp90);

/* char_traits<char> */
/* ?assign@?$char_traits@D@std@@SAXAADABD@Z */
/* ?assign@?$char_traits@D@std@@SAXAEADAEBD@Z */
void CDECL MSVCP_char_traits_char_assign(char *ch, const char *assign)
{
    *ch = *assign;
}

/* ?eq@?$char_traits@D@std@@SA_NABD0@Z */
/* ?eq@?$char_traits@D@std@@SA_NAEBD0@Z */
MSVCP_BOOL CDECL MSVCP_char_traits_char_eq(const char *ch1, const char *ch2)
{
    return *ch1 == *ch2;
}

/* ?lt@?$char_traits@D@std@@SA_NABD0@Z */
/* ?lt@?$char_traits@D@std@@SA_NAEBD0@Z */
MSVCP_BOOL CDECL MSVCP_char_traits_lt(const char *ch1, const char *ch2)
{
    return *ch1 < *ch2;
}

/* ?compare@?$char_traits@D@std@@SAHPBD0I@Z */
/* ?compare@?$char_traits@D@std@@SAHPEBD0_K@Z */
int CDECL MSVCP_char_traits_char_compare(
        const char *s1, const char *s2, size_t count)
{
    int ret = memcmp(s1, s2, count);
    return (ret>0 ? 1 : (ret<0 ? -1 : 0));
}

/* ?length@?$char_traits@D@std@@SAIPBD@Z */
/* ?length@?$char_traits@D@std@@SA_KPEBD@Z */
size_t CDECL MSVCP_char_traits_char_length(const char *str)
{
    return strlen(str);
}

/* ?_Copy_s@?$char_traits@D@std@@SAPADPADIPBDI@Z */
/* ?_Copy_s@?$char_traits@D@std@@SAPEADPEAD_KPEBD1@Z */
char* CDECL MSVCP_char_traits_char__Copy_s(char *dest,
        size_t size, const char *src, size_t count)
{
    if(!dest || !src || size<count) {
        if(dest && size)
            dest[0] = '\0';
        _invalid_parameter(NULL, NULL, NULL, 0, 0);
        return dest;
    }

    return memcpy(dest, src, count);
}

/* ?copy@?$char_traits@D@std@@SAPADPADPBDI@Z */
/* ?copy@?$char_traits@D@std@@SAPEADPEADPEBD_K@Z */
char* CDECL MSVCP_char_traits_char_copy(
        char *dest, const char *src, size_t count)
{
    return MSVCP_char_traits_char__Copy_s(dest, count, src, count);
}

/* ?find@?$char_traits@D@std@@SAPBDPBDIABD@Z */
/* ?find@?$char_traits@D@std@@SAPEBDPEBD_KAEBD@Z */
const char * CDECL MSVCP_char_traits_char_find(
        const char *str, size_t range, const char *c)
{
    return memchr(str, *c, range);
}

/* ?_Move_s@?$char_traits@D@std@@SAPADPADIPBDI@Z */
/* ?_Move_s@?$char_traits@D@std@@SAPEADPEAD_KPEBD1@Z */
char* CDECL MSVCP_char_traits_char__Move_s(char *dest,
        size_t size, const char *src, size_t count)
{
    if(!dest || !src || size<count) {
        if(dest && size)
            dest[0] = '\0';
        _invalid_parameter(NULL, NULL, NULL, 0, 0);
        return dest;
    }

    return memmove(dest, src, count);
}

/* ?move@?$char_traits@D@std@@SAPADPADPBDI@Z */
/* ?move@?$char_traits@D@std@@SAPEADPEADPEBD_K@Z */
char* CDECL MSVCP_char_traits_char_move(
        char *dest, const char *src, size_t count)
{
    return MSVCP_char_traits_char__Move_s(dest, count, src, count);
}

/* ?assign@?$char_traits@D@std@@SAPADPADID@Z */
/* ?assign@?$char_traits@D@std@@SAPEADPEAD_KD@Z */
char* CDECL MSVCP_char_traits_char_assignn(char *str, size_t num, char c)
{
    return memset(str, c, num);
}

/* ?to_char_type@?$char_traits@D@std@@SADABH@Z */
/* ?to_char_type@?$char_traits@D@std@@SADAEBH@Z */
char CDECL MSVCP_char_traits_char_to_char_type(const int *i)
{
    return (char)*i;
}

/* ?to_int_type@?$char_traits@D@std@@SAHABD@Z */
/* ?to_int_type@?$char_traits@D@std@@SAHAEBD@Z */
int CDECL MSVCP_char_traits_char_to_int_type(const char *ch)
{
    return (int)*ch;
}

/* ?eq_int_type@?$char_traits@D@std@@SA_NABH0@Z */
/* ?eq_int_type@?$char_traits@D@std@@SA_NAEBH0@Z */
MSVCP_BOOL CDECL MSVCP_char_traits_char_eq_int_type(const int *i1, const int *i2)
{
    return *i1 == *i2;
}

/* ?eof@?$char_traits@D@std@@SAHXZ */
int CDECL MSVCP_char_traits_char_eof(void)
{
    return EOF;
}

/* ?not_eof@?$char_traits@D@std@@SAHABH@Z */
/* ?not_eof@?$char_traits@D@std@@SAHAEBH@Z */
int CDECL MSVCP_char_traits_char_not_eof(int *in)
{
    return (*in==EOF ? !EOF : *in);
}


/* char_traits<wchar_t> */
/* ?assign@?$char_traits@_W@std@@SAXAA_WAB_W@Z */
/* ?assign@?$char_traits@_W@std@@SAXAEA_WAEB_W@Z */
void CDECL MSVCP_char_traits_wchar_assign(wchar_t *ch,
        const wchar_t *assign)
{
    *ch = *assign;
}

/* ?eq@?$char_traits@_W@std@@SA_NAB_W0@Z */
/* ?eq@?$char_traits@_W@std@@SA_NAEB_W0@Z */
MSVCP_BOOL CDECL MSVCP_char_traits_wchar_eq(wchar_t *ch1, wchar_t *ch2)
{
    return *ch1 == *ch2;
}

/* ?lt@?$char_traits@_W@std@@SA_NAB_W0@Z */
/* ?lt@?$char_traits@_W@std@@SA_NAEB_W0@Z */
MSVCP_BOOL CDECL MSVCP_char_traits_wchar_lt(const wchar_t *ch1,
        const wchar_t *ch2)
{
    return *ch1 < *ch2;
}

/* ?compare@?$char_traits@_W@std@@SAHPB_W0I@Z */
/* ?compare@?$char_traits@_W@std@@SAHPEB_W0_K@Z */
int CDECL MSVCP_char_traits_wchar_compare(const wchar_t *s1,
        const wchar_t *s2, size_t count)
{
    int ret = memcmp(s1, s2, sizeof(wchar_t[count]));
    return (ret>0 ? 1 : (ret<0 ? -1 : 0));
}

/* ?length@?$char_traits@_W@std@@SAIPB_W@Z */
/* ?length@?$char_traits@_W@std@@SA_KPEB_W@Z */
size_t CDECL MSVCP_char_traits_wchar_length(const wchar_t *str)
{
    return wcslen((WCHAR*)str);
}

/* ?_Copy_s@?$char_traits@_W@std@@SAPA_WPA_WIPB_WI@Z */
/* ?_Copy_s@?$char_traits@_W@std@@SAPEA_WPEA_W_KPEB_W1@Z */
wchar_t* CDECL MSVCP_char_traits_wchar__Copy_s(wchar_t *dest,
        size_t size, const wchar_t *src, size_t count)
{
    if(!dest || !src || size<count) {
        if(dest && size)
            dest[0] = '\0';
        _invalid_parameter(NULL, NULL, NULL, 0, 0);
        return dest;
    }

    return memcpy(dest, src, sizeof(wchar_t[count]));
}

/* ?copy@?$char_traits@_W@std@@SAPA_WPA_WPB_WI@Z */
/* ?copy@?$char_traits@_W@std@@SAPEA_WPEA_WPEB_W_K@Z */
wchar_t* CDECL MSVCP_char_traits_wchar_copy(wchar_t *dest,
        const wchar_t *src, size_t count)
{
    return MSVCP_char_traits_wchar__Copy_s(dest, count, src, count);
}

/* ?find@?$char_traits@_W@std@@SAPB_WPB_WIAB_W@Z */
/* ?find@?$char_traits@_W@std@@SAPEB_WPEB_W_KAEB_W@Z */
const wchar_t* CDECL MSVCP_char_traits_wchar_find(
        const wchar_t *str, size_t range, const wchar_t *c)
{
    size_t i=0;

    for(i=0; i<range; i++)
        if(str[i] == *c)
            return str+i;

    return NULL;
}

/* ?_Move_s@?$char_traits@_W@std@@SAPA_WPA_WIPB_WI@Z */
/* ?_Move_s@?$char_traits@_W@std@@SAPEA_WPEA_W_KPEB_W1@Z */
wchar_t* CDECL MSVCP_char_traits_wchar__Move_s(wchar_t *dest,
        size_t size, const wchar_t *src, size_t count)
{
    if(!dest || !src || size<count) {
        if(dest && size)
            dest[0] = '\0';
        _invalid_parameter(NULL, NULL, NULL, 0, 0);
        return dest;
    }

    return memmove(dest, src, sizeof(WCHAR[count]));
}

/* ?move@?$char_traits@_W@std@@SAPA_WPA_WPB_WI@Z */
/* ?move@?$char_traits@_W@std@@SAPEA_WPEA_WPEB_W_K@Z */
wchar_t* CDECL MSVCP_char_traits_wchar_move(wchar_t *dest,
        const wchar_t *src, size_t count)
{
    return MSVCP_char_traits_wchar__Move_s(dest, count, src, count);
}

/* ?assign@?$char_traits@_W@std@@SAPA_WPA_WI_W@Z */
/* ?assign@?$char_traits@_W@std@@SAPEA_WPEA_W_K_W@Z */
wchar_t* CDECL MSVCP_char_traits_wchar_assignn(wchar_t *str,
        size_t num, wchar_t c)
{
    size_t i;

    for(i=0; i<num; i++)
        str[i] = c;

    return str;
}

/* ?to_char_type@?$char_traits@_W@std@@SA_WABG@Z */
/* ?to_char_type@?$char_traits@_W@std@@SA_WAEBG@Z */
wchar_t CDECL MSVCP_char_traits_wchar_to_char_type(const unsigned short *i)
{
    return *i;
}

/* ?to_int_type@?$char_traits@_W@std@@SAGAB_W@Z */
/* ?to_int_type@?$char_traits@_W@std@@SAGAEB_W@Z */
unsigned short CDECL MSVCP_char_traits_wchar_to_int_type(const wchar_t *ch)
{
    return *ch;
}

/* ?eq_int_type@?$char_traits@_W@std@@SA_NABG0@Z */
/* ?eq_int_type@?$char_traits@_W@std@@SA_NAEBG0@Z */
MSVCP_BOOL CDECL MSVCP_char_traits_wchar_eq_int_tpe(const unsigned short *i1,
        const unsigned short *i2)
{
    return *i1 == *i2;
}

/* ?eof@?$char_traits@_W@std@@SAGXZ */
unsigned short CDECL MSVCP_char_traits_wchar_eof(void)
{
    return WEOF;
}

/* ?not_eof@?$char_traits@_W@std@@SAGABG@Z */
/* ?not_eof@?$char_traits@_W@std@@SAGAEBG@Z */
unsigned short CDECL MSVCP_char_traits_wchar_not_eof(const unsigned short *in)
{
    return (*in==WEOF ? !WEOF : *in);
}


/* char_traits<unsigned short> */
/* ?assign@?$char_traits@G@std@@SAXAAGABG@Z */
/* ?assign@?$char_traits@G@std@@SAXAEAGAEBG@Z */
void CDECL MSVCP_char_traits_short_assign(unsigned short *ch,
        const unsigned short *assign)
{
    *ch = *assign;
}

/* ?eq@?$char_traits@G@std@@SA_NABG0@Z */
/* ?eq@?$char_traits@G@std@@SA_NAEBG0@Z */
MSVCP_BOOL CDECL MSVCP_char_traits_short_eq(const unsigned short *ch1,
        const unsigned short *ch2)
{
    return *ch1 == *ch2;
}

/* ?lt@?$char_traits@G@std@@SA_NABG0@Z */
/* ?lt@?$char_traits@G@std@@SA_NAEBG0@Z */
MSVCP_BOOL CDECL MSVCP_char_traits_short_lt(const unsigned short *ch1,
        const unsigned short *ch2)
{
    return *ch1 < *ch2;
}

/* ?compare@?$char_traits@G@std@@SAHPBG0I@Z */
/* ?compare@?$char_traits@G@std@@SAHPEBG0_K@Z */
int CDECL MSVCP_char_traits_short_compare(const unsigned short *s1,
        const unsigned short *s2, size_t count)
{
    size_t i;

    for(i=0; i<count; i++)
        if(s1[i] != s2[i])
            return (s1[i] < s2[i] ? -1 : 1);

    return 0;
}

/* ?length@?$char_traits@G@std@@SAIPBG@Z */
/* ?length@?$char_traits@G@std@@SA_KPEBG@Z */
size_t CDECL MSVCP_char_traits_short_length(const unsigned short *str)
{
    size_t len;

    for(len=0; str[len]; len++);

    return len;
}

/* ?_Copy_s@?$char_traits@G@std@@SAPAGPAGIPBGI@Z */
/* ?_Copy_s@?$char_traits@G@std@@SAPEAGPEAG_KPEBG1@Z */
unsigned short * CDECL MSVCP_char_traits_short__Copy_s(unsigned short *dest,
        size_t size, const unsigned short *src, size_t count)
{
    if(size<count) {
        _invalid_parameter(NULL, NULL, NULL, 0, 0);
        return dest;
    }

    return memcpy(dest, src, sizeof(unsigned short[count]));
}

/* ?copy@?$char_traits@G@std@@SAPAGPAGPBGI@Z */
/* ?copy@?$char_traits@G@std@@SAPEAGPEAGPEBG_K@Z */
unsigned short* CDECL MSVCP_char_traits_short_copy(unsigned short *dest,
        const unsigned short *src, size_t count)
{
    return MSVCP_char_traits_short__Copy_s(dest, count, src, count);
}

/* ?find@?$char_traits@G@std@@SAPBGPBGIABG@Z */
/* ?find@?$char_traits@G@std@@SAPEBGPEBG_KAEBG@Z */
const unsigned short* CDECL MSVCP_char_traits_short_find(
        const unsigned short *str, size_t range, const unsigned short *c)
{
    size_t i;

    for(i=0; i<range; i++)
        if(str[i] == *c)
            return str+i;

    return NULL;
}

/* ?_Move_s@?$char_traits@G@std@@SAPAGPAGIPBGI@Z */
/* ?_Move_s@?$char_traits@G@std@@SAPEAGPEAG_KPEBG1@Z */
unsigned short* CDECL MSVCP_char_traits_short__Move_s(unsigned short *dest,
        size_t size, const unsigned short *src, size_t count)
{
    if(size<count) {
        _invalid_parameter(NULL, NULL, NULL, 0, 0);
        return dest;
    }

    return memmove(dest, src, sizeof(unsigned short[count]));
}

/* ?move@?$char_traits@G@std@@SAPAGPAGPBGI@Z */
/* ?move@?$char_traits@G@std@@SAPEAGPEAGPEBG_K@Z */
unsigned short* CDECL MSVCP_char_traits_short_move(unsigned short *dest,
        const unsigned short *src, size_t count)
{
    return MSVCP_char_traits_short__Move_s(dest, count, src, count);
}

/* ?assign@?$char_traits@G@std@@SAPAGPAGIG@Z */
/* ?assign@?$char_traits@G@std@@SAPEAGPEAG_KG@Z */
unsigned short* CDECL MSVCP_char_traits_short_assignn(unsigned short *str,
        size_t num, unsigned short c)
{
    size_t i;

    for(i=0; i<num; i++)
        str[i] = c;

    return str;
}

/* ?to_char_type@?$char_traits@G@std@@SAGABG@Z */
/* ?to_char_type@?$char_traits@G@std@@SAGAEBG@Z */
unsigned short CDECL MSVCP_char_traits_short_to_char_type(const unsigned short *i)
{
    return *i;
}

/* ?to_int_type@?$char_traits@G@std@@SAGABG@Z */
/* ?to_int_type@?$char_traits@G@std@@SAGAEBG@Z */
unsigned short CDECL MSVCP_char_traits_short_to_int_type(const unsigned short *ch)
{
    return *ch;
}

/* ?eq_int_type@?$char_traits@G@std@@SA_NABG0@Z */
/* ?eq_int_type@?$char_traits@G@std@@SA_NAEBG0@Z */
MSVCP_BOOL CDECL MSVCP_char_traits_short_eq_int_type(unsigned short *i1,
        unsigned short *i2)
{
    return *i1 == *i2;
}

/* ?eof@?$char_traits@G@std@@SAGXZ */
unsigned short CDECL MSVCP_char_traits_short_eof(void)
{
    return -1;
}

/* ?not_eof@?$char_traits@G@std@@SAGABG@Z */
/* ?not_eof@?$char_traits@G@std@@SAGAEBG@Z */
unsigned short CDECL MSVCP_char_traits_short_not_eof(const unsigned short *in)
{
    return (*in==(unsigned short)-1 ? 0 : *in);
}


/* _String_base */
/* ?_Xlen@_String_base@std@@SAXXZ */
void  CDECL MSVCP__String_base_Xlen(void)
{
    static const char msg[] = "string too long";

    TRACE("\n");
    throw_exception(EXCEPTION_LENGTH_ERROR, msg);
}

/* ?_Xran@_String_base@std@@SAXXZ */
void CDECL MSVCP__String_base_Xran(void)
{
    static const char msg[] = "invalid string position";

    TRACE("\n");
    throw_exception(EXCEPTION_OUT_OF_RANGE, msg);
}

/* ?_Xinvarg@_String_base@std@@SAXXZ */
void CDECL MSVCP__String_base_Xinvarg(void)
{
    static const char msg[] = "invalid string argument";

    TRACE("\n");
    throw_exception(EXCEPTION_INVALID_ARGUMENT, msg);
}


/* basic_string<char, char_traits<char>, allocator<char>> */
/* ?npos@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@2IB */
/* ?npos@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@2_KB */
const size_t MSVCP_basic_string_char_npos = -1;

/* Internal: basic_string_char_ptr - return pointer to stored string */
static char* basic_string_char_ptr(basic_string_char *this)
{
    if(this->res == BUF_SIZE_CHAR-1)
        return this->data.buf;
    return this->data.ptr;
}

/* Internal: basic_string_char_const_ptr - returns const pointer to stored string */
static const char* basic_string_char_const_ptr(const basic_string_char *this)
{
    if(this->res == BUF_SIZE_CHAR-1)
        return this->data.buf;
    return this->data.ptr;
}

/* Internal: basic_string_char_eos - sets string length, puts '\0' on the end */
static void basic_string_char_eos(basic_string_char *this, size_t len)
{
    static const char nullbyte = '\0';

    this->size = len;
    MSVCP_char_traits_char_assign(basic_string_char_ptr(this)+len, &nullbyte);
}

/* Internal: basic_string_char_inside - checks if given pointer points inside stored string */
static MSVCP_BOOL basic_string_char_inside(
        basic_string_char *this, const char *ptr)
{
    char *cstr = basic_string_char_ptr(this);

    return (ptr<cstr || ptr>=cstr+this->size) ? FALSE : TRUE;
}

/* Internal: basic_string_char_tidy - initialize basic_string buffer, deallocates data */
/* Caution: new_size have to be smaller than BUF_SIZE_CHAR */
static void basic_string_char_tidy(basic_string_char *this,
        MSVCP_BOOL built, size_t new_size)
{
    if(built && BUF_SIZE_CHAR<=this->res) {
        char *ptr = this->data.ptr;

        if(new_size > 0)
            MSVCP_char_traits_char__Copy_s(this->data.buf, BUF_SIZE_CHAR, ptr, new_size);
        MSVCP_allocator_char_deallocate(this->allocator, ptr, this->res+1);
    }

    this->res = BUF_SIZE_CHAR-1;
    basic_string_char_eos(this, new_size);
}

/* Internal: basic_string_char_grow - changes size of internal buffer */
static MSVCP_BOOL basic_string_char_grow(
        basic_string_char *this, size_t new_size, MSVCP_BOOL trim)
{
    if(this->res < new_size) {
        size_t new_res = new_size;
        char *ptr;

        new_res |= 0xf;

        if(new_res/3 < this->res/2)
            new_res = this->res + this->res/2;

        ptr = MSVCP_allocator_char_allocate(this->allocator, new_res);
        if(!ptr)
            ptr = MSVCP_allocator_char_allocate(this->allocator, new_size+1);
        else
            new_size = new_res;
        if(!ptr) {
            ERR("Out of memory\n");
            basic_string_char_tidy(this, TRUE, 0);
            return FALSE;
        }

        MSVCP_char_traits_char__Copy_s(ptr, new_size,
                basic_string_char_ptr(this), this->size);
        basic_string_char_tidy(this, TRUE, 0);
        this->data.ptr = ptr;
        this->res = new_size;
        basic_string_char_eos(this, this->size);
    } else if(trim && new_size < BUF_SIZE_CHAR)
        basic_string_char_tidy(this, TRUE,
                new_size<this->size ? new_size : this->size);
    else if(new_size == 0)
        basic_string_char_eos(this, 0);

    return (new_size>0);
}

/* ?erase@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAEAAV12@II@Z */
/* ?erase@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAAAEAV12@_K0@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_char_erase, 12)
basic_string_char* __thiscall MSVCP_basic_string_char_erase(
        basic_string_char *this, size_t pos, size_t len)
{
    TRACE("%p %lu %lu\n", this, (unsigned long)pos, (unsigned long)len);

    if(pos > this->size) {
        MSVCP__String_base_Xran();
        return NULL;
    }

    if(len > this->size-pos)
        len = this->size-pos;

    if(len) {
        MSVCP_char_traits_char__Move_s(basic_string_char_ptr(this)+pos,
                this->res-pos, basic_string_char_ptr(this)+pos+len,
                this->size-pos-len);
        basic_string_char_eos(this, this->size-len);
    }

    return this;
}

/* ?assign@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAEAAV12@ABV12@II@Z */
/* ?assign@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAAAEAV12@AEBV12@_K1@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_char_assign_substr, 16)
basic_string_char* __thiscall MSVCP_basic_string_char_assign_substr(
        basic_string_char *this, const basic_string_char *assign,
        size_t pos, size_t len)
{
    TRACE("%p %p %lu %lu\n", this, assign, (unsigned long)pos, (unsigned long)len);

    if(assign->size < pos) {
        MSVCP__String_base_Xran();
        return NULL;
    }

    if(len > assign->size-pos)
        len = assign->size-pos;

    if(this == assign) {
        MSVCP_basic_string_char_erase(this, pos+len, MSVCP_basic_string_char_npos);
        MSVCP_basic_string_char_erase(this, 0, pos);
    } else if(basic_string_char_grow(this, len, FALSE)) {
        MSVCP_char_traits_char__Copy_s(basic_string_char_ptr(this),
                this->res, basic_string_char_const_ptr(assign)+pos, len);
        basic_string_char_eos(this, len);
    }

    return this;
}

/* ?assign@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAEAAV12@ABV12@@Z */
/* ?assign@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAAAEAV12@AEBV12@@Z */
/* ??4?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAEAAV01@ABV01@@Z */
/* ??4?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAAAEAV01@AEBV01@@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_char_assign, 8)
basic_string_char* __thiscall MSVCP_basic_string_char_assign(
        basic_string_char *this, const basic_string_char *assign)
{
    return MSVCP_basic_string_char_assign_substr(this, assign,
            0, MSVCP_basic_string_char_npos);
}

/* ?assign@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAEAAV12@PBDI@Z */
/* ?assign@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAAAEAV12@PEBD_K@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_char_assign_cstr_len, 12)
basic_string_char* __thiscall MSVCP_basic_string_char_assign_cstr_len(
        basic_string_char *this, const char *str, size_t len)
{
    TRACE("%p %s %lu\n", this, debugstr_a(str), (unsigned long)len);

    if(basic_string_char_inside(this, str))
        return MSVCP_basic_string_char_assign_substr(this, this,
                str-basic_string_char_ptr(this), len);
    else if(basic_string_char_grow(this, len, FALSE)) {
        MSVCP_char_traits_char__Copy_s(basic_string_char_ptr(this),
                this->res, str, len);
        basic_string_char_eos(this, len);
    }

    return this;
}

/* ?assign@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAEAAV12@PBD@Z */
/* ?assign@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAAAEAV12@PEBD@Z */
/* ??4?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAEAAV01@PBD@Z */
/* ??4?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAAAEAV01@PEBD@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_char_assign_cstr, 8)
basic_string_char* __thiscall MSVCP_basic_string_char_assign_cstr(
        basic_string_char *this, const char *str)
{
    return MSVCP_basic_string_char_assign_cstr_len(this, str,
            MSVCP_char_traits_char_length(str));
}

/* ?c_str@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QBEPBDXZ */
/* ?c_str@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEBAPEBDXZ */
/* ?data@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QBEPBDXZ */
/* ?data@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEBAPEBDXZ */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_char_c_str, 4)
const char* __thiscall MSVCP_basic_string_char_c_str(basic_string_char *this)
{
    TRACE("%p\n", this);
    return basic_string_char_const_ptr(this);
}

/* ?capacity@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QBEIXZ */
/* ?capacity@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEBA_KXZ */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_char_capacity, 4)
size_t __thiscall MSVCP_basic_string_char_capacity(basic_string_char *this)
{
    TRACE("%p\n", this);
    return this->res;
}

/* ??0?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAE@XZ */
/* ??0?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAA@XZ */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_char_ctor, 4)
basic_string_char* __thiscall MSVCP_basic_string_char_ctor(basic_string_char *this)
{
    TRACE("%p\n", this);

    basic_string_char_tidy(this, FALSE, 0);
    return this;
}

/* ??0?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAE@ABV01@@Z */
/* ??0?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAA@AEBV01@@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_char_copy_ctor, 8)
basic_string_char* __thiscall MSVCP_basic_string_char_copy_ctor(
    basic_string_char *this, const basic_string_char *copy)
{
    TRACE("%p %p\n", this, copy);

    basic_string_char_tidy(this, FALSE, 0);
    MSVCP_basic_string_char_assign(this, copy);
    return this;
}

/* ??0?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAE@PBD@Z */
/* ??0?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAA@PEBD@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_char_ctor_cstr, 8)
basic_string_char* __thiscall MSVCP_basic_string_char_ctor_cstr(
        basic_string_char *this, const char *str)
{
    TRACE("%p %s\n", this, debugstr_a(str));

    basic_string_char_tidy(this, FALSE, 0);
    MSVCP_basic_string_char_assign_cstr(this, str);
    return this;
}

/* ??0?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAE@PBDI@Z */
/* ??0?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAA@PEBD_K@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_char_ctor_cstr_len, 12)
basic_string_char* __thiscall MSVCP_basic_string_char_ctor_cstr_len(
        basic_string_char *this, const char *str, size_t len)
{
    TRACE("%p %s %d\n", this, str, len);

    basic_string_char_tidy(this, FALSE, 0);
    MSVCP_basic_string_char_assign_cstr_len(this, str, len);
    return this;
}

/* ??0?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAE@ABV01@II@Z */
/* ??0?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAA@AEBV01@_K1@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_char_ctor_substr, 16)
basic_string_char* __thiscall MSVCP_basic_string_char_ctor_substr(
        basic_string_char *this, const basic_string_char *assign,
        size_t pos, size_t len)
{
    TRACE("%p %p %lu %lu\n", this, assign, (unsigned long)pos, (unsigned long)len);

    basic_string_char_tidy(this, FALSE, 0);
    MSVCP_basic_string_char_assign_substr(this, assign, pos, len);
    return this;
}

/* ??1?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAE@XZ */
/* ??1?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAA@XZ */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_char_dtor, 4)
void __thiscall MSVCP_basic_string_char_dtor(basic_string_char *this)
{
    TRACE("%p\n", this);
    basic_string_char_tidy(this, TRUE, 0);
}

/* ?size@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QBEIXZ */
/* ?size@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEBA_KXZ */
/* ?length@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QBEIXZ */
/* ?length@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEBA_KXZ */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_char_length, 4)
size_t __thiscall MSVCP_basic_string_char_length(basic_string_char *this)
{
    TRACE("%p\n", this);
    return this->size;
}

/* ?swap@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAEXAAV12@@Z */
/* ?swap@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAAXAEAV12@@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_char_swap, 8)
void __thiscall MSVCP_basic_string_char_swap(basic_string_char *this, basic_string_char *str)
{
    if(this != str) {
        char tmp[sizeof(this->data)];
        const size_t size = this->size;
        const size_t res = this->res;

        memcpy(tmp, this->data.buf, sizeof(this->data));
        memcpy(this->data.buf, str->data.buf, sizeof(this->data));
        memcpy(str->data.buf, tmp, sizeof(this->data));

        this->size = str->size;
        this->res = str->res;

        str->size = size;
        str->res = res;
    }
}

/* ?append@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAEAAV12@ABV12@II@Z */
/* ?append@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAAAEAV12@AEBV12@_K1@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_char_append_substr, 16)
basic_string_char* __thiscall MSVCP_basic_string_char_append_substr(basic_string_char *this,
        const basic_string_char *append, size_t offset, size_t count)
{
    TRACE("%p %p %lu %lu\n", this, append, (unsigned long)offset, (unsigned long)count);

    if(append->size < offset)
        MSVCP__String_base_Xran();

    if(count > append->size-offset)
        count = append->size-offset;

    if(MSVCP_basic_string_char_npos-this->size<=count || this->size+count<this->size)
        MSVCP__String_base_Xlen();

    if(basic_string_char_grow(this, this->size+count, FALSE)) {
        MSVCP_char_traits_char__Copy_s(basic_string_char_ptr(this)+this->size,
                this->res-this->size, basic_string_char_const_ptr(append)+offset, count);
        basic_string_char_eos(this, this->size+count);
    }

    return this;
}

/* ?append@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAEAAV12@ABV12@@Z */
/* ?append@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAAAEAV12@AEBV12@@Z */
/* ??Y?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAEAAV01@ABV01@@Z */
/* ??Y?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAAAEAV01@AEBV01@@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_char_append, 8)
basic_string_char* __thiscall MSVCP_basic_string_char_append(
        basic_string_char *this, const basic_string_char *append)
{
    return MSVCP_basic_string_char_append_substr(this, append,
            0, MSVCP_basic_string_char_npos);
}

/* ?append@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAEAAV12@PBDI@Z */
/* ?append@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAAAEAV12@PEBD_K@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_char_append_cstr_len, 12)
basic_string_char* __thiscall MSVCP_basic_string_char_append_cstr_len(
        basic_string_char *this, const char *append, size_t count)
{
    TRACE("%p %s %lu\n", this, append, (unsigned long)count);

    if(basic_string_char_inside(this, append))
        return MSVCP_basic_string_char_append_substr(this, this,
                append-basic_string_char_ptr(this), count);

    if(MSVCP_basic_string_char_npos-this->size<=count || this->size+count<this->size)
        MSVCP__String_base_Xlen();

    if(basic_string_char_grow(this, this->size+count, FALSE)) {
        MSVCP_char_traits_char__Copy_s(basic_string_char_ptr(this)+this->size,
                this->res-this->size, append, count);
        basic_string_char_eos(this, this->size+count);
    }

    return this;
}

/* ?append@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAEAAV12@PBD@Z */
/* ?append@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAAAEAV12@PEBD@Z */
/* ??Y?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAEAAV01@PBD@Z */
/* ??Y?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAAAEAV01@PEBD@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_char_append_cstr, 8)
basic_string_char* __thiscall MSVCP_basic_string_char_append_cstr(
        basic_string_char *this, const char *append)
{
    return MSVCP_basic_string_char_append_cstr_len(this, append,
            MSVCP_char_traits_char_length(append));
}

/* ?append@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAEAAV12@ID@Z */
/* ?append@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAAAEAV12@_KD@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_char_append_len_ch, 12)
basic_string_char* __thiscall MSVCP_basic_string_char_append_len_ch(
        basic_string_char *this, size_t count, char ch)
{
    TRACE("%p %lu %c\n", this, (unsigned long)count, ch);

    if(MSVCP_basic_string_char_npos-this->size <= count)
        MSVCP__String_base_Xlen();

    if(basic_string_char_grow(this, this->size+count, FALSE)) {
        MSVCP_char_traits_char_assignn(basic_string_char_ptr(this)+this->size, count, ch);
        basic_string_char_eos(this, this->size+count);
    }

    return this;
}

/* ??Y?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAEAAV01@D@Z */
/* ??Y?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAAAEAV01@D@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_char_append_ch, 8)
basic_string_char* __thiscall MSVCP_basic_string_char_append_ch(
        basic_string_char *this, char ch)
{
    return MSVCP_basic_string_char_append_len_ch(this, 1, ch);
}

/* ??$?HDU?$char_traits@D@std@@V?$allocator@D@1@@std@@YA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@0@ABV10@PBD@Z */
/* ??$?HDU?$char_traits@D@std@@V?$allocator@D@1@@std@@YA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@0@AEBV10@PEBD@Z */
basic_string_char* __cdecl MSVCP_basic_string_char_concatenate_bstr_cstr(basic_string_char *ret,
        const basic_string_char *left, const char *right)
{
    TRACE("%p %s\n", left, right);

    MSVCP_basic_string_char_copy_ctor(ret, left);
    MSVCP_basic_string_char_append_cstr(ret, right);
    return ret;
}

/* ??$?HDU?$char_traits@D@std@@V?$allocator@D@1@@std@@YA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@0@PBDABV10@@Z */
/* ??$?HDU?$char_traits@D@std@@V?$allocator@D@1@@std@@YA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@0@PEBDAEBV10@@Z */
basic_string_char* __cdecl MSVCP_basic_string_char_concatenate_cstr_bstr(basic_string_char *ret,
        const char *left, const basic_string_char *right)
{
    TRACE("%s %p\n", left, right);

    MSVCP_basic_string_char_ctor_cstr(ret, left);
    MSVCP_basic_string_char_append(ret, right);
    return ret;
}

/* ??$?HDU?$char_traits@D@std@@V?$allocator@D@1@@std@@YA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@0@ABV10@0@Z */
/* ??$?HDU?$char_traits@D@std@@V?$allocator@D@1@@std@@YA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@0@AEBV10@0@Z */
basic_string_char* __cdecl MSVCP_basic_string_char_concatenate(basic_string_char *ret,
        const basic_string_char *left, const basic_string_char *right)
{
    TRACE("%p %p\n", left, right);

    MSVCP_basic_string_char_copy_ctor(ret, left);
    MSVCP_basic_string_char_append(ret, right);
    return ret;
}

/* ?compare@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QBEHIIPBDI@Z */
/* ?compare@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEBAH_K0PEBD0@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_char_compare_substr_cstr_len, 20)
int __thiscall MSVCP_basic_string_char_compare_substr_cstr_len(
            const basic_string_char *this, size_t pos, size_t num,
            const char *str, size_t count)
{
    int ans;

    TRACE("%p %lu %lu %s %lu\n", this, (unsigned long)pos,
            (unsigned long)num, str, (unsigned long)count);

    if(this->size < pos)
        MSVCP__String_base_Xran();

    if(pos+num > this->size)
        num = this->size-pos;

    ans = MSVCP_char_traits_char_compare(basic_string_char_const_ptr(this)+pos,
            str, num>count ? count : num);
    if(ans)
        return ans;

    if(num > count)
        ans = 1;
    else if(num < count)
        ans = -1;
    return ans;
}

/* ?compare@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QBEHIIPBD@Z */
/* ?compare@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEBAH_K0PEBD@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_char_compare_substr_cstr, 16)
int __thiscall MSVCP_basic_string_char_compare_substr_cstr(const basic_string_char *this,
        size_t pos, size_t num, const char *str)
{
    return MSVCP_basic_string_char_compare_substr_cstr_len(this, pos, num,
            str, MSVCP_char_traits_char_length(str));
}

/* ?compare@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QBEHPBD@Z */
/* ?compare@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEBAHPEBD@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_char_compare_cstr, 8)
int __thiscall MSVCP_basic_string_char_compare_cstr(
        const basic_string_char *this, const char *str)
{
    return MSVCP_basic_string_char_compare_substr_cstr_len(this, 0, this->size,
            str, MSVCP_char_traits_char_length(str));
}

/* ?compare@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QBEHIIABV12@II@Z */
/* ?compare@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEBAH_K0AEBV12@00@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_char_compare_substr_substr, 24)
int __thiscall MSVCP_basic_string_char_compare_substr_substr(
        const basic_string_char *this, size_t pos, size_t num,
        const basic_string_char *compare, size_t off, size_t count)
{
    TRACE("%p %lu %lu %p %lu %lu\n", this, (unsigned long)pos, (unsigned long)num,
            compare, (unsigned long)off, (unsigned long)count);

    if(compare->size < off)
        MSVCP__String_base_Xran();

    if(off+count > compare->size)
        count = compare->size-off;

    return MSVCP_basic_string_char_compare_substr_cstr_len(this, pos, num,
            basic_string_char_const_ptr(compare)+off, count);
}

/* ?compare@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QBEHIIABV12@@Z */
/* ?compare@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEBAH_K0AEBV12@@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_char_compare_substr, 16)
int __thiscall MSVCP_basic_string_char_compare_substr(
        const basic_string_char *this, size_t pos, size_t num,
        const basic_string_char *compare)
{
    return MSVCP_basic_string_char_compare_substr_cstr_len(this, pos, num,
            basic_string_char_const_ptr(compare), compare->size);
}

/* ?compare@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QBEHABV12@@Z */
/* ?compare@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEBAHAEBV12@@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_char_compare, 8)
int __thiscall MSVCP_basic_string_char_compare(
        const basic_string_char *this, const basic_string_char *compare)
{
    return MSVCP_basic_string_char_compare_substr_cstr_len(this, 0, this->size,
            basic_string_char_const_ptr(compare), compare->size);
}

/* ??$?MDU?$char_traits@D@std@@V?$allocator@D@1@@std@@YA_NABV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@0@0@Z */
/* ??$?MDU?$char_traits@D@std@@V?$allocator@D@1@@std@@YA_NAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@0@0@Z */
MSVCP_BOOL __cdecl MSVCP_basic_string_char_lower(
        const basic_string_char *left, const basic_string_char *right)
{
    return MSVCP_basic_string_char_compare(left, right) < 0;
}

/* ??$?MDU?$char_traits@D@std@@V?$allocator@D@1@@std@@YA_NABV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@0@PBD@Z */
/* ??$?MDU?$char_traits@D@std@@V?$allocator@D@1@@std@@YA_NAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@0@PEBD@Z */
MSVCP_BOOL __cdecl MSVCP_basic_string_char_lower_bstr_cstr(
        const basic_string_char *left, const char *right)
{
    return MSVCP_basic_string_char_compare_cstr(left, right) < 0;
}

/* ??$?MDU?$char_traits@D@std@@V?$allocator@D@1@@std@@YA_NPBDABV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@0@@Z */
/* ??$?MDU?$char_traits@D@std@@V?$allocator@D@1@@std@@YA_NPEBDAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@0@@Z */
MSVCP_BOOL __cdecl MSVCP_basic_string_char_lower_cstr_bstr(
        const char *left, const basic_string_char *right)
{
    return MSVCP_basic_string_char_compare_cstr(right, left) > 0;
}

/* ?find@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QBEIPBDII@Z */
/* ?find@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEBA_KPEBD_K1@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_char_find_cstr_substr, 16)
size_t __thiscall MSVCP_basic_string_char_find_cstr_substr(
        const basic_string_char *this, const char *find, size_t pos, size_t len)
{
    const char *p, *end;

    TRACE("%p %s %lu %lu\n", this, find, (unsigned long)pos, (unsigned long)len);

    if(len==0 && pos<=this->size)
        return pos;

    end = basic_string_char_const_ptr(this)+this->size-len+1;
    for(p=basic_string_char_const_ptr(this)+pos; p<end; p++) {
        p = MSVCP_char_traits_char_find(p, end-p, find);
        if(!p)
            break;

        if(!MSVCP_char_traits_char_compare(p, find, len))
            return p-basic_string_char_const_ptr(this);
    }

    return MSVCP_basic_string_char_npos;
}

/* ?find@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QBEIDI@Z */
/* ?find@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEBA_KD_K@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_char_find_ch, 12)
size_t __thiscall MSVCP_basic_string_char_find_ch(
        const basic_string_char *this, char ch, size_t pos)
{
    return MSVCP_basic_string_char_find_cstr_substr(this, &ch, pos, 1);
}

/* ?at@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAEAADI@Z */
/* ?at@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAAAEAD_K@Z */
/* ??A?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAEAADI@Z */
/* ??A?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAAAEAD_K@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_char_at, 8)
char* __thiscall MSVCP_basic_string_char_at(
        basic_string_char *this, size_t pos)
{
    TRACE("%p %lu\n", this, (unsigned long)pos);

    if(this->size <= pos)
        MSVCP__String_base_Xran();

    return basic_string_char_ptr(this)+pos;
}

/* ?at@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QBEABDI@Z */
/* ?at@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEBAAEBD_K@Z */
/* ??A?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QBEABDI@Z */
/* ??A?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEBAAEBD_K@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_char_const_at, 8)
const char* __thiscall MSVCP_basic_string_char_const_at(
        const basic_string_char *this, size_t pos)
{
    TRACE("%p %lu\n", this, (unsigned long)pos);

    if(this->size <= pos)
        MSVCP__String_base_Xran();

    return basic_string_char_const_ptr(this)+pos;
}

/* ?resize@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAEXID@Z */
/* ?resize@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAAX_KD@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_char_resize_ch, 12)
void __thiscall MSVCP_basic_string_char_resize_ch(
        basic_string_char *this, size_t size, char ch)
{
    TRACE("%p %lu %c\n", this, (unsigned long)size, ch);

    if(size <= this->size)
        MSVCP_basic_string_char_erase(this, size, this->size);
    else
        MSVCP_basic_string_char_append_len_ch(this, size-this->size, ch);
}

/* ?resize@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAEXI@Z */
/* ?resize@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAAX_K@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_char_resize, 8)
void __thiscall MSVCP_basic_string_char_resize(
        basic_string_char *this, size_t size)
{
    MSVCP_basic_string_char_resize_ch(this, size, '\0');
}

/* ?clear@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAEXXZ */
/* ?clear@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAAXXZ */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_char_clear, 4)
void __thiscall MSVCP_basic_string_char_clear(basic_string_char *this)
{
    basic_string_char_eos(this, 0);
}


/* basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t>> */
/* ?npos@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@2IB */
/* ?npos@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@2_KB */
const size_t MSVCP_basic_string_wchar_npos = -1;

/* Internal: basic_string_wchar_ptr - return pointer to stored string */
static wchar_t* basic_string_wchar_ptr(basic_string_wchar *this)
{
    if(this->res == BUF_SIZE_WCHAR-1)
        return this->data.buf;
    return this->data.ptr;
}

/* Internal: basic_string_wchar_const_ptr - returns const pointer to stored string */
static const wchar_t* basic_string_wchar_const_ptr(const basic_string_wchar *this)
{
    if(this->res == BUF_SIZE_WCHAR-1)
        return this->data.buf;
    return this->data.ptr;
}

/* Internal: basic_string_wchar_eos - sets string length, puts '\0' on the end */
static void basic_string_wchar_eos(basic_string_wchar *this, size_t len)
{
    static const wchar_t nullbyte_w = '\0';

    this->size = len;
    MSVCP_char_traits_wchar_assign(basic_string_wchar_ptr(this)+len, &nullbyte_w);
}

/* Internal: basic_string_char_inside - checks if given pointer points inside stored string */
static MSVCP_BOOL basic_string_wchar_inside(
        basic_string_wchar *this, const wchar_t *ptr)
{
    wchar_t *cstr = basic_string_wchar_ptr(this);

    return (ptr<cstr || ptr>=cstr+this->size) ? FALSE : TRUE;
}

/* Internal: basic_string_char_tidy - initialize basic_string buffer, deallocates data */
/* Caution: new_size have to be smaller than BUF_SIZE_WCHAR */
static void basic_string_wchar_tidy(basic_string_wchar *this,
        MSVCP_BOOL built, size_t new_size)
{
    if(built && BUF_SIZE_WCHAR<=this->res) {
        wchar_t *ptr = this->data.ptr;

        if(new_size > 0)
            MSVCP_char_traits_wchar__Copy_s(this->data.buf, BUF_SIZE_WCHAR, ptr, new_size);
        MSVCP_allocator_wchar_deallocate(this->allocator, ptr, this->res+1);
    }

    this->res = BUF_SIZE_WCHAR-1;
    basic_string_wchar_eos(this, new_size);
}

/* Internal: basic_string_wchar_grow - changes size of internal buffer */
static MSVCP_BOOL basic_string_wchar_grow(
        basic_string_wchar *this, size_t new_size, MSVCP_BOOL trim)
{
    if(this->res < new_size) {
        size_t new_res = new_size;
        wchar_t *ptr;

        new_res |= 0xf;

        if(new_res/3 < this->res/2)
            new_res = this->res + this->res/2;

        ptr = MSVCP_allocator_wchar_allocate(this->allocator, new_res);
        if(!ptr)
            ptr = MSVCP_allocator_wchar_allocate(this->allocator, new_size+1);
        else
            new_size = new_res;
        if(!ptr) {
            ERR("Out of memory\n");
            basic_string_wchar_tidy(this, TRUE, 0);
            return FALSE;
        }

        MSVCP_char_traits_wchar__Copy_s(ptr, new_size,
                basic_string_wchar_ptr(this), this->size);
        basic_string_wchar_tidy(this, TRUE, 0);
        this->data.ptr = ptr;
        this->res = new_size;
        basic_string_wchar_eos(this, this->size);
    } else if(trim && new_size < BUF_SIZE_WCHAR)
        basic_string_wchar_tidy(this, TRUE,
                new_size<this->size ? new_size : this->size);
    else if(new_size == 0)
        basic_string_wchar_eos(this, 0);

    return (new_size>0);
}

/* ?erase@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAEAAV12@II@Z */
/* ?erase@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEAAAEAV12@_K0@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_wchar_erase, 12)
basic_string_wchar* __thiscall MSVCP_basic_string_wchar_erase(
            basic_string_wchar *this, size_t pos, size_t len)
{
    TRACE("%p %lu %lu\n", this, (unsigned long)pos, (unsigned long)len);

    if(pos > this->size) {
        MSVCP__String_base_Xran();
        return NULL;
    }

    if(len > this->size-pos)
        len = this->size-pos;

    if(len) {
        MSVCP_char_traits_wchar__Move_s(basic_string_wchar_ptr(this)+pos,
                this->res-pos, basic_string_wchar_ptr(this)+pos+len,
                this->size-pos-len);
        basic_string_wchar_eos(this, this->size-len);
    }

    return this;
}

/* ?assign@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAEAAV12@ABV12@II@Z */
/* ?assign@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEAAAEAV12@AEBV12@_K1@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_wchar_assign_substr, 16)
basic_string_wchar* __thiscall MSVCP_basic_string_wchar_assign_substr(
            basic_string_wchar *this, const basic_string_wchar *assign,
            size_t pos, size_t len)
{
    TRACE("%p %p %lu %lu\n", this, assign, (unsigned long)pos, (unsigned long)len);

    if(assign->size < pos) {
        MSVCP__String_base_Xran();
        return NULL;
    }

    if(len > assign->size-pos)
        len = assign->size-pos;

    if(this == assign) {
        MSVCP_basic_string_wchar_erase(this, pos+len, MSVCP_basic_string_wchar_npos);
        MSVCP_basic_string_wchar_erase(this, 0, pos);
    } else if(basic_string_wchar_grow(this, len, FALSE)) {
        MSVCP_char_traits_wchar__Copy_s(basic_string_wchar_ptr(this),
                this->res, basic_string_wchar_const_ptr(assign)+pos, len);
        basic_string_wchar_eos(this, len);
    }

    return this;
}

/* ?assign@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAEAAV12@ABV12@@Z */
/* ?assign@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEAAAEAV12@AEBV12@@Z */
/* ??4?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAEAAV01@ABV01@@Z */
/* ??4?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEAAAEAV01@AEBV01@@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_wchar_assign, 8)
basic_string_wchar* __thiscall MSVCP_basic_string_wchar_assign(
            basic_string_wchar *this, const basic_string_wchar *assign)
{
    return MSVCP_basic_string_wchar_assign_substr(this, assign,
            0, MSVCP_basic_string_wchar_npos);
}

/* ?assign@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAEAAV12@PB_WI@Z */
/* ?assign@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEAAAEAV12@PEB_W_K@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_wchar_assign_cstr_len, 12)
basic_string_wchar* __thiscall MSVCP_basic_string_wchar_assign_cstr_len(
            basic_string_wchar *this, const wchar_t *str, size_t len)
{
    TRACE("%p %s %lu\n", this, debugstr_w(str), (unsigned long)len);

    if(basic_string_wchar_inside(this, str))
        return MSVCP_basic_string_wchar_assign_substr(this, this,
                str-basic_string_wchar_ptr(this), len);
    else if(basic_string_wchar_grow(this, len, FALSE)) {
        MSVCP_char_traits_wchar__Copy_s(basic_string_wchar_ptr(this),
                this->res, str, len);
        basic_string_wchar_eos(this, len);
    }

    return this;
}

/* ?assign@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAEAAV12@PB_W@Z */
/* ?assign@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEAAAEAV12@PEB_W@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_wchar_assign_cstr, 8)
basic_string_wchar* __thiscall MSVCP_basic_string_wchar_assign_cstr(
            basic_string_wchar *this, const wchar_t *str)
{
    return MSVCP_basic_string_wchar_assign_cstr_len(this, str,
            MSVCP_char_traits_wchar_length(str));
}

/* ?c_str@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QBEPB_WXZ */
/* ?c_str@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEBAPEB_WXZ */
/* ?data@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QBEPB_WXZ */
/* ?data@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEBAPEB_WXZ */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_wchar_c_str, 4)
const wchar_t* __thiscall MSVCP_basic_string_wchar_c_str(basic_string_wchar *this)
{
    TRACE("%p\n", this);
    return basic_string_wchar_const_ptr(this);
}

/* ?capacity@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QBEIXZ */
/* ?capacity@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEBA_KXZ */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_wchar_capacity, 4)
size_t __thiscall MSVCP_basic_string_wchar_capacity(basic_string_wchar *this)
{
    TRACE("%p\n", this);
    return this->res;
}

/* ??0?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAE@XZ */
/* ??0?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEAA@XZ */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_wchar_ctor, 4)
basic_string_wchar* __thiscall MSVCP_basic_string_wchar_ctor(basic_string_wchar *this)
{
    TRACE("%p\n", this);

    basic_string_wchar_tidy(this, FALSE, 0);
    return this;
}

/* ??0?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAE@ABV01@@Z */
/* ??0?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEAA@AEBV01@@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_wchar_copy_ctor, 8)
basic_string_wchar* __thiscall MSVCP_basic_string_wchar_copy_ctor(
            basic_string_wchar *this, const basic_string_wchar *copy)
{
    TRACE("%p %p\n", this, copy);

    basic_string_wchar_tidy(this, FALSE, 0);
    MSVCP_basic_string_wchar_assign(this, copy);
    return this;
}

/* ??0?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAE@PB_W@Z */
/* ??0?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEAA@PEB_W@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_wchar_ctor_cstr, 8)
basic_string_wchar* __thiscall MSVCP_basic_string_wchar_ctor_cstr(
            basic_string_wchar *this, const wchar_t *str)
{
    TRACE("%p %s\n", this, debugstr_w(str));

    basic_string_wchar_tidy(this, FALSE, 0);
    MSVCP_basic_string_wchar_assign_cstr(this, str);
    return this;
}

/* ??0?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAE@PB_WI@Z */
/* ??0?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEAA@PEB_W_K@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_wchar_ctor_cstr_len, 12)
basic_string_wchar* __thiscall MSVCP_basic_string_wchar_ctor_cstr_len(
        basic_string_wchar *this, const wchar_t *str, size_t len)
{
    TRACE("%p %s %d\n", this, debugstr_w(str), len);

    basic_string_wchar_tidy(this, FALSE, 0);
    MSVCP_basic_string_wchar_assign_cstr_len(this, str, len);
    return this;
}

/* ??0?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAE@ABV01@II@Z */
/* ??0?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEAA@AEBV01@_K1@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_wchar_ctor_substr, 16)
basic_string_wchar* __thiscall MSVCP_basic_string_wchar_ctor_substr(
        basic_string_wchar *this, const basic_string_wchar *assign,
        size_t pos, size_t len)
{
    TRACE("%p %p %lu %lu\n", this, assign, (unsigned long)pos, (unsigned long)len);

    basic_string_wchar_tidy(this, FALSE, 0);
    MSVCP_basic_string_wchar_assign_substr(this, assign, pos, len);
    return this;
}

/* ??1?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAE@XZ */
/* ??1?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEAA@XZ */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_wchar_dtor, 4)
void __thiscall MSVCP_basic_string_wchar_dtor(basic_string_wchar *this)
{
    TRACE("%p\n", this);
    basic_string_wchar_tidy(this, TRUE, 0);
}

/* ?size@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QBEIXZ */
/* ?size@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEBA_KXZ */
/* ?length@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QBEIXZ */
/* ?length@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEBA_KXZ */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_wchar_length, 4)
size_t __thiscall MSVCP_basic_string_wchar_length(basic_string_wchar *this)
{
    TRACE("%p\n", this);
    return this->size;
}

/* ?swap@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAEXAAV12@@Z */
/* ?swap@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEAAXAEAV12@@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_wchar_swap, 8)
void __thiscall MSVCP_basic_string_wchar_swap(basic_string_wchar *this, basic_string_wchar *str)
{
    if(this != str) {
        char tmp[sizeof(this->data)];
        const size_t size = this->size;
        const size_t res = this->res;

        memcpy(tmp, this->data.buf, sizeof(this->data));
        memcpy(this->data.buf, str->data.buf, sizeof(this->data));
        memcpy(str->data.buf, tmp, sizeof(this->data));

        this->size = str->size;
        this->res = str->res;

        str->size = size;
        str->res = res;
    }
}

/* ?append@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAEAAV12@ABV12@II@Z */
/* ?append@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEAAAEAV12@AEBV12@_K1@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_wchar_append_substr, 16)
basic_string_wchar* __thiscall MSVCP_basic_string_wchar_append_substr(basic_string_wchar *this,
        const basic_string_wchar *append, size_t offset, size_t count)
{
    TRACE("%p %p %lu %lu\n", this, append, (unsigned long)offset, (unsigned long)count);

    if(append->size < offset)
        MSVCP__String_base_Xran();

    if(count > append->size-offset)
        count = append->size-offset;

    if(MSVCP_basic_string_wchar_npos-this->size<=count || this->size+count<this->size)
        MSVCP__String_base_Xlen();

    if(basic_string_wchar_grow(this, this->size+count, FALSE)) {
        MSVCP_char_traits_wchar__Copy_s(basic_string_wchar_ptr(this)+this->size,
                this->res-this->size, basic_string_wchar_const_ptr(append)+offset, count);
        basic_string_wchar_eos(this, this->size+count);
    }

    return this;
}

/* ?append@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAEAAV12@ABV12@@Z */
/* ?append@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEAAAEAV12@AEBV12@@Z */
/* ??Y?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAEAAV01@ABV01@@Z */
/* ??Y?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEAAAEAV01@AEBV01@@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_wchar_append, 8)
basic_string_wchar* __thiscall MSVCP_basic_string_wchar_append(
            basic_string_wchar *this, const basic_string_wchar *append)
{
    return MSVCP_basic_string_wchar_append_substr(this, append,
            0, MSVCP_basic_string_wchar_npos);
}

/* ?append@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAEAAV12@PB_WI@Z */
/* ?append@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEAAAEAV12@PEB_W_K@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_wchar_append_cstr_len, 12)
basic_string_wchar* __thiscall MSVCP_basic_string_wchar_append_cstr_len(
        basic_string_wchar *this, const wchar_t *append, size_t count)
{
    TRACE("%p %s %lu\n", this, debugstr_w(append), (unsigned long)count);

    if(basic_string_wchar_inside(this, append))
        return MSVCP_basic_string_wchar_append_substr(this, this,
                append-basic_string_wchar_ptr(this), count);

    if(MSVCP_basic_string_wchar_npos-this->size<=count || this->size+count<this->size)
        MSVCP__String_base_Xlen();

    if(basic_string_wchar_grow(this, this->size+count, FALSE)) {
        MSVCP_char_traits_wchar__Copy_s(basic_string_wchar_ptr(this)+this->size,
                this->res-this->size, append, count);
        basic_string_wchar_eos(this, this->size+count);
    }

    return this;
}

/* ?append@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAEAAV12@PB_W@Z */
/* ?append@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEAAAEAV12@PEB_W@Z */
/* ??Y?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAEAAV01@PB_W@Z */
/* ??Y?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEAAAEAV01@PEB_W@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_wchar_append_cstr, 8)
basic_string_wchar* __thiscall MSVCP_basic_string_wchar_append_cstr(
        basic_string_wchar *this, const wchar_t *append)
{
    return MSVCP_basic_string_wchar_append_cstr_len(this, append,
            MSVCP_char_traits_wchar_length(append));
}

/* ?append@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAEAAV12@I_W@Z */
/* ?append@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEAAAEAV12@_K_W@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_wchar_append_len_ch, 12)
basic_string_wchar* __thiscall MSVCP_basic_string_wchar_append_len_ch(
        basic_string_wchar *this, size_t count, wchar_t ch)
{
    TRACE("%p %lu %c\n", this, (unsigned long)count, ch);

    if(MSVCP_basic_string_wchar_npos-this->size <= count)
        MSVCP__String_base_Xlen();

    if(basic_string_wchar_grow(this, this->size+count, FALSE)) {
        MSVCP_char_traits_wchar_assignn(basic_string_wchar_ptr(this)+this->size, count, ch);
        basic_string_wchar_eos(this, this->size+count);
    }

    return this;
}

/* ??Y?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAEAAV01@_W@Z */
/* ??Y?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEAAAEAV01@_W@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_wchar_append_ch, 8)
basic_string_wchar* __thiscall MSVCP_basic_string_wchar_append_ch(
        basic_string_wchar *this, wchar_t ch)
{
    return MSVCP_basic_string_wchar_append_len_ch(this, 1, ch);
}

/* ??$?H_WU?$char_traits@_W@std@@V?$allocator@_W@1@@std@@YA?AV?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@0@ABV10@PB_W@Z */
/* ??$?H_WU?$char_traits@_W@std@@V?$allocator@_W@1@@std@@YA?AV?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@0@AEBV10@PEB_W@Z */
basic_string_wchar* __cdecl MSVCP_basic_string_wchar_concatenate_bstr_cstr(basic_string_wchar *ret,
        const basic_string_wchar *left, const wchar_t *right)
{
    TRACE("%p %s\n", left, debugstr_w(right));

    MSVCP_basic_string_wchar_copy_ctor(ret, left);
    MSVCP_basic_string_wchar_append_cstr(ret, right);
    return ret;
}

/* ??$?H_WU?$char_traits@_W@std@@V?$allocator@_W@1@@std@@YA?AV?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@0@PB_WABV10@@Z */
/* ??$?H_WU?$char_traits@_W@std@@V?$allocator@_W@1@@std@@YA?AV?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@0@PEB_WAEBV10@@Z */
basic_string_wchar* __cdecl MSVCP_basic_string_wchar_concatenate_cstr_bstr(basic_string_wchar *ret,
        const wchar_t *left, const basic_string_wchar *right)
{
    TRACE("%s %p\n", debugstr_w(left), right);

    MSVCP_basic_string_wchar_ctor_cstr(ret, left);
    MSVCP_basic_string_wchar_append(ret, right);
    return ret;
}

/* ??$?H_WU?$char_traits@_W@std@@V?$allocator@_W@1@@std@@YA?AV?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@0@ABV10@0@Z */
/* ??$?H_WU?$char_traits@_W@std@@V?$allocator@_W@1@@std@@YA?AV?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@0@AEBV10@0@Z */
basic_string_wchar* __cdecl MSVCP_basic_string_wchar_concatenate(basic_string_wchar *ret,
        const basic_string_wchar *left, const basic_string_wchar *right)
{
    TRACE("%p %p\n", left, right);

    MSVCP_basic_string_wchar_copy_ctor(ret, left);
    MSVCP_basic_string_wchar_append(ret, right);
    return ret;
}

/* ?compare@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QBEHIIPB_WI@Z */
/* ?compare@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEBAH_K0PEB_W0@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_wchar_compare_substr_cstr_len, 20)
int __thiscall MSVCP_basic_string_wchar_compare_substr_cstr_len(
        const basic_string_wchar *this, size_t pos, size_t num,
        const wchar_t *str, size_t count)
{
    int ans;

    TRACE("%p %lu %lu %s %lu\n", this, (unsigned long)pos,
            (unsigned long)num, debugstr_w(str), (unsigned long)count);

    if(this->size < pos)
        MSVCP__String_base_Xran();

    if(pos+num > this->size)
        num = this->size-pos;

    ans = MSVCP_char_traits_wchar_compare(basic_string_wchar_const_ptr(this)+pos,
            str, num>count ? count : num);
    if(ans)
        return ans;

    if(num > count)
        ans = 1;
    else if(num < count)
        ans = -1;
    return ans;
}

/* ?compare@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QBEHIIPB_W@Z */
/* ?compare@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEBAH_K0PEB_W@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_wchar_compare_substr_cstr, 16)
int __thiscall MSVCP_basic_string_wchar_compare_substr_cstr(const basic_string_wchar *this,
        size_t pos, size_t num, const wchar_t *str)
{
    return MSVCP_basic_string_wchar_compare_substr_cstr_len(this, pos, num,
            str, MSVCP_char_traits_wchar_length(str));
}

/* ?compare@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QBEHPB_W@Z */
/* ?compare@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEBAHPEB_W@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_wchar_compare_cstr, 8)
int __thiscall MSVCP_basic_string_wchar_compare_cstr(
        const basic_string_wchar *this, const wchar_t *str)
{
    return MSVCP_basic_string_wchar_compare_substr_cstr_len(this, 0, this->size,
            str, MSVCP_char_traits_wchar_length(str));
}

/* ?compare@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QBEHIIABV12@II@Z */
/* ?compare@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEBAH_K0AEBV12@00@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_wchar_compare_substr_substr, 24)
int __thiscall MSVCP_basic_string_wchar_compare_substr_substr(
        const basic_string_wchar *this, size_t pos, size_t num,
        const basic_string_wchar *compare, size_t off, size_t count)
{
    TRACE("%p %lu %lu %p %lu %lu\n", this, (unsigned long)pos, (unsigned long)num,
            compare, (unsigned long)off, (unsigned long)count);

    if(compare->size < off)
        MSVCP__String_base_Xran();

    if(off+count > compare->size)
        count = compare->size-off;

    return MSVCP_basic_string_wchar_compare_substr_cstr_len(this, pos, num,
            basic_string_wchar_const_ptr(compare)+off, count);
}

/* ?compare@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QBEHIIABV12@@Z */
/* ?compare@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEBAH_K0AEBV12@@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_wchar_compare_substr, 16)
int __thiscall MSVCP_basic_string_wchar_compare_substr(
        const basic_string_wchar *this, size_t pos, size_t num,
        const basic_string_wchar *compare)
{
    return MSVCP_basic_string_wchar_compare_substr_cstr_len(this, pos, num,
            basic_string_wchar_const_ptr(compare), compare->size);
}

/* ?compare@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QBEHABV12@@Z */
/* ?compare@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEBAHAEBV12@@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_wchar_compare, 8)
int __thiscall MSVCP_basic_string_wchar_compare(
        const basic_string_wchar *this, const basic_string_wchar *compare)
{
    return MSVCP_basic_string_wchar_compare_substr_cstr_len(this, 0, this->size,
            basic_string_wchar_const_ptr(compare), compare->size);
}

/* ??$?M_WU?$char_traits@_W@std@@V?$allocator@_W@1@@std@@YA_NABV?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@0@0@Z */
/* ??$?M_WU?$char_traits@_W@std@@V?$allocator@_W@1@@std@@YA_NAEBV?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@0@0@Z */
MSVCP_BOOL __cdecl MSVCP_basic_string_wchar_lower(
        const basic_string_wchar *left, const basic_string_wchar *right)
{
    return MSVCP_basic_string_wchar_compare(left, right) < 0;
}

/* ??$?M_WU?$char_traits@_W@std@@V?$allocator@_W@1@@std@@YA_NABV?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@0@PB_W@Z */
/* ??$?M_WU?$char_traits@_W@std@@V?$allocator@_W@1@@std@@YA_NAEBV?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@0@PEB_W@Z */
MSVCP_BOOL __cdecl MSVCP_basic_string_wchar_lower_bstr_cstr(
        const basic_string_wchar *left, const wchar_t *right)
{
    return MSVCP_basic_string_wchar_compare_cstr(left, right) < 0;
}

/* ??$?M_WU?$char_traits@_W@std@@V?$allocator@_W@1@@std@@YA_NPB_WABV?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@0@@Z */
/* ??$?M_WU?$char_traits@_W@std@@V?$allocator@_W@1@@std@@YA_NPEB_WAEBV?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@0@@Z */
MSVCP_BOOL __cdecl MSVCP_basic_string_wchar_lower_cstr_bstr(
        const wchar_t *left, const basic_string_wchar *right)
{
    return MSVCP_basic_string_wchar_compare_cstr(right, left) > 0;
}

/* ?find@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QBEIPB_WII@Z */
/* ?find@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEBA_KPEB_W_K1@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_wchar_find_cstr_substr, 16)
size_t __thiscall MSVCP_basic_string_wchar_find_cstr_substr(
        const basic_string_wchar *this, const wchar_t *find, size_t pos, size_t len)
{
    const wchar_t *p, *end;

    TRACE("%p %s %lu %lu\n", this, debugstr_w(find), (unsigned long)pos, (unsigned long)len);

    if(len==0 && pos<=this->size)
        return pos;

    end = basic_string_wchar_const_ptr(this)+this->size-len+1;
    for(p=basic_string_wchar_const_ptr(this)+pos; p<end; p++) {
        p = MSVCP_char_traits_wchar_find(p, end-p, find);
        if(!p)
            break;

        if(!MSVCP_char_traits_wchar_compare(p, find, len))
            return p-basic_string_wchar_const_ptr(this);
    }

    return MSVCP_basic_string_wchar_npos;
}

/* ?find@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QBEI_WI@Z */
/* ?find@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEBA_K_W_K@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_wchar_find_ch, 12)
size_t __thiscall MSVCP_basic_string_wchar_find_ch(
        const basic_string_wchar *this, wchar_t ch, size_t pos)
{
    return MSVCP_basic_string_wchar_find_cstr_substr(this, &ch, pos, 1);
}

/* ?at@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAEAA_WI@Z */
/* ?at@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEAAAEA_W_K@Z */
/* ??A?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAEAA_WI@Z */
/* ??A?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEAAAEA_W_K@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_wchar_at, 8)
wchar_t* __thiscall MSVCP_basic_string_wchar_at(
        basic_string_wchar *this, size_t pos)
{
    TRACE("%p %lu\n", this, (unsigned long)pos);

    if(this->size <= pos)
        MSVCP__String_base_Xran();

    return basic_string_wchar_ptr(this)+pos;
}

/* ?at@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QBEAB_WI@Z */
/* ?at@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEBAAEB_W_K@Z */
/* ??A?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QBEAB_WI@Z */
/* ??A?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEBAAEB_W_K@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_wchar_const_at, 8)
const wchar_t* __thiscall MSVCP_basic_string_wchar_const_at(
        const basic_string_wchar *this, size_t pos)
{
    TRACE("%p %lu\n", this, (unsigned long)pos);

    if(this->size <= pos)
        MSVCP__String_base_Xran();

    return basic_string_wchar_const_ptr(this)+pos;
}

/* ?resize@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAEXI_W@Z */
/* ?resize@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEAAX_K_W@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_wchar_resize_ch, 12)
void __thiscall MSVCP_basic_string_wchar_resize_ch(
        basic_string_wchar *this, size_t size, wchar_t ch)
{
    TRACE("%p %lu %c\n", this, (unsigned long)size, ch);

    if(size <= this->size)
        MSVCP_basic_string_wchar_erase(this, size, this->size);
    else
        MSVCP_basic_string_wchar_append_len_ch(this, size-this->size, ch);
}

/* ?resize@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAEXI@Z */
/* ?resize@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEAAX_K@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_wchar_resize, 8)
void __thiscall MSVCP_basic_string_wchar_resize(
        basic_string_wchar *this, size_t size)
{
    MSVCP_basic_string_wchar_resize_ch(this, size, '\0');
}

/* ?clear@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAEXXZ */
/* ?clear@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEAAXXZ */
DEFINE_THISCALL_WRAPPER(MSVCP_basic_string_wchar_clear, 4)
void __thiscall MSVCP_basic_string_wchar_clear(basic_string_wchar *this)
{
    basic_string_wchar_eos(this, 0);
}
