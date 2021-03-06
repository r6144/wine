/*
 * Copyright 2010 Jacek Caban for CodeWeavers
 * Copyright 2010 Thomas Mullaly
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

#include "urlmon_main.h"
#include "wine/debug.h"

#define NO_SHLWAPI_REG
#include "shlwapi.h"

#define UINT_MAX 0xffffffff
#define USHORT_MAX 0xffff

#define URI_DISPLAY_NO_ABSOLUTE_URI         0x1
#define URI_DISPLAY_NO_DEFAULT_PORT_AUTH    0x2

#define ALLOW_NULL_TERM_SCHEME          0x01
#define ALLOW_NULL_TERM_USER_NAME       0x02
#define ALLOW_NULL_TERM_PASSWORD        0x04
#define ALLOW_BRACKETLESS_IP_LITERAL    0x08
#define SKIP_IP_FUTURE_CHECK            0x10
#define IGNORE_PORT_DELIMITER           0x20

#define RAW_URI_FORCE_PORT_DISP     0x1
#define RAW_URI_CONVERT_TO_DOS_PATH 0x2

WINE_DEFAULT_DEBUG_CHANNEL(urlmon);

static const IID IID_IUriObj = {0x4b364760,0x9f51,0x11df,{0x98,0x1c,0x08,0x00,0x20,0x0c,0x9a,0x66}};

typedef struct {
    const IUriVtbl  *lpIUriVtbl;
    LONG ref;

    BSTR            raw_uri;

    /* Information about the canonicalized URI's buffer. */
    WCHAR           *canon_uri;
    DWORD           canon_size;
    DWORD           canon_len;
    BOOL            display_modifiers;
    DWORD           create_flags;

    INT             scheme_start;
    DWORD           scheme_len;
    URL_SCHEME      scheme_type;

    INT             userinfo_start;
    DWORD           userinfo_len;
    INT             userinfo_split;

    INT             host_start;
    DWORD           host_len;
    Uri_HOST_TYPE   host_type;

    INT             port_offset;
    DWORD           port;
    BOOL            has_port;

    INT             authority_start;
    DWORD           authority_len;

    INT             domain_offset;

    INT             path_start;
    DWORD           path_len;
    INT             extension_offset;

    INT             query_start;
    DWORD           query_len;

    INT             fragment_start;
    DWORD           fragment_len;
} Uri;

typedef struct {
    const IUriBuilderVtbl  *lpIUriBuilderVtbl;
    LONG ref;

    Uri *uri;
    DWORD modified_props;

    WCHAR   *fragment;
    DWORD   fragment_len;

    WCHAR   *host;
    DWORD   host_len;

    WCHAR   *password;
    DWORD   password_len;

    WCHAR   *path;
    DWORD   path_len;

    BOOL    has_port;
    DWORD   port;

    WCHAR   *query;
    DWORD   query_len;

    WCHAR   *scheme;
    DWORD   scheme_len;

    WCHAR   *username;
    DWORD   username_len;
} UriBuilder;

typedef struct {
    const WCHAR *str;
    DWORD       len;
} h16;

typedef struct {
    /* IPv6 addresses can hold up to 8 h16 components. */
    h16         components[8];
    DWORD       h16_count;

    /* An IPv6 can have 1 elision ("::"). */
    const WCHAR *elision;

    /* An IPv6 can contain 1 IPv4 address as the last 32bits of the address. */
    const WCHAR *ipv4;
    DWORD       ipv4_len;

    INT         components_size;
    INT         elision_size;
} ipv6_address;

typedef struct {
    BSTR            uri;

    BOOL            is_relative;
    BOOL            is_opaque;
    BOOL            has_implicit_scheme;
    BOOL            has_implicit_ip;
    UINT            implicit_ipv4;

    const WCHAR     *scheme;
    DWORD           scheme_len;
    URL_SCHEME      scheme_type;

    const WCHAR     *username;
    DWORD           username_len;

    const WCHAR     *password;
    DWORD           password_len;

    const WCHAR     *host;
    DWORD           host_len;
    Uri_HOST_TYPE   host_type;

    BOOL            has_ipv6;
    ipv6_address    ipv6_address;

    BOOL            has_port;
    const WCHAR     *port;
    DWORD           port_len;
    DWORD           port_value;

    const WCHAR     *path;
    DWORD           path_len;

    const WCHAR     *query;
    DWORD           query_len;

    const WCHAR     *fragment;
    DWORD           fragment_len;
} parse_data;

static const CHAR hexDigits[] = "0123456789ABCDEF";

/* List of scheme types/scheme names that are recognized by the IUri interface as of IE 7. */
static const struct {
    URL_SCHEME  scheme;
    WCHAR       scheme_name[16];
} recognized_schemes[] = {
    {URL_SCHEME_FTP,            {'f','t','p',0}},
    {URL_SCHEME_HTTP,           {'h','t','t','p',0}},
    {URL_SCHEME_GOPHER,         {'g','o','p','h','e','r',0}},
    {URL_SCHEME_MAILTO,         {'m','a','i','l','t','o',0}},
    {URL_SCHEME_NEWS,           {'n','e','w','s',0}},
    {URL_SCHEME_NNTP,           {'n','n','t','p',0}},
    {URL_SCHEME_TELNET,         {'t','e','l','n','e','t',0}},
    {URL_SCHEME_WAIS,           {'w','a','i','s',0}},
    {URL_SCHEME_FILE,           {'f','i','l','e',0}},
    {URL_SCHEME_MK,             {'m','k',0}},
    {URL_SCHEME_HTTPS,          {'h','t','t','p','s',0}},
    {URL_SCHEME_SHELL,          {'s','h','e','l','l',0}},
    {URL_SCHEME_SNEWS,          {'s','n','e','w','s',0}},
    {URL_SCHEME_LOCAL,          {'l','o','c','a','l',0}},
    {URL_SCHEME_JAVASCRIPT,     {'j','a','v','a','s','c','r','i','p','t',0}},
    {URL_SCHEME_VBSCRIPT,       {'v','b','s','c','r','i','p','t',0}},
    {URL_SCHEME_ABOUT,          {'a','b','o','u','t',0}},
    {URL_SCHEME_RES,            {'r','e','s',0}},
    {URL_SCHEME_MSSHELLROOTED,  {'m','s','-','s','h','e','l','l','-','r','o','o','t','e','d',0}},
    {URL_SCHEME_MSSHELLIDLIST,  {'m','s','-','s','h','e','l','l','-','i','d','l','i','s','t',0}},
    {URL_SCHEME_MSHELP,         {'h','c','p',0}},
    {URL_SCHEME_WILDCARD,       {'*',0}}
};

/* List of default ports Windows recognizes. */
static const struct {
    URL_SCHEME  scheme;
    USHORT      port;
} default_ports[] = {
    {URL_SCHEME_FTP,    21},
    {URL_SCHEME_HTTP,   80},
    {URL_SCHEME_GOPHER, 70},
    {URL_SCHEME_NNTP,   119},
    {URL_SCHEME_TELNET, 23},
    {URL_SCHEME_WAIS,   210},
    {URL_SCHEME_HTTPS,  443},
};

/* List of 3 character top level domain names Windows seems to recognize.
 * There might be more, but, these are the only ones I've found so far.
 */
static const struct {
    WCHAR tld_name[4];
} recognized_tlds[] = {
    {{'c','o','m',0}},
    {{'e','d','u',0}},
    {{'g','o','v',0}},
    {{'i','n','t',0}},
    {{'m','i','l',0}},
    {{'n','e','t',0}},
    {{'o','r','g',0}}
};

static Uri *get_uri_obj(IUri *uri)
{
    Uri *ret;
    HRESULT hres;

    hres = IUri_QueryInterface(uri, &IID_IUriObj, (void**)&ret);
    return SUCCEEDED(hres) ? ret : NULL;
}

static inline BOOL is_alpha(WCHAR val) {
	return ((val >= 'a' && val <= 'z') || (val >= 'A' && val <= 'Z'));
}

static inline BOOL is_num(WCHAR val) {
	return (val >= '0' && val <= '9');
}

static inline BOOL is_drive_path(const WCHAR *str) {
    return (is_alpha(str[0]) && (str[1] == ':' || str[1] == '|'));
}

static inline BOOL is_unc_path(const WCHAR *str) {
    return (str[0] == '\\' && str[0] == '\\');
}

static inline BOOL is_forbidden_dos_path_char(WCHAR val) {
    return (val == '>' || val == '<' || val == '\"');
}

/* A URI is implicitly a file path if it begins with
 * a drive letter (eg X:) or starts with "\\" (UNC path).
 */
static inline BOOL is_implicit_file_path(const WCHAR *str) {
    return (is_unc_path(str) || (is_alpha(str[0]) && str[1] == ':'));
}

/* Checks if the URI is a hierarchical URI. A hierarchical
 * URI is one that has "//" after the scheme.
 */
static BOOL check_hierarchical(const WCHAR **ptr) {
    const WCHAR *start = *ptr;

    if(**ptr != '/')
        return FALSE;

    ++(*ptr);
    if(**ptr != '/') {
        *ptr = start;
        return FALSE;
    }

    ++(*ptr);
    return TRUE;
}

/* unreserved    = ALPHA / DIGIT / "-" / "." / "_" / "~" */
static inline BOOL is_unreserved(WCHAR val) {
    return (is_alpha(val) || is_num(val) || val == '-' || val == '.' ||
            val == '_' || val == '~');
}

/* sub-delims    = "!" / "$" / "&" / "'" / "(" / ")"
 *               / "*" / "+" / "," / ";" / "="
 */
static inline BOOL is_subdelim(WCHAR val) {
    return (val == '!' || val == '$' || val == '&' ||
            val == '\'' || val == '(' || val == ')' ||
            val == '*' || val == '+' || val == ',' ||
            val == ';' || val == '=');
}

/* gen-delims  = ":" / "/" / "?" / "#" / "[" / "]" / "@" */
static inline BOOL is_gendelim(WCHAR val) {
    return (val == ':' || val == '/' || val == '?' ||
            val == '#' || val == '[' || val == ']' ||
            val == '@');
}

/* Characters that delimit the end of the authority
 * section of a URI. Sometimes a '\\' is considered
 * an authority delimeter.
 */
static inline BOOL is_auth_delim(WCHAR val, BOOL acceptSlash) {
    return (val == '#' || val == '/' || val == '?' ||
            val == '\0' || (acceptSlash && val == '\\'));
}

/* reserved = gen-delims / sub-delims */
static inline BOOL is_reserved(WCHAR val) {
    return (is_subdelim(val) || is_gendelim(val));
}

static inline BOOL is_hexdigit(WCHAR val) {
    return ((val >= 'a' && val <= 'f') ||
            (val >= 'A' && val <= 'F') ||
            (val >= '0' && val <= '9'));
}

static inline BOOL is_path_delim(WCHAR val) {
    return (!val || val == '#' || val == '?');
}

static BOOL is_default_port(URL_SCHEME scheme, DWORD port) {
    DWORD i;

    for(i = 0; i < sizeof(default_ports)/sizeof(default_ports[0]); ++i) {
        if(default_ports[i].scheme == scheme && default_ports[i].port)
            return TRUE;
    }

    return FALSE;
}

/* List of schemes types Windows seems to expect to be hierarchical. */
static inline BOOL is_hierarchical_scheme(URL_SCHEME type) {
    return(type == URL_SCHEME_HTTP || type == URL_SCHEME_FTP ||
           type == URL_SCHEME_GOPHER || type == URL_SCHEME_NNTP ||
           type == URL_SCHEME_TELNET || type == URL_SCHEME_WAIS ||
           type == URL_SCHEME_FILE || type == URL_SCHEME_HTTPS ||
           type == URL_SCHEME_RES);
}

/* Checks if 'flags' contains an invalid combination of Uri_CREATE flags. */
static inline BOOL has_invalid_flag_combination(DWORD flags) {
    return((flags & Uri_CREATE_DECODE_EXTRA_INFO && flags & Uri_CREATE_NO_DECODE_EXTRA_INFO) ||
           (flags & Uri_CREATE_CANONICALIZE && flags & Uri_CREATE_NO_CANONICALIZE) ||
           (flags & Uri_CREATE_CRACK_UNKNOWN_SCHEMES && flags & Uri_CREATE_NO_CRACK_UNKNOWN_SCHEMES) ||
           (flags & Uri_CREATE_PRE_PROCESS_HTML_URI && flags & Uri_CREATE_NO_PRE_PROCESS_HTML_URI) ||
           (flags & Uri_CREATE_IE_SETTINGS && flags & Uri_CREATE_NO_IE_SETTINGS));
}

/* Applies each default Uri_CREATE flags to 'flags' if it
 * doesn't cause a flag conflict.
 */
static void apply_default_flags(DWORD *flags) {
    if(!(*flags & Uri_CREATE_NO_CANONICALIZE))
        *flags |= Uri_CREATE_CANONICALIZE;
    if(!(*flags & Uri_CREATE_NO_DECODE_EXTRA_INFO))
        *flags |= Uri_CREATE_DECODE_EXTRA_INFO;
    if(!(*flags & Uri_CREATE_NO_CRACK_UNKNOWN_SCHEMES))
        *flags |= Uri_CREATE_CRACK_UNKNOWN_SCHEMES;
    if(!(*flags & Uri_CREATE_NO_PRE_PROCESS_HTML_URI))
        *flags |= Uri_CREATE_PRE_PROCESS_HTML_URI;
    if(!(*flags & Uri_CREATE_IE_SETTINGS))
        *flags |= Uri_CREATE_NO_IE_SETTINGS;
}

/* Determines if the URI is hierarchical using the information already parsed into
 * data and using the current location of parsing in the URI string.
 *
 * Windows considers a URI hierarchical if on of the following is true:
 *  A.) It's a wildcard scheme.
 *  B.) It's an implicit file scheme.
 *  C.) It's a known hierarchical scheme and it has two '\\' after the scheme name.
 *      (the '\\' will be converted into "//" during canonicalization).
 *  D.) It's not a relative URI and "//" appears after the scheme name.
 */
static inline BOOL is_hierarchical_uri(const WCHAR **ptr, const parse_data *data) {
    const WCHAR *start = *ptr;

    if(data->scheme_type == URL_SCHEME_WILDCARD)
        return TRUE;
    else if(data->scheme_type == URL_SCHEME_FILE && data->has_implicit_scheme)
        return TRUE;
    else if(is_hierarchical_scheme(data->scheme_type) && (*ptr)[0] == '\\' && (*ptr)[1] == '\\') {
        *ptr += 2;
        return TRUE;
    } else if(!data->is_relative && check_hierarchical(ptr))
        return TRUE;

    *ptr = start;
    return FALSE;
}

/* Checks if the two Uri's are logically equivalent. It's a simple
 * comparison, since they are both of type Uri, and it can access
 * the properties of each Uri directly without the need to go
 * through the "IUri_Get*" interface calls.
 */
static BOOL are_equal_simple(const Uri *a, const Uri *b) {
    if(a->scheme_type == b->scheme_type) {
        const BOOL known_scheme = a->scheme_type != URL_SCHEME_UNKNOWN;
        const BOOL are_hierarchical =
                (a->authority_start > -1 && b->authority_start > -1);

        if(a->scheme_type == URL_SCHEME_FILE) {
            if(a->canon_len == b->canon_len)
                return !StrCmpIW(a->canon_uri, b->canon_uri);
        }

        /* Only compare the scheme names (if any) if their unknown scheme types. */
        if(!known_scheme) {
            if((a->scheme_start > -1 && b->scheme_start > -1) &&
               (a->scheme_len == b->scheme_len)) {
                /* Make sure the schemes are the same. */
                if(StrCmpNW(a->canon_uri+a->scheme_start, b->canon_uri+b->scheme_start, a->scheme_len))
                    return FALSE;
            } else if(a->scheme_len != b->scheme_len)
                /* One of the Uri's has a scheme name, while the other doesn't. */
                return FALSE;
        }

        /* If they have a userinfo component, perform case sensitive compare. */
        if((a->userinfo_start > -1 && b->userinfo_start > -1) &&
           (a->userinfo_len == b->userinfo_len)) {
            if(StrCmpNW(a->canon_uri+a->userinfo_start, b->canon_uri+b->userinfo_start, a->userinfo_len))
                return FALSE;
        } else if(a->userinfo_len != b->userinfo_len)
            /* One of the Uri's had a userinfo, while the other one doesn't. */
            return FALSE;

        /* Check if they have a host name. */
        if((a->host_start > -1 && b->host_start > -1) &&
           (a->host_len == b->host_len)) {
            /* Perform a case insensitive compare if they are a known scheme type. */
            if(known_scheme) {
                if(StrCmpNIW(a->canon_uri+a->host_start, b->canon_uri+b->host_start, a->host_len))
                    return FALSE;
            } else if(StrCmpNW(a->canon_uri+a->host_start, b->canon_uri+b->host_start, a->host_len))
                return FALSE;
        } else if(a->host_len != b->host_len)
            /* One of the Uri's had a host, while the other one didn't. */
            return FALSE;

        if(a->has_port && b->has_port) {
            if(a->port != b->port)
                return FALSE;
        } else if(a->has_port || b->has_port)
            /* One had a port, while the other one didn't. */
            return FALSE;

        /* Windows is weird with how it handles paths. For example
         * One URI could be "http://google.com" (after canonicalization)
         * and one could be "http://google.com/" and the IsEqual function
         * would still evaluate to TRUE, but, only if they are both hierarchical
         * URIs.
         */
        if((a->path_start > -1 && b->path_start > -1) &&
           (a->path_len == b->path_len)) {
            if(StrCmpNW(a->canon_uri+a->path_start, b->canon_uri+b->path_start, a->path_len))
                return FALSE;
        } else if(are_hierarchical && a->path_len == -1 && b->path_len == 0) {
            if(*(a->canon_uri+a->path_start) != '/')
                return FALSE;
        } else if(are_hierarchical && b->path_len == 1 && a->path_len == 0) {
            if(*(b->canon_uri+b->path_start) != '/')
                return FALSE;
        } else if(a->path_len != b->path_len)
            return FALSE;

        /* Compare the query strings of the two URIs. */
        if((a->query_start > -1 && b->query_start > -1) &&
           (a->query_len == b->query_len)) {
            if(StrCmpNW(a->canon_uri+a->query_start, b->canon_uri+b->query_start, a->query_len))
                return FALSE;
        } else if(a->query_len != b->query_len)
            return FALSE;

        if((a->fragment_start > -1 && b->fragment_start > -1) &&
           (a->fragment_len == b->fragment_len)) {
            if(StrCmpNW(a->canon_uri+a->fragment_start, b->canon_uri+b->fragment_start, a->fragment_len))
                return FALSE;
        } else if(a->fragment_len != b->fragment_len)
            return FALSE;

        /* If we get here, the two URIs are equivalent. */
        return TRUE;
    }

    return FALSE;
}

/* Computes the size of the given IPv6 address.
 * Each h16 component is 16bits, if there is an IPv4 address, it's
 * 32bits. If there's an elision it can be 16bits to 128bits, depending
 * on the number of other components.
 *
 * Modeled after google-url's CheckIPv6ComponentsSize function
 */
static void compute_ipv6_comps_size(ipv6_address *address) {
    address->components_size = address->h16_count * 2;

    if(address->ipv4)
        /* IPv4 address is 4 bytes. */
        address->components_size += 4;

    if(address->elision) {
        /* An elision can be anywhere from 2 bytes up to 16 bytes.
         * It size depends on the size of the h16 and IPv4 components.
         */
        address->elision_size = 16 - address->components_size;
        if(address->elision_size < 2)
            address->elision_size = 2;
    } else
        address->elision_size = 0;
}

/* Taken from dlls/jscript/lex.c */
static int hex_to_int(WCHAR val) {
    if(val >= '0' && val <= '9')
        return val - '0';
    else if(val >= 'a' && val <= 'f')
        return val - 'a' + 10;
    else if(val >= 'A' && val <= 'F')
        return val - 'A' + 10;

    return -1;
}

/* Helper function for converting a percent encoded string
 * representation of a WCHAR value into its actual WCHAR value. If
 * the two characters following the '%' aren't valid hex values then
 * this function returns the NULL character.
 *
 * Eg.
 *  "%2E" will result in '.' being returned by this function.
 */
static WCHAR decode_pct_val(const WCHAR *ptr) {
    WCHAR ret = '\0';

    if(*ptr == '%' && is_hexdigit(*(ptr + 1)) && is_hexdigit(*(ptr + 2))) {
        INT a = hex_to_int(*(ptr + 1));
        INT b = hex_to_int(*(ptr + 2));

        ret = a << 4;
        ret += b;
    }

    return ret;
}

/* Helper function for percent encoding a given character
 * and storing the encoded value into a given buffer (dest).
 *
 * It's up to the calling function to ensure that there is
 * at least enough space in 'dest' for the percent encoded
 * value to be stored (so dest + 3 spaces available).
 */
static inline void pct_encode_val(WCHAR val, WCHAR *dest) {
    dest[0] = '%';
    dest[1] = hexDigits[(val >> 4) & 0xf];
    dest[2] = hexDigits[val & 0xf];
}

/* Scans the range of characters [str, end] and returns the last occurrence
 * of 'ch' or returns NULL.
 */
static const WCHAR *str_last_of(const WCHAR *str, const WCHAR *end, WCHAR ch) {
    const WCHAR *ptr = end;

    while(ptr >= str) {
        if(*ptr == ch)
            return ptr;
        --ptr;
    }

    return NULL;
}

/* Attempts to parse the domain name from the host.
 *
 * This function also includes the Top-level Domain (TLD) name
 * of the host when it tries to find the domain name. If it finds
 * a valid domain name it will assign 'domain_start' the offset
 * into 'host' where the domain name starts.
 *
 * It's implied that if a domain name its range is implied to be
 * [host+domain_start, host+host_len).
 */
static void find_domain_name(const WCHAR *host, DWORD host_len,
                             INT *domain_start) {
    const WCHAR *last_tld, *sec_last_tld, *end;

    end = host+host_len-1;

    *domain_start = -1;

    /* There has to be at least enough room for a '.' followed by a
     * 3 character TLD for a domain to even exist in the host name.
     */
    if(host_len < 4)
        return;

    last_tld = str_last_of(host, end, '.');
    if(!last_tld)
        /* http://hostname -> has no domain name. */
        return;

    sec_last_tld = str_last_of(host, last_tld-1, '.');
    if(!sec_last_tld) {
        /* If the '.' is at the beginning of the host there
         * has to be at least 3 characters in the TLD for it
         * to be valid.
         *  Ex: .com -> .com as the domain name.
         *      .co  -> has no domain name.
         */
        if(last_tld-host == 0) {
            if(end-(last_tld-1) < 3)
                return;
        } else if(last_tld-host == 3) {
            DWORD i;

            /* If there's three characters in front of last_tld and
             * they are on the list of recognized TLDs, then this
             * host doesn't have a domain (since the host only contains
             * a TLD name.
             *  Ex: edu.uk -> has no domain name.
             *      foo.uk -> foo.uk as the domain name.
             */
            for(i = 0; i < sizeof(recognized_tlds)/sizeof(recognized_tlds[0]); ++i) {
                if(!StrCmpNIW(host, recognized_tlds[i].tld_name, 3))
                    return;
            }
        } else if(last_tld-host < 3)
            /* Anything less than 3 characters is considered part
             * of the TLD name.
             *  Ex: ak.uk -> Has no domain name.
             */
            return;

        /* Otherwise the domain name is the whole host name. */
        *domain_start = 0;
    } else if(end+1-last_tld > 3) {
        /* If the last_tld has more than 3 characters, then it's automatically
         * considered the TLD of the domain name.
         *  Ex: www.winehq.org.uk.test -> uk.test as the domain name.
         */
        *domain_start = (sec_last_tld+1)-host;
    } else if(last_tld - (sec_last_tld+1) < 4) {
        DWORD i;
        /* If the sec_last_tld is 3 characters long it HAS to be on the list of
         * recognized to still be considered part of the TLD name, otherwise
         * its considered the domain name.
         *  Ex: www.google.com.uk -> google.com.uk as the domain name.
         *      www.google.foo.uk -> foo.uk as the domain name.
         */
        if(last_tld - (sec_last_tld+1) == 3) {
            for(i = 0; i < sizeof(recognized_tlds)/sizeof(recognized_tlds[0]); ++i) {
                if(!StrCmpNIW(sec_last_tld+1, recognized_tlds[i].tld_name, 3)) {
                    const WCHAR *domain = str_last_of(host, sec_last_tld-1, '.');

                    if(!domain)
                        *domain_start = 0;
                    else
                        *domain_start = (domain+1) - host;
                    TRACE("Found domain name %s\n", debugstr_wn(host+*domain_start,
                                                        (host+host_len)-(host+*domain_start)));
                    return;
                }
            }

            *domain_start = (sec_last_tld+1)-host;
        } else {
            /* Since the sec_last_tld is less than 3 characters it's considered
             * part of the TLD.
             *  Ex: www.google.fo.uk -> google.fo.uk as the domain name.
             */
            const WCHAR *domain = str_last_of(host, sec_last_tld-1, '.');

            if(!domain)
                *domain_start = 0;
            else
                *domain_start = (domain+1) - host;
        }
    } else {
        /* The second to last TLD has more than 3 characters making it
         * the domain name.
         *  Ex: www.google.test.us -> test.us as the domain name.
         */
        *domain_start = (sec_last_tld+1)-host;
    }

    TRACE("Found domain name %s\n", debugstr_wn(host+*domain_start,
                                        (host+host_len)-(host+*domain_start)));
}

/* Removes the dot segments from a hierarchical URIs path component. This
 * function performs the removal in place.
 *
 * This is a modified version of Qt's QUrl function "removeDotsFromPath".
 *
 * This function returns the new length of the path string.
 */
static DWORD remove_dot_segments(WCHAR *path, DWORD path_len) {
    WCHAR *out = path;
    const WCHAR *in = out;
    const WCHAR *end = out + path_len;
    DWORD len;

    while(in < end) {
        /* A.  if the input buffer begins with a prefix of "/./" or "/.",
         *     where "." is a complete path segment, then replace that
         *     prefix with "/" in the input buffer; otherwise,
         */
        if(in <= end - 3 && in[0] == '/' && in[1] == '.' && in[2] == '/') {
            in += 2;
            continue;
        } else if(in == end - 2 && in[0] == '/' && in[1] == '.') {
            *out++ = '/';
            in += 2;
            break;
        }

        /* B.  if the input buffer begins with a prefix of "/../" or "/..",
         *     where ".." is a complete path segment, then replace that
         *     prefix with "/" in the input buffer and remove the last
         *     segment and its preceding "/" (if any) from the output
         *     buffer; otherwise,
         */
        if(in <= end - 4 && in[0] == '/' && in[1] == '.' && in[2] == '.' && in[3] == '/') {
            while(out > path && *(--out) != '/');

            in += 3;
            continue;
        } else if(in == end - 3 && in[0] == '/' && in[1] == '.' && in[2] == '.') {
            while(out > path && *(--out) != '/');

            if(*out == '/')
                ++out;

            in += 3;
            break;
        }

        /* C.  move the first path segment in the input buffer to the end of
         *     the output buffer, including the initial "/" character (if
         *     any) and any subsequent characters up to, but not including,
         *     the next "/" character or the end of the input buffer.
         */
        *out++ = *in++;
        while(in < end && *in != '/')
            *out++ = *in++;
    }

    len = out - path;
    TRACE("(%p %d): Path after dot segments removed %s len=%d\n", path, path_len,
        debugstr_wn(path, len), len);
    return len;
}

/* Attempts to find the file extension in a given path. */
static INT find_file_extension(const WCHAR *path, DWORD path_len) {
    const WCHAR *end;

    for(end = path+path_len-1; end >= path && *end != '/' && *end != '\\'; --end) {
        if(*end == '.')
            return end-path;
    }

    return -1;
}

/* Computes the location where the elision should occur in the IPv6
 * address using the numerical values of each component stored in
 * 'values'. If the address shouldn't contain an elision then 'index'
 * is assigned -1 as it's value. Otherwise 'index' will contain the
 * starting index (into values) where the elision should be, and 'count'
 * will contain the number of cells the elision covers.
 *
 * NOTES:
 *  Windows will expand an elision if the elision only represents 1 h16
 *  component of the URI.
 *
 *  Ex: [1::2:3:4:5:6:7] -> [1:0:2:3:4:5:6:7]
 *
 *  If the IPv6 address contains an IPv4 address, the IPv4 address is also
 *  considered for being included as part of an elision if all it's components
 *  are zeros.
 *
 *  Ex: [1:2:3:4:5:6:0.0.0.0] -> [1:2:3:4:5:6::]
 */
static void compute_elision_location(const ipv6_address *address, const USHORT values[8],
                                     INT *index, DWORD *count) {
    DWORD i, max_len, cur_len;
    INT max_index, cur_index;

    max_len = cur_len = 0;
    max_index = cur_index = -1;
    for(i = 0; i < 8; ++i) {
        BOOL check_ipv4 = (address->ipv4 && i == 6);
        BOOL is_end = (check_ipv4 || i == 7);

        if(check_ipv4) {
            /* Check if the IPv4 address contains only zeros. */
            if(values[i] == 0 && values[i+1] == 0) {
                if(cur_index == -1)
                    cur_index = i;

                cur_len += 2;
                ++i;
            }
        } else if(values[i] == 0) {
            if(cur_index == -1)
                cur_index = i;

            ++cur_len;
        }

        if(is_end || values[i] != 0) {
            /* We only consider it for an elision if it's
             * more than 1 component long.
             */
            if(cur_len > 1 && cur_len > max_len) {
                /* Found the new elision location. */
                max_len = cur_len;
                max_index = cur_index;
            }

            /* Reset the current range for the next range of zeros. */
            cur_index = -1;
            cur_len = 0;
        }
    }

    *index = max_index;
    *count = max_len;
}

/* Removes all the leading and trailing white spaces or
 * control characters from the URI and removes all control
 * characters inside of the URI string.
 */
static BSTR pre_process_uri(LPCWSTR uri) {
    BSTR ret;
    DWORD len;
    const WCHAR *start, *end;
    WCHAR *buf, *ptr;

    len = lstrlenW(uri);

    start = uri;
    /* Skip leading controls and whitespace. */
    while(iscntrlW(*start) || isspaceW(*start)) ++start;

    end = uri+len-1;
    if(start == end)
        /* URI consisted only of control/whitespace. */
        ret = SysAllocStringLen(NULL, 0);
    else {
        while(iscntrlW(*end) || isspaceW(*end)) --end;

        buf = heap_alloc(((end+1)-start)*sizeof(WCHAR));
        if(!buf)
            return NULL;

        for(ptr = buf; start < end+1; ++start) {
            if(!iscntrlW(*start))
                *ptr++ = *start;
        }

        ret = SysAllocStringLen(buf, ptr-buf);
        heap_free(buf);
    }

    return ret;
}

/* Converts the specified IPv4 address into an uint value.
 *
 * This function assumes that the IPv4 address has already been validated.
 */
static UINT ipv4toui(const WCHAR *ip, DWORD len) {
    UINT ret = 0;
    DWORD comp_value = 0;
    const WCHAR *ptr;

    for(ptr = ip; ptr < ip+len; ++ptr) {
        if(*ptr == '.') {
            ret <<= 8;
            ret += comp_value;
            comp_value = 0;
        } else
            comp_value = comp_value*10 + (*ptr-'0');
    }

    ret <<= 8;
    ret += comp_value;

    return ret;
}

/* Converts an IPv4 address in numerical form into it's fully qualified
 * string form. This function returns the number of characters written
 * to 'dest'. If 'dest' is NULL this function will return the number of
 * characters that would have been written.
 *
 * It's up to the caller to ensure there's enough space in 'dest' for the
 * address.
 */
static DWORD ui2ipv4(WCHAR *dest, UINT address) {
    static const WCHAR formatW[] =
        {'%','u','.','%','u','.','%','u','.','%','u',0};
    DWORD ret = 0;
    UCHAR digits[4];

    digits[0] = (address >> 24) & 0xff;
    digits[1] = (address >> 16) & 0xff;
    digits[2] = (address >> 8) & 0xff;
    digits[3] = address & 0xff;

    if(!dest) {
        WCHAR tmp[16];
        ret = sprintfW(tmp, formatW, digits[0], digits[1], digits[2], digits[3]);
    } else
        ret = sprintfW(dest, formatW, digits[0], digits[1], digits[2], digits[3]);

    return ret;
}

static DWORD ui2str(WCHAR *dest, UINT value) {
    static const WCHAR formatW[] = {'%','u',0};
    DWORD ret = 0;

    if(!dest) {
        WCHAR tmp[11];
        ret = sprintfW(tmp, formatW, value);
    } else
        ret = sprintfW(dest, formatW, value);

    return ret;
}

/* Converts an h16 component (from an IPv6 address) into it's
 * numerical value.
 *
 * This function assumes that the h16 component has already been validated.
 */
static USHORT h16tous(h16 component) {
    DWORD i;
    USHORT ret = 0;

    for(i = 0; i < component.len; ++i) {
        ret <<= 4;
        ret += hex_to_int(component.str[i]);
    }

    return ret;
}

/* Converts an IPv6 address into it's 128 bits (16 bytes) numerical value.
 *
 * This function assumes that the ipv6_address has already been validated.
 */
static BOOL ipv6_to_number(const ipv6_address *address, USHORT number[8]) {
    DWORD i, cur_component = 0;
    BOOL already_passed_elision = FALSE;

    for(i = 0; i < address->h16_count; ++i) {
        if(address->elision) {
            if(address->components[i].str > address->elision && !already_passed_elision) {
                /* Means we just passed the elision and need to add it's values to
                 * 'number' before we do anything else.
                 */
                DWORD j = 0;
                for(j = 0; j < address->elision_size; j+=2)
                    number[cur_component++] = 0;

                already_passed_elision = TRUE;
            }
        }

        number[cur_component++] = h16tous(address->components[i]);
    }

    /* Case when the elision appears after the h16 components. */
    if(!already_passed_elision && address->elision) {
        for(i = 0; i < address->elision_size; i+=2)
            number[cur_component++] = 0;
        already_passed_elision = TRUE;
    }

    if(address->ipv4) {
        UINT value = ipv4toui(address->ipv4, address->ipv4_len);

        if(cur_component != 6) {
            ERR("(%p %p): Failed sanity check with %d\n", address, number, cur_component);
            return FALSE;
        }

        number[cur_component++] = (value >> 16) & 0xffff;
        number[cur_component] = value & 0xffff;
    }

    return TRUE;
}

/* Checks if the characters pointed to by 'ptr' are
 * a percent encoded data octet.
 *
 * pct-encoded = "%" HEXDIG HEXDIG
 */
static BOOL check_pct_encoded(const WCHAR **ptr) {
    const WCHAR *start = *ptr;

    if(**ptr != '%')
        return FALSE;

    ++(*ptr);
    if(!is_hexdigit(**ptr)) {
        *ptr = start;
        return FALSE;
    }

    ++(*ptr);
    if(!is_hexdigit(**ptr)) {
        *ptr = start;
        return FALSE;
    }

    ++(*ptr);
    return TRUE;
}

/* dec-octet   = DIGIT                 ; 0-9
 *             / %x31-39 DIGIT         ; 10-99
 *             / "1" 2DIGIT            ; 100-199
 *             / "2" %x30-34 DIGIT     ; 200-249
 *             / "25" %x30-35          ; 250-255
 */
static BOOL check_dec_octet(const WCHAR **ptr) {
    const WCHAR *c1, *c2, *c3;

    c1 = *ptr;
    /* A dec-octet must be at least 1 digit long. */
    if(*c1 < '0' || *c1 > '9')
        return FALSE;

    ++(*ptr);

    c2 = *ptr;
    /* Since the 1 digit requirment was meet, it doesn't
     * matter if this is a DIGIT value, it's considered a
     * dec-octet.
     */
    if(*c2 < '0' || *c2 > '9')
        return TRUE;

    ++(*ptr);

    c3 = *ptr;
    /* Same explanation as above. */
    if(*c3 < '0' || *c3 > '9')
        return TRUE;

    /* Anything > 255 isn't a valid IP dec-octet. */
    if(*c1 >= '2' && *c2 >= '5' && *c3 >= '5') {
        *ptr = c1;
        return FALSE;
    }

    ++(*ptr);
    return TRUE;
}

/* Checks if there is an implicit IPv4 address in the host component of the URI.
 * The max value of an implicit IPv4 address is UINT_MAX.
 *
 *  Ex:
 *      "234567" would be considered an implicit IPv4 address.
 */
static BOOL check_implicit_ipv4(const WCHAR **ptr, UINT *val) {
    const WCHAR *start = *ptr;
    ULONGLONG ret = 0;
    *val = 0;

    while(is_num(**ptr)) {
        ret = ret*10 + (**ptr - '0');

        if(ret > UINT_MAX) {
            *ptr = start;
            return FALSE;
        }
        ++(*ptr);
    }

    if(*ptr == start)
        return FALSE;

    *val = ret;
    return TRUE;
}

/* Checks if the string contains an IPv4 address.
 *
 * This function has a strict mode or a non-strict mode of operation
 * When 'strict' is set to FALSE this function will return TRUE if
 * the string contains at least 'dec-octet "." dec-octet' since partial
 * IPv4 addresses will be normalized out into full IPv4 addresses. When
 * 'strict' is set this function expects there to be a full IPv4 address.
 *
 * IPv4address = dec-octet "." dec-octet "." dec-octet "." dec-octet
 */
static BOOL check_ipv4address(const WCHAR **ptr, BOOL strict) {
    const WCHAR *start = *ptr;

    if(!check_dec_octet(ptr)) {
        *ptr = start;
        return FALSE;
    }

    if(**ptr != '.') {
        *ptr = start;
        return FALSE;
    }

    ++(*ptr);
    if(!check_dec_octet(ptr)) {
        *ptr = start;
        return FALSE;
    }

    if(**ptr != '.') {
        if(strict) {
            *ptr = start;
            return FALSE;
        } else
            return TRUE;
    }

    ++(*ptr);
    if(!check_dec_octet(ptr)) {
        *ptr = start;
        return FALSE;
    }

    if(**ptr != '.') {
        if(strict) {
            *ptr = start;
            return FALSE;
        } else
            return TRUE;
    }

    ++(*ptr);
    if(!check_dec_octet(ptr)) {
        *ptr = start;
        return FALSE;
    }

    /* Found a four digit ip address. */
    return TRUE;
}
/* Tries to parse the scheme name of the URI.
 *
 * scheme = ALPHA *(ALPHA | NUM | '+' | '-' | '.') as defined by RFC 3896.
 * NOTE: Windows accepts a number as the first character of a scheme.
 */
static BOOL parse_scheme_name(const WCHAR **ptr, parse_data *data, DWORD extras) {
    const WCHAR *start = *ptr;

    data->scheme = NULL;
    data->scheme_len = 0;

    while(**ptr) {
        if(**ptr == '*' && *ptr == start) {
            /* Might have found a wildcard scheme. If it is the next
             * char has to be a ':' for it to be a valid URI
             */
            ++(*ptr);
            break;
        } else if(!is_num(**ptr) && !is_alpha(**ptr) && **ptr != '+' &&
           **ptr != '-' && **ptr != '.')
            break;

        (*ptr)++;
    }

    if(*ptr == start)
        return FALSE;

    /* Schemes must end with a ':' */
    if(**ptr != ':' && !((extras & ALLOW_NULL_TERM_SCHEME) && !**ptr)) {
        *ptr = start;
        return FALSE;
    }

    data->scheme = start;
    data->scheme_len = *ptr - start;

    ++(*ptr);
    return TRUE;
}

/* Tries to deduce the corresponding URL_SCHEME for the given URI. Stores
 * the deduced URL_SCHEME in data->scheme_type.
 */
static BOOL parse_scheme_type(parse_data *data) {
    /* If there's scheme data then see if it's a recognized scheme. */
    if(data->scheme && data->scheme_len) {
        DWORD i;

        for(i = 0; i < sizeof(recognized_schemes)/sizeof(recognized_schemes[0]); ++i) {
            if(lstrlenW(recognized_schemes[i].scheme_name) == data->scheme_len) {
                /* Has to be a case insensitive compare. */
                if(!StrCmpNIW(recognized_schemes[i].scheme_name, data->scheme, data->scheme_len)) {
                    data->scheme_type = recognized_schemes[i].scheme;
                    return TRUE;
                }
            }
        }

        /* If we get here it means it's not a recognized scheme. */
        data->scheme_type = URL_SCHEME_UNKNOWN;
        return TRUE;
    } else if(data->is_relative) {
        /* Relative URI's have no scheme. */
        data->scheme_type = URL_SCHEME_UNKNOWN;
        return TRUE;
    } else {
        /* Should never reach here! what happened... */
        FIXME("(%p): Unable to determine scheme type for URI %s\n", data, debugstr_w(data->uri));
        return FALSE;
    }
}

/* Tries to parse (or deduce) the scheme_name of a URI. If it can't
 * parse a scheme from the URI it will try to deduce the scheme_name and scheme_type
 * using the flags specified in 'flags' (if any). Flags that affect how this function
 * operates are the Uri_CREATE_ALLOW_* flags.
 *
 * All parsed/deduced information will be stored in 'data' when the function returns.
 *
 * Returns TRUE if it was able to successfully parse the information.
 */
static BOOL parse_scheme(const WCHAR **ptr, parse_data *data, DWORD flags, DWORD extras) {
    static const WCHAR fileW[] = {'f','i','l','e',0};
    static const WCHAR wildcardW[] = {'*',0};

    /* First check to see if the uri could implicitly be a file path. */
    if(is_implicit_file_path(*ptr)) {
        if(flags & Uri_CREATE_ALLOW_IMPLICIT_FILE_SCHEME) {
            data->scheme = fileW;
            data->scheme_len = lstrlenW(fileW);
            data->has_implicit_scheme = TRUE;

            TRACE("(%p %p %x): URI is an implicit file path.\n", ptr, data, flags);
        } else {
            /* Window's does not consider anything that can implicitly be a file
             * path to be a valid URI if the ALLOW_IMPLICIT_FILE_SCHEME flag is not set...
             */
            TRACE("(%p %p %x): URI is implicitly a file path, but, the ALLOW_IMPLICIT_FILE_SCHEME flag wasn't set.\n",
                    ptr, data, flags);
            return FALSE;
        }
    } else if(!parse_scheme_name(ptr, data, extras)) {
        /* No Scheme was found, this means it could be:
         *      a) an implicit Wildcard scheme
         *      b) a relative URI
         *      c) a invalid URI.
         */
        if(flags & Uri_CREATE_ALLOW_IMPLICIT_WILDCARD_SCHEME) {
            data->scheme = wildcardW;
            data->scheme_len = lstrlenW(wildcardW);
            data->has_implicit_scheme = TRUE;

            TRACE("(%p %p %x): URI is an implicit wildcard scheme.\n", ptr, data, flags);
        } else if (flags & Uri_CREATE_ALLOW_RELATIVE) {
            data->is_relative = TRUE;
            TRACE("(%p %p %x): URI is relative.\n", ptr, data, flags);
        } else {
            TRACE("(%p %p %x): Malformed URI found. Unable to deduce scheme name.\n", ptr, data, flags);
            return FALSE;
        }
    }

    if(!data->is_relative)
        TRACE("(%p %p %x): Found scheme=%s scheme_len=%d\n", ptr, data, flags,
                debugstr_wn(data->scheme, data->scheme_len), data->scheme_len);

    if(!parse_scheme_type(data))
        return FALSE;

    TRACE("(%p %p %x): Assigned %d as the URL_SCHEME.\n", ptr, data, flags, data->scheme_type);
    return TRUE;
}

static BOOL parse_username(const WCHAR **ptr, parse_data *data, DWORD flags, DWORD extras) {
    data->username = *ptr;

    while(**ptr != ':' && **ptr != '@') {
        if(**ptr == '%') {
            if(!check_pct_encoded(ptr)) {
                if(data->scheme_type != URL_SCHEME_UNKNOWN) {
                    *ptr = data->username;
                    data->username = NULL;
                    return FALSE;
                }
            } else
                continue;
        } else if(extras & ALLOW_NULL_TERM_USER_NAME && !**ptr)
            break;
        else if(is_auth_delim(**ptr, data->scheme_type != URL_SCHEME_UNKNOWN)) {
            *ptr = data->username;
            data->username = NULL;
            return FALSE;
        }

        ++(*ptr);
    }

    data->username_len = *ptr - data->username;
    return TRUE;
}

static BOOL parse_password(const WCHAR **ptr, parse_data *data, DWORD flags, DWORD extras) {
    data->password = *ptr;

    while(**ptr != '@') {
        if(**ptr == '%') {
            if(!check_pct_encoded(ptr)) {
                if(data->scheme_type != URL_SCHEME_UNKNOWN) {
                    *ptr = data->password;
                    data->password = NULL;
                    return FALSE;
                }
            } else
                continue;
        } else if(extras & ALLOW_NULL_TERM_PASSWORD && !**ptr)
            break;
        else if(is_auth_delim(**ptr, data->scheme_type != URL_SCHEME_UNKNOWN)) {
            *ptr = data->password;
            data->password = NULL;
            return FALSE;
        }

        ++(*ptr);
    }

    data->password_len = *ptr - data->password;
    return TRUE;
}

/* Parses the userinfo part of the URI (if it exists). The userinfo field of
 * a URI can consist of "username:password@", or just "username@".
 *
 * RFC def:
 * userinfo    = *( unreserved / pct-encoded / sub-delims / ":" )
 *
 * NOTES:
 *  1)  If there is more than one ':' in the userinfo part of the URI Windows
 *      uses the first occurrence of ':' to delimit the username and password
 *      components.
 *
 *      ex:
 *          ftp://user:pass:word@winehq.org
 *
 *      Would yield, "user" as the username and "pass:word" as the password.
 *
 *  2)  Windows allows any character to appear in the "userinfo" part of
 *      a URI, as long as it's not an authority delimeter character set.
 */
static void parse_userinfo(const WCHAR **ptr, parse_data *data, DWORD flags) {
    const WCHAR *start = *ptr;

    if(!parse_username(ptr, data, flags, 0)) {
        TRACE("(%p %p %x): URI contained no userinfo.\n", ptr, data, flags);
        return;
    }

    if(**ptr == ':') {
        ++(*ptr);
        if(!parse_password(ptr, data, flags, 0)) {
            *ptr = start;
            data->username = NULL;
            data->username_len = 0;
            TRACE("(%p %p %x): URI contained no userinfo.\n", ptr, data, flags);
            return;
        }
    }

    if(**ptr != '@') {
        *ptr = start;
        data->username = NULL;
        data->username_len = 0;
        data->password = NULL;
        data->password_len = 0;

        TRACE("(%p %p %x): URI contained no userinfo.\n", ptr, data, flags);
        return;
    }

    if(data->username)
        TRACE("(%p %p %x): Found username %s len=%d.\n", ptr, data, flags,
            debugstr_wn(data->username, data->username_len), data->username_len);

    if(data->password)
        TRACE("(%p %p %x): Found password %s len=%d.\n", ptr, data, flags,
            debugstr_wn(data->password, data->password_len), data->password_len);

    ++(*ptr);
}

/* Attempts to parse a port from the URI.
 *
 * NOTES:
 *  Windows seems to have a cap on what the maximum value
 *  for a port can be. The max value is USHORT_MAX.
 *
 * port = *DIGIT
 */
static BOOL parse_port(const WCHAR **ptr, parse_data *data, DWORD flags) {
    UINT port = 0;
    data->port = *ptr;

    while(!is_auth_delim(**ptr, data->scheme_type != URL_SCHEME_UNKNOWN)) {
        if(!is_num(**ptr)) {
            *ptr = data->port;
            data->port = NULL;
            return FALSE;
        }

        port = port*10 + (**ptr-'0');

        if(port > USHORT_MAX) {
            *ptr = data->port;
            data->port = NULL;
            return FALSE;
        }

        ++(*ptr);
    }

    data->has_port = TRUE;
    data->port_value = port;
    data->port_len = *ptr - data->port;

    TRACE("(%p %p %x): Found port %s len=%d value=%u\n", ptr, data, flags,
        debugstr_wn(data->port, data->port_len), data->port_len, data->port_value);
    return TRUE;
}

/* Attempts to parse a IPv4 address from the URI.
 *
 * NOTES:
 *  Window's normalizes IPv4 addresses, This means there's three
 *  possibilities for the URI to contain an IPv4 address.
 *      1)  A well formed address (ex. 192.2.2.2).
 *      2)  A partially formed address. For example "192.0" would
 *          normalize to "192.0.0.0" during canonicalization.
 *      3)  An implicit IPv4 address. For example "256" would
 *          normalize to "0.0.1.0" during canonicalization. Also
 *          note that the maximum value for an implicit IP address
 *          is UINT_MAX, if the value in the URI exceeds this then
 *          it is not considered an IPv4 address.
 */
static BOOL parse_ipv4address(const WCHAR **ptr, parse_data *data, DWORD flags) {
    const BOOL is_unknown = data->scheme_type == URL_SCHEME_UNKNOWN;
    data->host = *ptr;

    if(!check_ipv4address(ptr, FALSE)) {
        if(!check_implicit_ipv4(ptr, &data->implicit_ipv4)) {
            TRACE("(%p %p %x): URI didn't contain anything looking like an IPv4 address.\n",
                ptr, data, flags);
            *ptr = data->host;
            data->host = NULL;
            return FALSE;
        } else
            data->has_implicit_ip = TRUE;
    }

    /* Check if what we found is the only part of the host name (if it isn't
     * we don't have an IPv4 address).
     */
    if(**ptr == ':') {
        ++(*ptr);
        if(!parse_port(ptr, data, flags)) {
            *ptr = data->host;
            data->host = NULL;
            return FALSE;
        }
    } else if(!is_auth_delim(**ptr, !is_unknown)) {
        /* Found more data which belongs the host, so this isn't an IPv4. */
        *ptr = data->host;
        data->host = NULL;
        data->has_implicit_ip = FALSE;
        return FALSE;
    }

    data->host_len = *ptr - data->host;
    data->host_type = Uri_HOST_IPV4;

    TRACE("(%p %p %x): IPv4 address found. host=%s host_len=%d host_type=%d\n",
        ptr, data, flags, debugstr_wn(data->host, data->host_len),
        data->host_len, data->host_type);
    return TRUE;
}

/* Attempts to parse the reg-name from the URI.
 *
 * Because of the way Windows handles ':' this function also
 * handles parsing the port.
 *
 * reg-name = *( unreserved / pct-encoded / sub-delims )
 *
 * NOTE:
 *  Windows allows everything, but, the characters in "auth_delims" and ':'
 *  to appear in a reg-name, unless it's an unknown scheme type then ':' is
 *  allowed to appear (even if a valid port isn't after it).
 *
 *  Windows doesn't like host names which start with '[' and end with ']'
 *  and don't contain a valid IP literal address in between them.
 *
 *  On Windows if an '[' is encountered in the host name the ':' no longer
 *  counts as a delimiter until you reach the next ']' or an "authority delimeter".
 *
 *  A reg-name CAN be empty.
 */
static BOOL parse_reg_name(const WCHAR **ptr, parse_data *data, DWORD flags, DWORD extras) {
    const BOOL has_start_bracket = **ptr == '[';
    const BOOL known_scheme = data->scheme_type != URL_SCHEME_UNKNOWN;
    BOOL inside_brackets = has_start_bracket;
    BOOL ignore_col = extras & IGNORE_PORT_DELIMITER;

    /* We have to be careful with file schemes. */
    if(data->scheme_type == URL_SCHEME_FILE) {
        /* This is because an implicit file scheme could be "C:\\test" and it
         * would trick this function into thinking the host is "C", when after
         * canonicalization the host would end up being an empty string. A drive
         * path can also have a '|' instead of a ':' after the drive letter.
         */
        if(is_drive_path(*ptr)) {
            /* Regular old drive paths don't have a host type (or host name). */
            data->host_type = Uri_HOST_UNKNOWN;
            data->host = *ptr;
            data->host_len = 0;
            return TRUE;
        } else if(is_unc_path(*ptr))
            /* Skip past the "\\" of a UNC path. */
            *ptr += 2;
    }

    data->host = *ptr;

    while(!is_auth_delim(**ptr, known_scheme)) {
        if(**ptr == ':' && !ignore_col) {
            /* We can ignore ':' if were inside brackets.*/
            if(!inside_brackets) {
                const WCHAR *tmp = (*ptr)++;

                /* Attempt to parse the port. */
                if(!parse_port(ptr, data, flags)) {
                    /* Windows expects there to be a valid port for known scheme types. */
                    if(data->scheme_type != URL_SCHEME_UNKNOWN) {
                        *ptr = data->host;
                        data->host = NULL;
                        TRACE("(%p %p %x %x): Expected valid port\n", ptr, data, flags, extras);
                        return FALSE;
                    } else
                        /* Windows gives up on trying to parse a port when it
                         * encounters 1 invalid port.
                         */
                        ignore_col = TRUE;
                } else {
                    data->host_len = tmp - data->host;
                    break;
                }
            }
        } else if(**ptr == '%' && known_scheme) {
            /* Has to be a legit % encoded value. */
            if(!check_pct_encoded(ptr)) {
                *ptr = data->host;
                data->host = NULL;
                return FALSE;
            } else
                continue;
        } else if(**ptr == ']')
            inside_brackets = FALSE;
        else if(**ptr == '[')
            inside_brackets = TRUE;

        ++(*ptr);
    }

    if(has_start_bracket) {
        /* Make sure the last character of the host wasn't a ']'. */
        if(*(*ptr-1) == ']') {
            TRACE("(%p %p %x %x): Expected an IP literal inside of the host\n",
                ptr, data, flags, extras);
            *ptr = data->host;
            data->host = NULL;
            return FALSE;
        }
    }

    /* Don't overwrite our length if we found a port earlier. */
    if(!data->port)
        data->host_len = *ptr - data->host;

    /* If the host is empty, then it's an unknown host type. */
    if(data->host_len == 0)
        data->host_type = Uri_HOST_UNKNOWN;
    else
        data->host_type = Uri_HOST_DNS;

    TRACE("(%p %p %x %x): Parsed reg-name. host=%s len=%d\n", ptr, data, flags, extras,
        debugstr_wn(data->host, data->host_len), data->host_len);
    return TRUE;
}

/* Attempts to parse an IPv6 address out of the URI.
 *
 * IPv6address =                               6( h16 ":" ) ls32
 *                /                       "::" 5( h16 ":" ) ls32
 *                / [               h16 ] "::" 4( h16 ":" ) ls32
 *                / [ *1( h16 ":" ) h16 ] "::" 3( h16 ":" ) ls32
 *                / [ *2( h16 ":" ) h16 ] "::" 2( h16 ":" ) ls32
 *                / [ *3( h16 ":" ) h16 ] "::"    h16 ":"   ls32
 *                / [ *4( h16 ":" ) h16 ] "::"              ls32
 *                / [ *5( h16 ":" ) h16 ] "::"              h16
 *                / [ *6( h16 ":" ) h16 ] "::"
 *
 * ls32        = ( h16 ":" h16 ) / IPv4address
 *             ; least-significant 32 bits of address.
 *
 * h16         = 1*4HEXDIG
 *             ; 16 bits of address represented in hexadecimal.
 *
 * Modeled after google-url's 'DoParseIPv6' function.
 */
static BOOL parse_ipv6address(const WCHAR **ptr, parse_data *data, DWORD flags) {
    const WCHAR *start, *cur_start;
    ipv6_address ip;

    start = cur_start = *ptr;
    memset(&ip, 0, sizeof(ipv6_address));

    for(;; ++(*ptr)) {
        /* Check if we're on the last character of the host. */
        BOOL is_end = (is_auth_delim(**ptr, data->scheme_type != URL_SCHEME_UNKNOWN)
                        || **ptr == ']');

        BOOL is_split = (**ptr == ':');
        BOOL is_elision = (is_split && !is_end && *(*ptr+1) == ':');

        /* Check if we're at the end of a component, or
         * if we're at the end of the IPv6 address.
         */
        if(is_split || is_end) {
            DWORD cur_len = 0;

            cur_len = *ptr - cur_start;

            /* h16 can't have a length > 4. */
            if(cur_len > 4) {
                *ptr = start;

                TRACE("(%p %p %x): h16 component to long.\n",
                    ptr, data, flags);
                return FALSE;
            }

            if(cur_len == 0) {
                /* An h16 component can't have the length of 0 unless
                 * the elision is at the beginning of the address, or
                 * at the end of the address.
                 */
                if(!((*ptr == start && is_elision) ||
                    (is_end && (*ptr-2) == ip.elision))) {
                    *ptr = start;
                    TRACE("(%p %p %x): IPv6 component cannot have a length of 0.\n",
                        ptr, data, flags);
                    return FALSE;
                }
            }

            if(cur_len > 0) {
                /* An IPv6 address can have no more than 8 h16 components. */
                if(ip.h16_count >= 8) {
                    *ptr = start;
                    TRACE("(%p %p %x): Not a IPv6 address, to many h16 components.\n",
                        ptr, data, flags);
                    return FALSE;
                }

                ip.components[ip.h16_count].str = cur_start;
                ip.components[ip.h16_count].len = cur_len;

                TRACE("(%p %p %x): Found h16 component %s, len=%d, h16_count=%d\n",
                    ptr, data, flags, debugstr_wn(cur_start, cur_len), cur_len,
                    ip.h16_count);
                ++ip.h16_count;
            }
        }

        if(is_end)
            break;

        if(is_elision) {
            /* A IPv6 address can only have 1 elision ('::'). */
            if(ip.elision) {
                *ptr = start;

                TRACE("(%p %p %x): IPv6 address cannot have 2 elisions.\n",
                    ptr, data, flags);
                return FALSE;
            }

            ip.elision = *ptr;
            ++(*ptr);
        }

        if(is_split)
            cur_start = *ptr+1;
        else {
            if(!check_ipv4address(ptr, TRUE)) {
                if(!is_hexdigit(**ptr)) {
                    /* Not a valid character for an IPv6 address. */
                    *ptr = start;
                    return FALSE;
                }
            } else {
                /* Found an IPv4 address. */
                ip.ipv4 = cur_start;
                ip.ipv4_len = *ptr - cur_start;

                TRACE("(%p %p %x): Found an attached IPv4 address %s len=%d.\n",
                    ptr, data, flags, debugstr_wn(ip.ipv4, ip.ipv4_len),
                    ip.ipv4_len);

                /* IPv4 addresses can only appear at the end of a IPv6. */
                break;
            }
        }
    }

    compute_ipv6_comps_size(&ip);

    /* Make sure the IPv6 address adds up to 16 bytes. */
    if(ip.components_size + ip.elision_size != 16) {
        *ptr = start;
        TRACE("(%p %p %x): Invalid IPv6 address, did not add up to 16 bytes.\n",
            ptr, data, flags);
        return FALSE;
    }

    if(ip.elision_size == 2) {
        /* For some reason on Windows if an elision that represents
         * only 1 h16 component is encountered at the very begin or
         * end of an IPv6 address, Windows does not consider it a
         * valid IPv6 address.
         *
         *  Ex: [::2:3:4:5:6:7] is not valid, even though the sum
         *      of all the components == 128bits.
         */
         if(ip.elision < ip.components[0].str ||
            ip.elision > ip.components[ip.h16_count-1].str) {
            *ptr = start;
            TRACE("(%p %p %x): Invalid IPv6 address. Detected elision of 2 bytes at the beginning or end of the address.\n",
                ptr, data, flags);
            return FALSE;
        }
    }

    data->host_type = Uri_HOST_IPV6;
    data->has_ipv6 = TRUE;
    data->ipv6_address = ip;

    TRACE("(%p %p %x): Found valid IPv6 literal %s len=%d\n",
        ptr, data, flags, debugstr_wn(start, *ptr-start),
        *ptr-start);
    return TRUE;
}

/*  IPvFuture  = "v" 1*HEXDIG "." 1*( unreserved / sub-delims / ":" ) */
static BOOL parse_ipvfuture(const WCHAR **ptr, parse_data *data, DWORD flags) {
    const WCHAR *start = *ptr;

    /* IPvFuture has to start with a 'v' or 'V'. */
    if(**ptr != 'v' && **ptr != 'V')
        return FALSE;

    /* Following the v there must be at least 1 hex digit. */
    ++(*ptr);
    if(!is_hexdigit(**ptr)) {
        *ptr = start;
        return FALSE;
    }

    ++(*ptr);
    while(is_hexdigit(**ptr))
        ++(*ptr);

    /* End of the hexdigit sequence must be a '.' */
    if(**ptr != '.') {
        *ptr = start;
        return FALSE;
    }

    ++(*ptr);
    if(!is_unreserved(**ptr) && !is_subdelim(**ptr) && **ptr != ':') {
        *ptr = start;
        return FALSE;
    }

    ++(*ptr);
    while(is_unreserved(**ptr) || is_subdelim(**ptr) || **ptr == ':')
        ++(*ptr);

    data->host_type = Uri_HOST_UNKNOWN;

    TRACE("(%p %p %x): Parsed IPvFuture address %s len=%d\n", ptr, data, flags,
        debugstr_wn(start, *ptr-start), *ptr-start);

    return TRUE;
}

/* IP-literal = "[" ( IPv6address / IPvFuture  ) "]" */
static BOOL parse_ip_literal(const WCHAR **ptr, parse_data *data, DWORD flags, DWORD extras) {
    data->host = *ptr;

    if(**ptr != '[' && !(extras & ALLOW_BRACKETLESS_IP_LITERAL)) {
        data->host = NULL;
        return FALSE;
    } else if(**ptr == '[')
        ++(*ptr);

    if(!parse_ipv6address(ptr, data, flags)) {
        if(extras & SKIP_IP_FUTURE_CHECK || !parse_ipvfuture(ptr, data, flags)) {
            *ptr = data->host;
            data->host = NULL;
            return FALSE;
        }
    }

    if(**ptr != ']' && !(extras & ALLOW_BRACKETLESS_IP_LITERAL)) {
        *ptr = data->host;
        data->host = NULL;
        return FALSE;
    } else if(!**ptr && extras & ALLOW_BRACKETLESS_IP_LITERAL) {
        /* The IP literal didn't contain brackets and was followed by
         * a NULL terminator, so no reason to even check the port.
         */
        data->host_len = *ptr - data->host;
        return TRUE;
    }

    ++(*ptr);
    if(**ptr == ':') {
        ++(*ptr);
        /* If a valid port is not found, then let it trickle down to
         * parse_reg_name.
         */
        if(!parse_port(ptr, data, flags)) {
            *ptr = data->host;
            data->host = NULL;
            return FALSE;
        }
    } else
        data->host_len = *ptr - data->host;

    return TRUE;
}

/* Parses the host information from the URI.
 *
 * host = IP-literal / IPv4address / reg-name
 */
static BOOL parse_host(const WCHAR **ptr, parse_data *data, DWORD flags, DWORD extras) {
    if(!parse_ip_literal(ptr, data, flags, extras)) {
        if(!parse_ipv4address(ptr, data, flags)) {
            if(!parse_reg_name(ptr, data, flags, extras)) {
                TRACE("(%p %p %x %x): Malformed URI, Unknown host type.\n",
                    ptr, data, flags, extras);
                return FALSE;
            }
        }
    }

    return TRUE;
}

/* Parses the authority information from the URI.
 *
 * authority   = [ userinfo "@" ] host [ ":" port ]
 */
static BOOL parse_authority(const WCHAR **ptr, parse_data *data, DWORD flags) {
    parse_userinfo(ptr, data, flags);

    /* Parsing the port will happen during one of the host parsing
     * routines (if the URI has a port).
     */
    if(!parse_host(ptr, data, flags, 0))
        return FALSE;

    return TRUE;
}

/* Attempts to parse the path information of a hierarchical URI. */
static BOOL parse_path_hierarchical(const WCHAR **ptr, parse_data *data, DWORD flags) {
    const WCHAR *start = *ptr;
    static const WCHAR slash[] = {'/',0};
    const BOOL is_file = data->scheme_type == URL_SCHEME_FILE;

    if(is_path_delim(**ptr)) {
        if(data->scheme_type == URL_SCHEME_WILDCARD) {
            /* Wildcard schemes don't get a '/' attached if their path is
             * empty.
             */
            data->path = NULL;
            data->path_len = 0;
        } else if(!(flags & Uri_CREATE_NO_CANONICALIZE)) {
            /* If the path component is empty, then a '/' is added. */
            data->path = slash;
            data->path_len = 1;
        }
    } else {
        while(!is_path_delim(**ptr)) {
            if(**ptr == '%' && data->scheme_type != URL_SCHEME_UNKNOWN && !is_file) {
                if(!check_pct_encoded(ptr)) {
                    *ptr = start;
                    return FALSE;
                } else
                    continue;
            } else if(is_forbidden_dos_path_char(**ptr) && is_file &&
                      (flags & Uri_CREATE_FILE_USE_DOS_PATH)) {
                /* File schemes with USE_DOS_PATH set aren't allowed to have
                 * a '<' or '>' or '\"' appear in them.
                 */
                *ptr = start;
                return FALSE;
            } else if(**ptr == '\\') {
                /* Not allowed to have a backslash if NO_CANONICALIZE is set
                 * and the scheme is known type (but not a file scheme).
                 */
                if(flags & Uri_CREATE_NO_CANONICALIZE) {
                    if(data->scheme_type != URL_SCHEME_FILE &&
                       data->scheme_type != URL_SCHEME_UNKNOWN) {
                        *ptr = start;
                        return FALSE;
                    }
                }
            }

            ++(*ptr);
        }

        /* The only time a URI doesn't have a path is when
         * the NO_CANONICALIZE flag is set and the raw URI
         * didn't contain one.
         */
        if(*ptr == start) {
            data->path = NULL;
            data->path_len = 0;
        } else {
            data->path = start;
            data->path_len = *ptr - start;
        }
    }

    if(data->path)
        TRACE("(%p %p %x): Parsed path %s len=%d\n", ptr, data, flags,
            debugstr_wn(data->path, data->path_len), data->path_len);
    else
        TRACE("(%p %p %x): The URI contained no path\n", ptr, data, flags);

    return TRUE;
}

/* Parses the path of a opaque URI (much less strict then the parser
 * for a hierarchical URI).
 *
 * NOTE:
 *  Windows allows invalid % encoded data to appear in opaque URI paths
 *  for unknown scheme types.
 *
 *  File schemes with USE_DOS_PATH set aren't allowed to have '<', '>', or '\"'
 *  appear in them.
 */
static BOOL parse_path_opaque(const WCHAR **ptr, parse_data *data, DWORD flags) {
    const BOOL known_scheme = data->scheme_type != URL_SCHEME_UNKNOWN;
    const BOOL is_file = data->scheme_type == URL_SCHEME_FILE;

    data->path = *ptr;

    while(!is_path_delim(**ptr)) {
        if(**ptr == '%' && known_scheme) {
            if(!check_pct_encoded(ptr)) {
                *ptr = data->path;
                data->path = NULL;
                return FALSE;
            } else
                continue;
        } else if(is_forbidden_dos_path_char(**ptr) && is_file &&
                  (flags & Uri_CREATE_FILE_USE_DOS_PATH)) {
            *ptr = data->path;
            data->path = NULL;
            return FALSE;
        }

        ++(*ptr);
    }

    data->path_len = *ptr - data->path;
    TRACE("(%p %p %x): Parsed opaque URI path %s len=%d\n", ptr, data, flags,
        debugstr_wn(data->path, data->path_len), data->path_len);
    return TRUE;
}

/* Determines how the URI should be parsed after the scheme information.
 *
 * If the scheme is followed, by "//" then, it is treated as an hierarchical URI
 * which then the authority and path information will be parsed out. Otherwise, the
 * URI will be treated as an opaque URI which the authority information is not parsed
 * out.
 *
 * RFC 3896 definition of hier-part:
 *
 * hier-part   = "//" authority path-abempty
 *                 / path-absolute
 *                 / path-rootless
 *                 / path-empty
 *
 * MSDN opaque URI definition:
 *  scheme ":" path [ "#" fragment ]
 *
 * NOTES:
 *  If the URI is of an unknown scheme type and has a "//" following the scheme then it
 *  is treated as a hierarchical URI, but, if the CREATE_NO_CRACK_UNKNOWN_SCHEMES flag is
 *  set then it is considered an opaque URI reguardless of what follows the scheme information
 *  (per MSDN documentation).
 */
static BOOL parse_hierpart(const WCHAR **ptr, parse_data *data, DWORD flags) {
    const WCHAR *start = *ptr;

    /* Checks if the authority information needs to be parsed. */
    if(is_hierarchical_uri(ptr, data)) {
        /* Only treat it as a hierarchical URI if the scheme_type is known or
         * the Uri_CREATE_NO_CRACK_UNKNOWN_SCHEMES flag is not set.
         */
        if(data->scheme_type != URL_SCHEME_UNKNOWN ||
           !(flags & Uri_CREATE_NO_CRACK_UNKNOWN_SCHEMES)) {
            TRACE("(%p %p %x): Treating URI as an hierarchical URI.\n", ptr, data, flags);
            data->is_opaque = FALSE;

            /* TODO: Handle hierarchical URI's, parse authority then parse the path. */
            if(!parse_authority(ptr, data, flags))
                return FALSE;

            return parse_path_hierarchical(ptr, data, flags);
        } else
            /* Reset ptr to it's starting position so opaque path parsing
             * begins at the correct location.
             */
            *ptr = start;
    }

    /* If it reaches here, then the URI will be treated as an opaque
     * URI.
     */

    TRACE("(%p %p %x): Treating URI as an opaque URI.\n", ptr, data, flags);

    data->is_opaque = TRUE;
    if(!parse_path_opaque(ptr, data, flags))
        return FALSE;

    return TRUE;
}

/* Attempts to parse the query string from the URI.
 *
 * NOTES:
 *  If NO_DECODE_EXTRA_INFO flag is set, then invalid percent encoded
 *  data is allowed appear in the query string. For unknown scheme types
 *  invalid percent encoded data is allowed to appear reguardless.
 */
static BOOL parse_query(const WCHAR **ptr, parse_data *data, DWORD flags) {
    const BOOL known_scheme = data->scheme_type != URL_SCHEME_UNKNOWN;

    if(**ptr != '?') {
        TRACE("(%p %p %x): URI didn't contain a query string.\n", ptr, data, flags);
        return TRUE;
    }

    data->query = *ptr;

    ++(*ptr);
    while(**ptr && **ptr != '#') {
        if(**ptr == '%' && known_scheme &&
           !(flags & Uri_CREATE_NO_DECODE_EXTRA_INFO)) {
            if(!check_pct_encoded(ptr)) {
                *ptr = data->query;
                data->query = NULL;
                return FALSE;
            } else
                continue;
        }

        ++(*ptr);
    }

    data->query_len = *ptr - data->query;

    TRACE("(%p %p %x): Parsed query string %s len=%d\n", ptr, data, flags,
        debugstr_wn(data->query, data->query_len), data->query_len);
    return TRUE;
}

/* Attempts to parse the fragment from the URI.
 *
 * NOTES:
 *  If NO_DECODE_EXTRA_INFO flag is set, then invalid percent encoded
 *  data is allowed appear in the query string. For unknown scheme types
 *  invalid percent encoded data is allowed to appear reguardless.
 */
static BOOL parse_fragment(const WCHAR **ptr, parse_data *data, DWORD flags) {
    const BOOL known_scheme = data->scheme_type != URL_SCHEME_UNKNOWN;

    if(**ptr != '#') {
        TRACE("(%p %p %x): URI didn't contain a fragment.\n", ptr, data, flags);
        return TRUE;
    }

    data->fragment = *ptr;

    ++(*ptr);
    while(**ptr) {
        if(**ptr == '%' && known_scheme &&
           !(flags & Uri_CREATE_NO_DECODE_EXTRA_INFO)) {
            if(!check_pct_encoded(ptr)) {
                *ptr = data->fragment;
                data->fragment = NULL;
                return FALSE;
            } else
                continue;
        }

        ++(*ptr);
    }

    data->fragment_len = *ptr - data->fragment;

    TRACE("(%p %p %x): Parsed fragment %s len=%d\n", ptr, data, flags,
        debugstr_wn(data->fragment, data->fragment_len), data->fragment_len);
    return TRUE;
}

/* Parses and validates the components of the specified by data->uri
 * and stores the information it parses into 'data'.
 *
 * Returns TRUE if it successfully parsed the URI. False otherwise.
 */
static BOOL parse_uri(parse_data *data, DWORD flags) {
    const WCHAR *ptr;
    const WCHAR **pptr;

    ptr = data->uri;
    pptr = &ptr;

    TRACE("(%p %x): BEGINNING TO PARSE URI %s.\n", data, flags, debugstr_w(data->uri));

    if(!parse_scheme(pptr, data, flags, 0))
        return FALSE;

    if(!parse_hierpart(pptr, data, flags))
        return FALSE;

    if(!parse_query(pptr, data, flags))
        return FALSE;

    if(!parse_fragment(pptr, data, flags))
        return FALSE;

    TRACE("(%p %x): FINISHED PARSING URI.\n", data, flags);
    return TRUE;
}

static BOOL canonicalize_username(const parse_data *data, Uri *uri, DWORD flags, BOOL computeOnly) {
    const WCHAR *ptr;

    if(!data->username) {
        uri->userinfo_start = -1;
        return TRUE;
    }

    uri->userinfo_start = uri->canon_len;
    for(ptr = data->username; ptr < data->username+data->username_len; ++ptr) {
        if(*ptr == '%') {
            /* Only decode % encoded values for known scheme types. */
            if(data->scheme_type != URL_SCHEME_UNKNOWN) {
                /* See if the value really needs decoded. */
                WCHAR val = decode_pct_val(ptr);
                if(is_unreserved(val)) {
                    if(!computeOnly)
                        uri->canon_uri[uri->canon_len] = val;

                    ++uri->canon_len;

                    /* Move pass the hex characters. */
                    ptr += 2;
                    continue;
                }
            }
        } else if(!is_reserved(*ptr) && !is_unreserved(*ptr) && *ptr != '\\') {
            /* Only percent encode forbidden characters if the NO_ENCODE_FORBIDDEN_CHARACTERS flag
             * is NOT set.
             */
            if(!(flags & Uri_CREATE_NO_ENCODE_FORBIDDEN_CHARACTERS)) {
                if(!computeOnly)
                    pct_encode_val(*ptr, uri->canon_uri + uri->canon_len);

                uri->canon_len += 3;
                continue;
            }
        }

        if(!computeOnly)
            /* Nothing special, so just copy the character over. */
            uri->canon_uri[uri->canon_len] = *ptr;
        ++uri->canon_len;
    }

    return TRUE;
}

static BOOL canonicalize_password(const parse_data *data, Uri *uri, DWORD flags, BOOL computeOnly) {
    const WCHAR *ptr;

    if(!data->password) {
        uri->userinfo_split = -1;
        return TRUE;
    }

    if(uri->userinfo_start == -1)
        /* Has a password, but, doesn't have a username. */
        uri->userinfo_start = uri->canon_len;

    uri->userinfo_split = uri->canon_len - uri->userinfo_start;

    /* Add the ':' to the userinfo component. */
    if(!computeOnly)
        uri->canon_uri[uri->canon_len] = ':';
    ++uri->canon_len;

    for(ptr = data->password; ptr < data->password+data->password_len; ++ptr) {
        if(*ptr == '%') {
            /* Only decode % encoded values for known scheme types. */
            if(data->scheme_type != URL_SCHEME_UNKNOWN) {
                /* See if the value really needs decoded. */
                WCHAR val = decode_pct_val(ptr);
                if(is_unreserved(val)) {
                    if(!computeOnly)
                        uri->canon_uri[uri->canon_len] = val;

                    ++uri->canon_len;

                    /* Move pass the hex characters. */
                    ptr += 2;
                    continue;
                }
            }
        } else if(!is_reserved(*ptr) && !is_unreserved(*ptr) && *ptr != '\\') {
            /* Only percent encode forbidden characters if the NO_ENCODE_FORBIDDEN_CHARACTERS flag
             * is NOT set.
             */
            if(!(flags & Uri_CREATE_NO_ENCODE_FORBIDDEN_CHARACTERS)) {
                if(!computeOnly)
                    pct_encode_val(*ptr, uri->canon_uri + uri->canon_len);

                uri->canon_len += 3;
                continue;
            }
        }

        if(!computeOnly)
            /* Nothing special, so just copy the character over. */
            uri->canon_uri[uri->canon_len] = *ptr;
        ++uri->canon_len;
    }

    return TRUE;
}

/* Canonicalizes the userinfo of the URI represented by the parse_data.
 *
 * Canonicalization of the userinfo is a simple process. If there are any percent
 * encoded characters that fall in the "unreserved" character set, they are decoded
 * to their actual value. If a character is not in the "unreserved" or "reserved" sets
 * then it is percent encoded. Other than that the characters are copied over without
 * change.
 */
static BOOL canonicalize_userinfo(const parse_data *data, Uri *uri, DWORD flags, BOOL computeOnly) {
    uri->userinfo_start = uri->userinfo_split = -1;
    uri->userinfo_len = 0;

    if(!data->username && !data->password)
        /* URI doesn't have userinfo, so nothing to do here. */
        return TRUE;

    if(!canonicalize_username(data, uri, flags, computeOnly))
        return FALSE;

    if(!canonicalize_password(data, uri, flags, computeOnly))
        return FALSE;

    uri->userinfo_len = uri->canon_len - uri->userinfo_start;
    if(!computeOnly)
        TRACE("(%p %p %x %d): Canonicalized userinfo, userinfo_start=%d, userinfo=%s, userinfo_split=%d userinfo_len=%d.\n",
                data, uri, flags, computeOnly, uri->userinfo_start, debugstr_wn(uri->canon_uri + uri->userinfo_start, uri->userinfo_len),
                uri->userinfo_split, uri->userinfo_len);

    /* Now insert the '@' after the userinfo. */
    if(!computeOnly)
        uri->canon_uri[uri->canon_len] = '@';
    ++uri->canon_len;

    return TRUE;
}

/* Attempts to canonicalize a reg_name.
 *
 * Things that happen:
 *  1)  If Uri_CREATE_NO_CANONICALIZE flag is not set, then the reg_name is
 *      lower cased. Unless it's an unknown scheme type, which case it's
 *      no lower cased reguardless.
 *
 *  2)  Unreserved % encoded characters are decoded for known
 *      scheme types.
 *
 *  3)  Forbidden characters are % encoded as long as
 *      Uri_CREATE_NO_ENCODE_FORBIDDEN_CHARACTERS flag is not set and
 *      it isn't an unknown scheme type.
 *
 *  4)  If it's a file scheme and the host is "localhost" it's removed.
 *
 *  5)  If it's a file scheme and Uri_CREATE_FILE_USE_DOS_PATH is set,
 *      then the UNC path characters are added before the host name.
 */
static BOOL canonicalize_reg_name(const parse_data *data, Uri *uri,
                                  DWORD flags, BOOL computeOnly) {
    static const WCHAR localhostW[] =
            {'l','o','c','a','l','h','o','s','t',0};
    const WCHAR *ptr;
    const BOOL known_scheme = data->scheme_type != URL_SCHEME_UNKNOWN;

    if(data->scheme_type == URL_SCHEME_FILE &&
       data->host_len == lstrlenW(localhostW)) {
        if(!StrCmpNIW(data->host, localhostW, data->host_len)) {
            uri->host_start = -1;
            uri->host_len = 0;
            uri->host_type = Uri_HOST_UNKNOWN;
            return TRUE;
        }
    }

    if(data->scheme_type == URL_SCHEME_FILE && flags & Uri_CREATE_FILE_USE_DOS_PATH) {
        if(!computeOnly) {
            uri->canon_uri[uri->canon_len] = '\\';
            uri->canon_uri[uri->canon_len+1] = '\\';
        }
        uri->canon_len += 2;
        uri->authority_start = uri->canon_len;
    }

    uri->host_start = uri->canon_len;

    for(ptr = data->host; ptr < data->host+data->host_len; ++ptr) {
        if(*ptr == '%' && known_scheme) {
            WCHAR val = decode_pct_val(ptr);
            if(is_unreserved(val)) {
                /* If NO_CANONICALZE is not set, then windows lower cases the
                 * decoded value.
                 */
                if(!(flags & Uri_CREATE_NO_CANONICALIZE) && isupperW(val)) {
                    if(!computeOnly)
                        uri->canon_uri[uri->canon_len] = tolowerW(val);
                } else {
                    if(!computeOnly)
                        uri->canon_uri[uri->canon_len] = val;
                }
                ++uri->canon_len;

                /* Skip past the % encoded character. */
                ptr += 2;
                continue;
            } else {
                /* Just copy the % over. */
                if(!computeOnly)
                    uri->canon_uri[uri->canon_len] = *ptr;
                ++uri->canon_len;
            }
        } else if(*ptr == '\\') {
            /* Only unknown scheme types could have made it here with a '\\' in the host name. */
            if(!computeOnly)
                uri->canon_uri[uri->canon_len] = *ptr;
            ++uri->canon_len;
        } else if(!(flags & Uri_CREATE_NO_ENCODE_FORBIDDEN_CHARACTERS) &&
                  !is_unreserved(*ptr) && !is_reserved(*ptr) && known_scheme) {
            if(!computeOnly) {
                pct_encode_val(*ptr, uri->canon_uri+uri->canon_len);

                /* The percent encoded value gets lower cased also. */
                if(!(flags & Uri_CREATE_NO_CANONICALIZE)) {
                    uri->canon_uri[uri->canon_len+1] = tolowerW(uri->canon_uri[uri->canon_len+1]);
                    uri->canon_uri[uri->canon_len+2] = tolowerW(uri->canon_uri[uri->canon_len+2]);
                }
            }

            uri->canon_len += 3;
        } else {
            if(!computeOnly) {
                if(!(flags & Uri_CREATE_NO_CANONICALIZE) && known_scheme)
                    uri->canon_uri[uri->canon_len] = tolowerW(*ptr);
                else
                    uri->canon_uri[uri->canon_len] = *ptr;
            }

            ++uri->canon_len;
        }
    }

    uri->host_len = uri->canon_len - uri->host_start;

    if(!computeOnly)
        TRACE("(%p %p %x %d): Canonicalize reg_name=%s len=%d\n", data, uri, flags,
            computeOnly, debugstr_wn(uri->canon_uri+uri->host_start, uri->host_len),
            uri->host_len);

    if(!computeOnly)
        find_domain_name(uri->canon_uri+uri->host_start, uri->host_len,
            &(uri->domain_offset));

    return TRUE;
}

/* Attempts to canonicalize an implicit IPv4 address. */
static BOOL canonicalize_implicit_ipv4address(const parse_data *data, Uri *uri, DWORD flags, BOOL computeOnly) {
    uri->host_start = uri->canon_len;

    TRACE("%u\n", data->implicit_ipv4);
    /* For unknown scheme types Window's doesn't convert
     * the value into an IP address, but, it still considers
     * it an IPv4 address.
     */
    if(data->scheme_type == URL_SCHEME_UNKNOWN) {
        if(!computeOnly)
            memcpy(uri->canon_uri+uri->canon_len, data->host, data->host_len*sizeof(WCHAR));
        uri->canon_len += data->host_len;
    } else {
        if(!computeOnly)
            uri->canon_len += ui2ipv4(uri->canon_uri+uri->canon_len, data->implicit_ipv4);
        else
            uri->canon_len += ui2ipv4(NULL, data->implicit_ipv4);
    }

    uri->host_len = uri->canon_len - uri->host_start;
    uri->host_type = Uri_HOST_IPV4;

    if(!computeOnly)
        TRACE("%p %p %x %d): Canonicalized implicit IP address=%s len=%d\n",
            data, uri, flags, computeOnly,
            debugstr_wn(uri->canon_uri+uri->host_start, uri->host_len),
            uri->host_len);

    return TRUE;
}

/* Attempts to canonicalize an IPv4 address.
 *
 * If the parse_data represents a URI that has an implicit IPv4 address
 * (ex. http://256/, this function will convert 256 into 0.0.1.0). If
 * the implicit IP address exceeds the value of UINT_MAX (maximum value
 * for an IPv4 address) it's canonicalized as if were a reg-name.
 *
 * If the parse_data contains a partial or full IPv4 address it normalizes it.
 * A partial IPv4 address is something like "192.0" and would be normalized to
 * "192.0.0.0". With a full (or partial) IPv4 address like "192.002.01.003" would
 * be normalized to "192.2.1.3".
 *
 * NOTES:
 *  Window's ONLY normalizes IPv4 address for known scheme types (one that isn't
 *  URL_SCHEME_UNKNOWN). For unknown scheme types, it simply copies the data from
 *  the original URI into the canonicalized URI, but, it still recognizes URI's
 *  host type as HOST_IPV4.
 */
static BOOL canonicalize_ipv4address(const parse_data *data, Uri *uri, DWORD flags, BOOL computeOnly) {
    if(data->has_implicit_ip)
        return canonicalize_implicit_ipv4address(data, uri, flags, computeOnly);
    else {
        uri->host_start = uri->canon_len;

        /* Windows only normalizes for known scheme types. */
        if(data->scheme_type != URL_SCHEME_UNKNOWN) {
            /* parse_data contains a partial or full IPv4 address, so normalize it. */
            DWORD i, octetDigitCount = 0, octetCount = 0;
            BOOL octetHasDigit = FALSE;

            for(i = 0; i < data->host_len; ++i) {
                if(data->host[i] == '0' && !octetHasDigit) {
                    /* Can ignore leading zeros if:
                     *  1) It isn't the last digit of the octet.
                     *  2) i+1 != data->host_len
                     *  3) i+1 != '.'
                     */
                    if(octetDigitCount == 2 ||
                       i+1 == data->host_len ||
                       data->host[i+1] == '.') {
                        if(!computeOnly)
                            uri->canon_uri[uri->canon_len] = data->host[i];
                        ++uri->canon_len;
                        TRACE("Adding zero\n");
                    }
                } else if(data->host[i] == '.') {
                    if(!computeOnly)
                        uri->canon_uri[uri->canon_len] = data->host[i];
                    ++uri->canon_len;

                    octetDigitCount = 0;
                    octetHasDigit = FALSE;
                    ++octetCount;
                } else {
                    if(!computeOnly)
                        uri->canon_uri[uri->canon_len] = data->host[i];
                    ++uri->canon_len;

                    ++octetDigitCount;
                    octetHasDigit = TRUE;
                }
            }

            /* Make sure the canonicalized IP address has 4 dec-octets.
             * If doesn't add "0" ones until there is 4;
             */
            for( ; octetCount < 3; ++octetCount) {
                if(!computeOnly) {
                    uri->canon_uri[uri->canon_len] = '.';
                    uri->canon_uri[uri->canon_len+1] = '0';
                }

                uri->canon_len += 2;
            }
        } else {
            /* Windows doesn't normalize addresses in unknown schemes. */
            if(!computeOnly)
                memcpy(uri->canon_uri+uri->canon_len, data->host, data->host_len*sizeof(WCHAR));
            uri->canon_len += data->host_len;
        }

        uri->host_len = uri->canon_len - uri->host_start;
        if(!computeOnly)
            TRACE("(%p %p %x %d): Canonicalized IPv4 address, ip=%s len=%d\n",
                data, uri, flags, computeOnly,
                debugstr_wn(uri->canon_uri+uri->host_start, uri->host_len),
                uri->host_len);
    }

    return TRUE;
}

/* Attempts to canonicalize the IPv6 address of the URI.
 *
 * Multiple things happen during the canonicalization of an IPv6 address:
 *  1)  Any leading zero's in an h16 component are removed.
 *      Ex: [0001:0022::] -> [1:22::]
 *
 *  2)  The longest sequence of zero h16 components are compressed
 *      into a "::" (elision). If there's a tie, the first is choosen.
 *
 *      Ex: [0:0:0:0:1:6:7:8]   -> [::1:6:7:8]
 *          [0:0:0:0:1:2::]     -> [::1:2:0:0]
 *          [0:0:1:2:0:0:7:8]   -> [::1:2:0:0:7:8]
 *
 *  3)  If an IPv4 address is attached to the IPv6 address, it's
 *      also normalized.
 *      Ex: [::001.002.022.000] -> [::1.2.22.0]
 *
 *  4)  If an elision is present, but, only represents 1 h16 component
 *      it's expanded.
 *
 *      Ex: [1::2:3:4:5:6:7] -> [1:0:2:3:4:5:6:7]
 *
 *  5)  If the IPv6 address contains an IPv4 address and there exists
 *      at least 1 non-zero h16 component the IPv4 address is converted
 *      into two h16 components, otherwise it's normalized and kept as is.
 *
 *      Ex: [::192.200.003.4]       -> [::192.200.3.4]
 *          [ffff::192.200.003.4]   -> [ffff::c0c8:3041]
 *
 * NOTE:
 *  For unknown scheme types Windows simply copies the address over without any
 *  changes.
 *
 *  IPv4 address can be included in an elision if all its components are 0's.
 */
static BOOL canonicalize_ipv6address(const parse_data *data, Uri *uri,
                                     DWORD flags, BOOL computeOnly) {
    uri->host_start = uri->canon_len;

    if(data->scheme_type == URL_SCHEME_UNKNOWN) {
        if(!computeOnly)
            memcpy(uri->canon_uri+uri->canon_len, data->host, data->host_len*sizeof(WCHAR));
        uri->canon_len += data->host_len;
    } else {
        USHORT values[8];
        INT elision_start;
        DWORD i, elision_len;

        if(!ipv6_to_number(&(data->ipv6_address), values)) {
            TRACE("(%p %p %x %d): Failed to compute numerical value for IPv6 address.\n",
                data, uri, flags, computeOnly);
            return FALSE;
        }

        if(!computeOnly)
            uri->canon_uri[uri->canon_len] = '[';
        ++uri->canon_len;

        /* Find where the elision should occur (if any). */
        compute_elision_location(&(data->ipv6_address), values, &elision_start, &elision_len);

        TRACE("%p %p %x %d): Elision starts at %d, len=%u\n", data, uri, flags,
            computeOnly, elision_start, elision_len);

        for(i = 0; i < 8; ++i) {
            BOOL in_elision = (elision_start > -1 && i >= elision_start &&
                               i < elision_start+elision_len);
            BOOL do_ipv4 = (i == 6 && data->ipv6_address.ipv4 && !in_elision &&
                            data->ipv6_address.h16_count == 0);

            if(i == elision_start) {
                if(!computeOnly) {
                    uri->canon_uri[uri->canon_len] = ':';
                    uri->canon_uri[uri->canon_len+1] = ':';
                }
                uri->canon_len += 2;
            }

            /* We can ignore the current component if we're in the elision. */
            if(in_elision)
                continue;

            /* We only add a ':' if we're not at i == 0, or when we're at
             * the very end of elision range since the ':' colon was handled
             * earlier. Otherwise we would end up with ":::" after elision.
             */
            if(i != 0 && !(elision_start > -1 && i == elision_start+elision_len)) {
                if(!computeOnly)
                    uri->canon_uri[uri->canon_len] = ':';
                ++uri->canon_len;
            }

            if(do_ipv4) {
                UINT val;
                DWORD len;

                /* Combine the two parts of the IPv4 address values. */
                val = values[i];
                val <<= 16;
                val += values[i+1];

                if(!computeOnly)
                    len = ui2ipv4(uri->canon_uri+uri->canon_len, val);
                else
                    len = ui2ipv4(NULL, val);

                uri->canon_len += len;
                ++i;
            } else {
                /* Write a regular h16 component to the URI. */

                /* Short circuit for the trivial case. */
                if(values[i] == 0) {
                    if(!computeOnly)
                        uri->canon_uri[uri->canon_len] = '0';
                    ++uri->canon_len;
                } else {
                    static const WCHAR formatW[] = {'%','x',0};

                    if(!computeOnly)
                        uri->canon_len += sprintfW(uri->canon_uri+uri->canon_len,
                                            formatW, values[i]);
                    else {
                        WCHAR tmp[5];
                        uri->canon_len += sprintfW(tmp, formatW, values[i]);
                    }
                }
            }
        }

        /* Add the closing ']'. */
        if(!computeOnly)
            uri->canon_uri[uri->canon_len] = ']';
        ++uri->canon_len;
    }

    uri->host_len = uri->canon_len - uri->host_start;

    if(!computeOnly)
        TRACE("(%p %p %x %d): Canonicalized IPv6 address %s, len=%d\n", data, uri, flags,
            computeOnly, debugstr_wn(uri->canon_uri+uri->host_start, uri->host_len),
            uri->host_len);

    return TRUE;
}

/* Attempts to canonicalize the host of the URI (if any). */
static BOOL canonicalize_host(const parse_data *data, Uri *uri, DWORD flags, BOOL computeOnly) {
    uri->host_start = -1;
    uri->host_len = 0;
    uri->domain_offset = -1;

    if(data->host) {
        switch(data->host_type) {
        case Uri_HOST_DNS:
            uri->host_type = Uri_HOST_DNS;
            if(!canonicalize_reg_name(data, uri, flags, computeOnly))
                return FALSE;

            break;
        case Uri_HOST_IPV4:
            uri->host_type = Uri_HOST_IPV4;
            if(!canonicalize_ipv4address(data, uri, flags, computeOnly))
                return FALSE;

            break;
        case Uri_HOST_IPV6:
            if(!canonicalize_ipv6address(data, uri, flags, computeOnly))
                return FALSE;

            uri->host_type = Uri_HOST_IPV6;
            break;
        case Uri_HOST_UNKNOWN:
            if(data->host_len > 0 || data->scheme_type != URL_SCHEME_FILE) {
                uri->host_start = uri->canon_len;

                /* Nothing happens to unknown host types. */
                if(!computeOnly)
                    memcpy(uri->canon_uri+uri->canon_len, data->host, data->host_len*sizeof(WCHAR));
                uri->canon_len += data->host_len;
                uri->host_len = data->host_len;
            }

            uri->host_type = Uri_HOST_UNKNOWN;
            break;
        default:
            FIXME("(%p %p %x %d): Canonicalization for host type %d not supported.\n", data,
                    uri, flags, computeOnly, data->host_type);
            return FALSE;
       }
   }

   return TRUE;
}

static BOOL canonicalize_port(const parse_data *data, Uri *uri, DWORD flags, BOOL computeOnly) {
    BOOL has_default_port = FALSE;
    USHORT default_port = 0;
    DWORD i;

    uri->port_offset = -1;

    /* Check if the scheme has a default port. */
    for(i = 0; i < sizeof(default_ports)/sizeof(default_ports[0]); ++i) {
        if(default_ports[i].scheme == data->scheme_type) {
            has_default_port = TRUE;
            default_port = default_ports[i].port;
            break;
        }
    }

    uri->has_port = data->has_port || has_default_port;

    /* Possible cases:
     *  1)  Has a port which is the default port.
     *  2)  Has a port (not the default).
     *  3)  Doesn't have a port, but, scheme has a default port.
     *  4)  No port.
     */
    if(has_default_port && data->has_port && data->port_value == default_port) {
        /* If it's the default port and this flag isn't set, don't do anything. */
        if(flags & Uri_CREATE_NO_CANONICALIZE) {
            uri->port_offset = uri->canon_len-uri->authority_start;
            if(!computeOnly)
                uri->canon_uri[uri->canon_len] = ':';
            ++uri->canon_len;

            if(data->port) {
                /* Copy the original port over. */
                if(!computeOnly)
                    memcpy(uri->canon_uri+uri->canon_len, data->port, data->port_len*sizeof(WCHAR));
                uri->canon_len += data->port_len;
            } else {
                if(!computeOnly)
                    uri->canon_len += ui2str(uri->canon_uri+uri->canon_len, data->port_value);
                else
                    uri->canon_len += ui2str(NULL, data->port_value);
            }
        }

        uri->port = default_port;
    } else if(data->has_port) {
        uri->port_offset = uri->canon_len-uri->authority_start;
        if(!computeOnly)
            uri->canon_uri[uri->canon_len] = ':';
        ++uri->canon_len;

        if(flags & Uri_CREATE_NO_CANONICALIZE && data->port) {
            /* Copy the original over without changes. */
            if(!computeOnly)
                memcpy(uri->canon_uri+uri->canon_len, data->port, data->port_len*sizeof(WCHAR));
            uri->canon_len += data->port_len;
        } else {
            if(!computeOnly)
                uri->canon_len += ui2str(uri->canon_uri+uri->canon_len, data->port_value);
            else
                uri->canon_len += ui2str(NULL, data->port_value);
        }

        uri->port = data->port_value;
    } else if(has_default_port)
        uri->port = default_port;

    return TRUE;
}

/* Canonicalizes the authority of the URI represented by the parse_data. */
static BOOL canonicalize_authority(const parse_data *data, Uri *uri, DWORD flags, BOOL computeOnly) {
    uri->authority_start = uri->canon_len;
    uri->authority_len = 0;

    if(!canonicalize_userinfo(data, uri, flags, computeOnly))
        return FALSE;

    if(!canonicalize_host(data, uri, flags, computeOnly))
        return FALSE;

    if(!canonicalize_port(data, uri, flags, computeOnly))
        return FALSE;

    if(uri->host_start != -1 || (data->is_relative && (data->password || data->username)))
        uri->authority_len = uri->canon_len - uri->authority_start;
    else
        uri->authority_start = -1;

    return TRUE;
}

/* Attempts to canonicalize the path of a hierarchical URI.
 *
 * Things that happen:
 *  1). Forbidden characters are percent encoded, unless the NO_ENCODE_FORBIDDEN
 *      flag is set or it's a file URI. Forbidden characters are always encoded
 *      for file schemes reguardless and forbidden characters are never encoded
 *      for unknown scheme types.
 *
 *  2). For known scheme types '\\' are changed to '/'.
 *
 *  3). Percent encoded, unreserved characters are decoded to their actual values.
 *      Unless the scheme type is unknown. For file schemes any percent encoded
 *      character in the unreserved or reserved set is decoded.
 *
 *  4). For File schemes if the path is starts with a drive letter and doesn't
 *      start with a '/' then one is appended.
 *      Ex: file://c:/test.mp3 -> file:///c:/test.mp3
 *
 *  5). Dot segments are removed from the path for all scheme types
 *      unless NO_CANONICALIZE flag is set. Dot segments aren't removed
 *      for wildcard scheme types.
 *
 * NOTES:
 *      file://c:/test%20test   -> file:///c:/test%2520test
 *      file://c:/test%3Etest   -> file:///c:/test%253Etest
 * if Uri_CREATE_FILE_USE_DOS_PATH is not set:
 *      file:///c:/test%20test  -> file:///c:/test%20test
 *      file:///c:/test%test    -> file:///c:/test%25test
 */
static BOOL canonicalize_path_hierarchical(const parse_data *data, Uri *uri,
                                           DWORD flags, BOOL computeOnly) {
    const WCHAR *ptr;
    const BOOL known_scheme = data->scheme_type != URL_SCHEME_UNKNOWN;
    const BOOL is_file = data->scheme_type == URL_SCHEME_FILE;

    BOOL escape_pct = FALSE;

    if(!data->path) {
        uri->path_start = -1;
        uri->path_len = 0;
        return TRUE;
    }

    uri->path_start = uri->canon_len;
    ptr = data->path;

    if(is_file && uri->host_start == -1) {
        /* Check if a '/' needs to be appended for the file scheme. */
        if(data->path_len > 1 && is_drive_path(ptr) && !(flags & Uri_CREATE_FILE_USE_DOS_PATH)) {
            if(!computeOnly)
                uri->canon_uri[uri->canon_len] = '/';
            uri->canon_len++;
            escape_pct = TRUE;
        } else if(*ptr == '/') {
            if(!(flags & Uri_CREATE_FILE_USE_DOS_PATH)) {
                /* Copy the extra '/' over. */
                if(!computeOnly)
                    uri->canon_uri[uri->canon_len] = '/';
                ++uri->canon_len;
            }
            ++ptr;
        }

        if(is_drive_path(ptr)) {
            if(!computeOnly) {
                uri->canon_uri[uri->canon_len] = *ptr;
                /* If theres a '|' after the drive letter, convert it to a ':'. */
                uri->canon_uri[uri->canon_len+1] = ':';
            }
            ptr += 2;
            uri->canon_len += 2;
        }
    }

    if(!is_file && *(data->path) && *(data->path) != '/') {
        /* Prepend a '/' to the path if it doesn't have one. */
        if(!computeOnly)
            uri->canon_uri[uri->canon_len] = '/';
        ++uri->canon_len;
    }

    for(; ptr < data->path+data->path_len; ++ptr) {
        if(*ptr == '%') {
            const WCHAR *tmp = ptr;
            WCHAR val;

            /* Check if the % represents a valid encoded char, or if it needs encoded. */
            BOOL force_encode = !check_pct_encoded(&tmp) && is_file && !(flags&Uri_CREATE_FILE_USE_DOS_PATH);
            val = decode_pct_val(ptr);

            if(force_encode || escape_pct) {
                /* Escape the percent sign in the file URI. */
                if(!computeOnly)
                    pct_encode_val(*ptr, uri->canon_uri+uri->canon_len);
                uri->canon_len += 3;
            } else if((is_unreserved(val) && known_scheme) ||
                      (is_file && (is_unreserved(val) || is_reserved(val) ||
                      (val && flags&Uri_CREATE_FILE_USE_DOS_PATH && !is_forbidden_dos_path_char(val))))) {
                if(!computeOnly)
                    uri->canon_uri[uri->canon_len] = val;
                ++uri->canon_len;

                ptr += 2;
                continue;
            } else {
                if(!computeOnly)
                    uri->canon_uri[uri->canon_len] = *ptr;
                ++uri->canon_len;
            }
        } else if(*ptr == '/' && is_file && (flags & Uri_CREATE_FILE_USE_DOS_PATH)) {
            /* Convert the '/' back to a '\\'. */
            if(!computeOnly)
                uri->canon_uri[uri->canon_len] = '\\';
            ++uri->canon_len;
        } else if(*ptr == '\\' && known_scheme) {
            if(is_file && (flags & Uri_CREATE_FILE_USE_DOS_PATH)) {
                /* Don't convert the '\\' to a '/'. */
                if(!computeOnly)
                    uri->canon_uri[uri->canon_len] = *ptr;
                ++uri->canon_len;
            } else {
                if(!computeOnly)
                    uri->canon_uri[uri->canon_len] = '/';
                ++uri->canon_len;
            }
        } else if(known_scheme && !is_unreserved(*ptr) && !is_reserved(*ptr) &&
                  (!(flags & Uri_CREATE_NO_ENCODE_FORBIDDEN_CHARACTERS) || is_file)) {
            if(is_file && (flags & Uri_CREATE_FILE_USE_DOS_PATH)) {
                /* Don't escape the character. */
                if(!computeOnly)
                    uri->canon_uri[uri->canon_len] = *ptr;
                ++uri->canon_len;
            } else {
                /* Escape the forbidden character. */
                if(!computeOnly)
                    pct_encode_val(*ptr, uri->canon_uri+uri->canon_len);
                uri->canon_len += 3;
            }
        } else {
            if(!computeOnly)
                uri->canon_uri[uri->canon_len] = *ptr;
            ++uri->canon_len;
        }
    }

    uri->path_len = uri->canon_len - uri->path_start;

    /* Removing the dot segments only happens when it's not in
     * computeOnly mode and it's not a wildcard scheme. File schemes
     * with USE_DOS_PATH set don't get dot segments removed.
     */
    if(!(is_file && (flags & Uri_CREATE_FILE_USE_DOS_PATH)) &&
       data->scheme_type != URL_SCHEME_WILDCARD) {
        if(!(flags & Uri_CREATE_NO_CANONICALIZE) && !computeOnly) {
            /* Remove the dot segments (if any) and reset everything to the new
             * correct length.
             */
            DWORD new_len = remove_dot_segments(uri->canon_uri+uri->path_start, uri->path_len);
            uri->canon_len -= uri->path_len-new_len;
            uri->path_len = new_len;
        }
    }

    if(!computeOnly)
        TRACE("Canonicalized path %s len=%d\n",
            debugstr_wn(uri->canon_uri+uri->path_start, uri->path_len),
            uri->path_len);

    return TRUE;
}

/* Attempts to canonicalize the path for an opaque URI.
 *
 * For known scheme types:
 *  1)  forbidden characters are percent encoded if
 *      NO_ENCODE_FORBIDDEN_CHARACTERS isn't set.
 *
 *  2)  Percent encoded, unreserved characters are decoded
 *      to their actual values, for known scheme types.
 *
 *  3)  '\\' are changed to '/' for known scheme types
 *      except for mailto schemes.
 *
 *  4)  For file schemes, if USE_DOS_PATH is set all '/'
 *      are converted to backslashes.
 *
 *  5)  For file schemes, if USE_DOS_PATH isn't set all '\'
 *      are converted to forward slashes.
 */
static BOOL canonicalize_path_opaque(const parse_data *data, Uri *uri, DWORD flags, BOOL computeOnly) {
    const WCHAR *ptr;
    const BOOL known_scheme = data->scheme_type != URL_SCHEME_UNKNOWN;
    const BOOL is_file = data->scheme_type == URL_SCHEME_FILE;

    if(!data->path) {
        uri->path_start = -1;
        uri->path_len = 0;
        return TRUE;
    }

    uri->path_start = uri->canon_len;

    /* Windows doesn't allow a "//" to appear after the scheme
     * of a URI, if it's an opaque URI.
     */
    if(data->scheme && *(data->path) == '/' && *(data->path+1) == '/') {
        /* So it inserts a "/." before the "//" if it exists. */
        if(!computeOnly) {
            uri->canon_uri[uri->canon_len] = '/';
            uri->canon_uri[uri->canon_len+1] = '.';
        }

        uri->canon_len += 2;
    }

    for(ptr = data->path; ptr < data->path+data->path_len; ++ptr) {
        if(*ptr == '%' && known_scheme) {
            WCHAR val = decode_pct_val(ptr);

            if(is_unreserved(val)) {
                if(!computeOnly)
                    uri->canon_uri[uri->canon_len] = val;
                ++uri->canon_len;

                ptr += 2;
                continue;
            } else {
                if(!computeOnly)
                    uri->canon_uri[uri->canon_len] = *ptr;
                ++uri->canon_len;
            }
        } else if(*ptr == '/' && is_file && (flags & Uri_CREATE_FILE_USE_DOS_PATH)) {
            if(!computeOnly)
                uri->canon_uri[uri->canon_len] = '\\';
            ++uri->canon_len;
        } else if(*ptr == '\\' && is_file) {
            if(!(flags & Uri_CREATE_FILE_USE_DOS_PATH)) {
                /* Convert to a '/'. */
                if(!computeOnly)
                    uri->canon_uri[uri->canon_len] = '/';
                ++uri->canon_len;
            } else {
                /* Just copy it over. */
                if(!computeOnly)
                    uri->canon_uri[uri->canon_len] = *ptr;
                ++uri->canon_len;
            }
        } else if(known_scheme && !is_unreserved(*ptr) && !is_reserved(*ptr) &&
                  !(flags & Uri_CREATE_NO_ENCODE_FORBIDDEN_CHARACTERS)) {
            if(is_file && (flags & Uri_CREATE_FILE_USE_DOS_PATH)) {
                /* Forbidden characters aren't percent encoded for file schemes
                 * with USE_DOS_PATH set.
                 */
                if(!computeOnly)
                    uri->canon_uri[uri->canon_len] = *ptr;
                ++uri->canon_len;
            } else if(data->scheme_type == URL_SCHEME_MK && *ptr == '\\') {
                /* MK URIs don't get '\\' percent encoded. */
                if(!computeOnly)
                    uri->canon_uri[uri->canon_len] = *ptr;
                ++uri->canon_len;
            } else {
                if(!computeOnly)
                    pct_encode_val(*ptr, uri->canon_uri+uri->canon_len);
                uri->canon_len += 3;
            }
        } else {
            if(!computeOnly)
                uri->canon_uri[uri->canon_len] = *ptr;
            ++uri->canon_len;
        }
    }

    uri->path_len = uri->canon_len - uri->path_start;

    TRACE("(%p %p %x %d): Canonicalized opaque URI path %s len=%d\n", data, uri, flags, computeOnly,
        debugstr_wn(uri->canon_uri+uri->path_start, uri->path_len), uri->path_len);
    return TRUE;
}

/* Determines how the URI represented by the parse_data should be canonicalized.
 *
 * Essentially, if the parse_data represents an hierarchical URI then it calls
 * canonicalize_authority and the canonicalization functions for the path. If the
 * URI is opaque it canonicalizes the path of the URI.
 */
static BOOL canonicalize_hierpart(const parse_data *data, Uri *uri, DWORD flags, BOOL computeOnly) {
    if(!data->is_opaque || (data->is_relative && (data->password || data->username))) {
        /* "//" is only added for non-wildcard scheme types.
         *
         * A "//" is only added to a relative URI if it has a
         * host or port component (this only happens if a IUriBuilder
         * is generating an IUri).
         */
        if((data->is_relative && (data->host || data->has_port)) ||
           (!data->is_relative && data->scheme_type != URL_SCHEME_WILDCARD)) {
            if(!computeOnly) {
                INT pos = uri->canon_len;

                uri->canon_uri[pos] = '/';
                uri->canon_uri[pos+1] = '/';
           }
           uri->canon_len += 2;
        }

        if(!canonicalize_authority(data, uri, flags, computeOnly))
            return FALSE;

        if(data->is_relative && (data->password || data->username)) {
            if(!canonicalize_path_opaque(data, uri, flags, computeOnly))
                return FALSE;
        } else {
            if(!canonicalize_path_hierarchical(data, uri, flags, computeOnly))
                return FALSE;
        }
    } else {
        /* Opaque URI's don't have an authority. */
        uri->userinfo_start = uri->userinfo_split = -1;
        uri->userinfo_len = 0;
        uri->host_start = -1;
        uri->host_len = 0;
        uri->host_type = Uri_HOST_UNKNOWN;
        uri->has_port = FALSE;
        uri->authority_start = -1;
        uri->authority_len = 0;
        uri->domain_offset = -1;
        uri->port_offset = -1;

        if(is_hierarchical_scheme(data->scheme_type)) {
            DWORD i;

            /* Absolute URIs aren't displayed for known scheme types
             * which should be hierarchical URIs.
             */
            uri->display_modifiers |= URI_DISPLAY_NO_ABSOLUTE_URI;

            /* Windows also sets the port for these (if they have one). */
            for(i = 0; i < sizeof(default_ports)/sizeof(default_ports[0]); ++i) {
                if(data->scheme_type == default_ports[i].scheme) {
                    uri->has_port = TRUE;
                    uri->port = default_ports[i].port;
                    break;
                }
            }
        }

        if(!canonicalize_path_opaque(data, uri, flags, computeOnly))
            return FALSE;
    }

    if(uri->path_start > -1 && !computeOnly)
        /* Finding file extensions happens for both types of URIs. */
        uri->extension_offset = find_file_extension(uri->canon_uri+uri->path_start, uri->path_len);
    else
        uri->extension_offset = -1;

    return TRUE;
}

/* Attempts to canonicalize the query string of the URI.
 *
 * Things that happen:
 *  1)  For known scheme types forbidden characters
 *      are percent encoded, unless the NO_DECODE_EXTRA_INFO flag is set
 *      or NO_ENCODE_FORBIDDEN_CHARACTERS is set.
 *
 *  2)  For known scheme types, percent encoded, unreserved characters
 *      are decoded as long as the NO_DECODE_EXTRA_INFO flag isn't set.
 */
static BOOL canonicalize_query(const parse_data *data, Uri *uri, DWORD flags, BOOL computeOnly) {
    const WCHAR *ptr, *end;
    const BOOL known_scheme = data->scheme_type != URL_SCHEME_UNKNOWN;

    if(!data->query) {
        uri->query_start = -1;
        uri->query_len = 0;
        return TRUE;
    }

    uri->query_start = uri->canon_len;

    end = data->query+data->query_len;
    for(ptr = data->query; ptr < end; ++ptr) {
        if(*ptr == '%') {
            if(known_scheme && !(flags & Uri_CREATE_NO_DECODE_EXTRA_INFO)) {
                WCHAR val = decode_pct_val(ptr);
                if(is_unreserved(val)) {
                    if(!computeOnly)
                        uri->canon_uri[uri->canon_len] = val;
                    ++uri->canon_len;

                    ptr += 2;
                    continue;
                }
            }
        } else if(known_scheme && !is_unreserved(*ptr) && !is_reserved(*ptr)) {
            if(!(flags & Uri_CREATE_NO_ENCODE_FORBIDDEN_CHARACTERS) &&
               !(flags & Uri_CREATE_NO_DECODE_EXTRA_INFO)) {
                if(!computeOnly)
                    pct_encode_val(*ptr, uri->canon_uri+uri->canon_len);
                uri->canon_len += 3;
                continue;
            }
        }

        if(!computeOnly)
            uri->canon_uri[uri->canon_len] = *ptr;
        ++uri->canon_len;
    }

    uri->query_len = uri->canon_len - uri->query_start;

    if(!computeOnly)
        TRACE("(%p %p %x %d): Canonicalized query string %s len=%d\n", data, uri, flags,
            computeOnly, debugstr_wn(uri->canon_uri+uri->query_start, uri->query_len),
            uri->query_len);
    return TRUE;
}

static BOOL canonicalize_fragment(const parse_data *data, Uri *uri, DWORD flags, BOOL computeOnly) {
    const WCHAR *ptr, *end;
    const BOOL known_scheme = data->scheme_type != URL_SCHEME_UNKNOWN;

    if(!data->fragment) {
        uri->fragment_start = -1;
        uri->fragment_len = 0;
        return TRUE;
    }

    uri->fragment_start = uri->canon_len;

    end = data->fragment + data->fragment_len;
    for(ptr = data->fragment; ptr < end; ++ptr) {
        if(*ptr == '%') {
            if(known_scheme && !(flags & Uri_CREATE_NO_DECODE_EXTRA_INFO)) {
                WCHAR val = decode_pct_val(ptr);
                if(is_unreserved(val)) {
                    if(!computeOnly)
                        uri->canon_uri[uri->canon_len] = val;
                    ++uri->canon_len;

                    ptr += 2;
                    continue;
                }
            }
        } else if(known_scheme && !is_unreserved(*ptr) && !is_reserved(*ptr)) {
            if(!(flags & Uri_CREATE_NO_ENCODE_FORBIDDEN_CHARACTERS) &&
               !(flags & Uri_CREATE_NO_DECODE_EXTRA_INFO)) {
                if(!computeOnly)
                    pct_encode_val(*ptr, uri->canon_uri+uri->canon_len);
                uri->canon_len += 3;
                continue;
            }
        }

        if(!computeOnly)
            uri->canon_uri[uri->canon_len] = *ptr;
        ++uri->canon_len;
    }

    uri->fragment_len = uri->canon_len - uri->fragment_start;

    if(!computeOnly)
        TRACE("(%p %p %x %d): Canonicalized fragment %s len=%d\n", data, uri, flags,
            computeOnly, debugstr_wn(uri->canon_uri+uri->fragment_start, uri->fragment_len),
            uri->fragment_len);
    return TRUE;
}

/* Canonicalizes the scheme information specified in the parse_data using the specified flags. */
static BOOL canonicalize_scheme(const parse_data *data, Uri *uri, DWORD flags, BOOL computeOnly) {
    uri->scheme_start = -1;
    uri->scheme_len = 0;

    if(!data->scheme) {
        /* The only type of URI that doesn't have to have a scheme is a relative
         * URI.
         */
        if(!data->is_relative) {
            FIXME("(%p %p %x): Unable to determine the scheme type of %s.\n", data,
                    uri, flags, debugstr_w(data->uri));
            return FALSE;
        }
    } else {
        if(!computeOnly) {
            DWORD i;
            INT pos = uri->canon_len;

            for(i = 0; i < data->scheme_len; ++i) {
                /* Scheme name must be lower case after canonicalization. */
                uri->canon_uri[i + pos] = tolowerW(data->scheme[i]);
            }

            uri->canon_uri[i + pos] = ':';
            uri->scheme_start = pos;

            TRACE("(%p %p %x): Canonicalized scheme=%s, len=%d.\n", data, uri, flags,
                    debugstr_wn(uri->canon_uri,  uri->scheme_len), data->scheme_len);
        }

        /* This happens in both computation modes. */
        uri->canon_len += data->scheme_len + 1;
        uri->scheme_len = data->scheme_len;
    }
    return TRUE;
}

/* Compute's what the length of the URI specified by the parse_data will be
 * after canonicalization occurs using the specified flags.
 *
 * This function will return a non-zero value indicating the length of the canonicalized
 * URI, or -1 on error.
 */
static int compute_canonicalized_length(const parse_data *data, DWORD flags) {
    Uri uri;

    memset(&uri, 0, sizeof(Uri));

    TRACE("(%p %x): Beginning to compute canonicalized length for URI %s\n", data, flags,
            debugstr_w(data->uri));

    if(!canonicalize_scheme(data, &uri, flags, TRUE)) {
        ERR("(%p %x): Failed to compute URI scheme length.\n", data, flags);
        return -1;
    }

    if(!canonicalize_hierpart(data, &uri, flags, TRUE)) {
        ERR("(%p %x): Failed to compute URI hierpart length.\n", data, flags);
        return -1;
    }

    if(!canonicalize_query(data, &uri, flags, TRUE)) {
        ERR("(%p %x): Failed to compute query string length.\n", data, flags);
        return -1;
    }

    if(!canonicalize_fragment(data, &uri, flags, TRUE)) {
        ERR("(%p %x): Failed to compute fragment length.\n", data, flags);
        return -1;
    }

    TRACE("(%p %x): Finished computing canonicalized URI length. length=%d\n", data, flags, uri.canon_len);

    return uri.canon_len;
}

/* Canonicalizes the URI data specified in the parse_data, using the given flags. If the
 * canonicalization succeededs it will store all the canonicalization information
 * in the pointer to the Uri.
 *
 * To canonicalize a URI this function first computes what the length of the URI
 * specified by the parse_data will be. Once this is done it will then perfom the actual
 * canonicalization of the URI.
 */
static HRESULT canonicalize_uri(const parse_data *data, Uri *uri, DWORD flags) {
    INT len;

    uri->canon_uri = NULL;
    len = uri->canon_size = uri->canon_len = 0;

    TRACE("(%p %p %x): beginning to canonicalize URI %s.\n", data, uri, flags, debugstr_w(data->uri));

    /* First try to compute the length of the URI. */
    len = compute_canonicalized_length(data, flags);
    if(len == -1) {
        ERR("(%p %p %x): Could not compute the canonicalized length of %s.\n", data, uri, flags,
                debugstr_w(data->uri));
        return E_INVALIDARG;
    }

    uri->canon_uri = heap_alloc((len+1)*sizeof(WCHAR));
    if(!uri->canon_uri)
        return E_OUTOFMEMORY;

    uri->canon_size = len;
    if(!canonicalize_scheme(data, uri, flags, FALSE)) {
        ERR("(%p %p %x): Unable to canonicalize the scheme of the URI.\n", data, uri, flags);
        return E_INVALIDARG;
    }
    uri->scheme_type = data->scheme_type;

    if(!canonicalize_hierpart(data, uri, flags, FALSE)) {
        ERR("(%p %p %x): Unable to canonicalize the heirpart of the URI\n", data, uri, flags);
        return E_INVALIDARG;
    }

    if(!canonicalize_query(data, uri, flags, FALSE)) {
        ERR("(%p %p %x): Unable to canonicalize query string of the URI.\n",
            data, uri, flags);
        return E_INVALIDARG;
    }

    if(!canonicalize_fragment(data, uri, flags, FALSE)) {
        ERR("(%p %p %x): Unable to canonicalize fragment of the URI.\n",
            data, uri, flags);
        return E_INVALIDARG;
    }

    /* There's a possibility we didn't use all the space we allocated
     * earlier.
     */
    if(uri->canon_len < uri->canon_size) {
        /* This happens if the URI is hierarchical and dot
         * segments were removed from it's path.
         */
        WCHAR *tmp = heap_realloc(uri->canon_uri, (uri->canon_len+1)*sizeof(WCHAR));
        if(!tmp)
            return E_OUTOFMEMORY;

        uri->canon_uri = tmp;
        uri->canon_size = uri->canon_len;
    }

    uri->canon_uri[uri->canon_len] = '\0';
    TRACE("(%p %p %x): finished canonicalizing the URI. uri=%s\n", data, uri, flags, debugstr_w(uri->canon_uri));

    return S_OK;
}

static HRESULT get_builder_component(LPWSTR *component, DWORD *component_len,
                                     LPCWSTR source, DWORD source_len,
                                     LPCWSTR *output, DWORD *output_len)
{
    if(!output_len) {
        if(output)
            *output = NULL;
        return E_POINTER;
    }

    if(!output) {
        *output_len = 0;
        return E_POINTER;
    }

    if(!(*component) && source) {
        /* Allocate 'component', and copy the contents from 'source'
         * into the new allocation.
         */
        *component = heap_alloc((source_len+1)*sizeof(WCHAR));
        if(!(*component))
            return E_OUTOFMEMORY;

        memcpy(*component, source, source_len*sizeof(WCHAR));
        (*component)[source_len] = '\0';
        *component_len = source_len;
    }

    *output = *component;
    *output_len = *component_len;
    return *output ? S_OK : S_FALSE;
}

/* Allocates 'component' and copies the string from 'new_value' into 'component'.
 * If 'prefix' is set and 'new_value' isn't NULL, then it checks if 'new_value'
 * starts with 'prefix'. If it doesn't then 'prefix' is prepended to 'component'.
 *
 * If everything is successful, then will set 'success_flag' in 'flags'.
 */
static HRESULT set_builder_component(LPWSTR *component, DWORD *component_len, LPCWSTR new_value,
                                     WCHAR prefix, DWORD *flags, DWORD success_flag)
{
    heap_free(*component);

    if(!new_value) {
        *component = NULL;
        *component_len = 0;
    } else {
        BOOL add_prefix = FALSE;
        DWORD len = lstrlenW(new_value);
        DWORD pos = 0;

        if(prefix && *new_value != prefix) {
            add_prefix = TRUE;
            *component = heap_alloc((len+2)*sizeof(WCHAR));
        } else
            *component = heap_alloc((len+1)*sizeof(WCHAR));

        if(!(*component))
            return E_OUTOFMEMORY;

        if(add_prefix)
            (*component)[pos++] = prefix;

        memcpy(*component+pos, new_value, (len+1)*sizeof(WCHAR));
        *component_len = len+pos;
    }

    *flags |= success_flag;
    return S_OK;
}

#define URI(x)         ((IUri*)  &(x)->lpIUriVtbl)
#define URIBUILDER(x)  ((IUriBuilder*)  &(x)->lpIUriBuilderVtbl)

static void reset_builder(UriBuilder *builder) {
    if(builder->uri)
        IUri_Release(URI(builder->uri));
    builder->uri = NULL;

    heap_free(builder->fragment);
    builder->fragment = NULL;
    builder->fragment_len = 0;

    heap_free(builder->host);
    builder->host = NULL;
    builder->host_len = 0;

    heap_free(builder->password);
    builder->password = NULL;
    builder->password_len = 0;

    heap_free(builder->path);
    builder->path = NULL;
    builder->path_len = 0;

    heap_free(builder->query);
    builder->query = NULL;
    builder->query_len = 0;

    heap_free(builder->scheme);
    builder->scheme = NULL;
    builder->scheme_len = 0;

    heap_free(builder->username);
    builder->username = NULL;
    builder->username_len = 0;

    builder->has_port = FALSE;
    builder->port = 0;
    builder->modified_props = 0;
}

static HRESULT validate_scheme_name(const UriBuilder *builder, parse_data *data, DWORD flags) {
    const WCHAR *component;
    const WCHAR *ptr;
    const WCHAR **pptr;
    DWORD expected_len;

    if(builder->scheme) {
        ptr = builder->scheme;
        expected_len = builder->scheme_len;
    } else if(builder->uri && builder->uri->scheme_start > -1) {
        ptr = builder->uri->canon_uri+builder->uri->scheme_start;
        expected_len = builder->uri->scheme_len;
    } else {
        static const WCHAR nullW[] = {0};
        ptr = nullW;
        expected_len = 0;
    }

    component = ptr;
    pptr = &ptr;
    if(parse_scheme(pptr, data, flags, ALLOW_NULL_TERM_SCHEME) &&
       data->scheme_len == expected_len) {
        if(data->scheme)
            TRACE("(%p %p %x): Found valid scheme component %s len=%d.\n", builder, data, flags,
               debugstr_wn(data->scheme, data->scheme_len), data->scheme_len);
    } else {
        TRACE("(%p %p %x): Invalid scheme component found %s.\n", builder, data, flags,
            debugstr_wn(component, expected_len));
        return INET_E_INVALID_URL;
   }

    return S_OK;
}

static HRESULT validate_username(const UriBuilder *builder, parse_data *data, DWORD flags) {
    const WCHAR *ptr;
    const WCHAR **pptr;
    DWORD expected_len;

    if(builder->username) {
        ptr = builder->username;
        expected_len = builder->username_len;
    } else if(!(builder->modified_props & Uri_HAS_USER_NAME) && builder->uri &&
              builder->uri->userinfo_start > -1 && builder->uri->userinfo_split != 0) {
        /* Just use the username from the base Uri. */
        data->username = builder->uri->canon_uri+builder->uri->userinfo_start;
        data->username_len = (builder->uri->userinfo_split > -1) ?
                                        builder->uri->userinfo_split : builder->uri->userinfo_len;
        ptr = NULL;
    } else {
        ptr = NULL;
        expected_len = 0;
    }

    if(ptr) {
        const WCHAR *component = ptr;
        pptr = &ptr;
        if(parse_username(pptr, data, flags, ALLOW_NULL_TERM_USER_NAME) &&
           data->username_len == expected_len)
            TRACE("(%p %p %x): Found valid username component %s len=%d.\n", builder, data, flags,
                debugstr_wn(data->username, data->username_len), data->username_len);
        else {
            TRACE("(%p %p %x): Invalid username component found %s.\n", builder, data, flags,
                debugstr_wn(component, expected_len));
            return INET_E_INVALID_URL;
        }
    }

    return S_OK;
}

static HRESULT validate_password(const UriBuilder *builder, parse_data *data, DWORD flags) {
    const WCHAR *ptr;
    const WCHAR **pptr;
    DWORD expected_len;

    if(builder->password) {
        ptr = builder->password;
        expected_len = builder->password_len;
    } else if(!(builder->modified_props & Uri_HAS_PASSWORD) && builder->uri &&
              builder->uri->userinfo_split > -1) {
        data->password = builder->uri->canon_uri+builder->uri->userinfo_start+builder->uri->userinfo_split+1;
        data->password_len = builder->uri->userinfo_len-builder->uri->userinfo_split-1;
        ptr = NULL;
    } else {
        ptr = NULL;
        expected_len = 0;
    }

    if(ptr) {
        const WCHAR *component = ptr;
        pptr = &ptr;
        if(parse_password(pptr, data, flags, ALLOW_NULL_TERM_PASSWORD) &&
           data->password_len == expected_len)
            TRACE("(%p %p %x): Found valid password component %s len=%d.\n", builder, data, flags,
                debugstr_wn(data->password, data->password_len), data->password_len);
        else {
            TRACE("(%p %p %x): Invalid password component found %s.\n", builder, data, flags,
                debugstr_wn(component, expected_len));
            return INET_E_INVALID_URL;
        }
    }

    return S_OK;
}

static HRESULT validate_userinfo(const UriBuilder *builder, parse_data *data, DWORD flags) {
    HRESULT hr;

    hr = validate_username(builder, data, flags);
    if(FAILED(hr))
        return hr;

    hr = validate_password(builder, data, flags);
    if(FAILED(hr))
        return hr;

    return S_OK;
}

static HRESULT validate_host(const UriBuilder *builder, parse_data *data, DWORD flags) {
    const WCHAR *ptr;
    const WCHAR **pptr;
    DWORD expected_len;

    if(builder->host) {
        ptr = builder->host;
        expected_len = builder->host_len;
    } else if(!(builder->modified_props & Uri_HAS_HOST) && builder->uri && builder->uri->host_start > -1) {
        ptr = builder->uri->canon_uri + builder->uri->host_start;
        expected_len = builder->uri->host_len;
    } else
        ptr = NULL;

    if(ptr) {
        const WCHAR *component = ptr;
        DWORD extras = ALLOW_BRACKETLESS_IP_LITERAL|IGNORE_PORT_DELIMITER|SKIP_IP_FUTURE_CHECK;
        pptr = &ptr;

        if(parse_host(pptr, data, flags, extras) && data->host_len == expected_len)
            TRACE("(%p %p %x): Found valid host name %s len=%d type=%d.\n", builder, data, flags,
                debugstr_wn(data->host, data->host_len), data->host_len, data->host_type);
        else {
            TRACE("(%p %p %x): Invalid host name found %s.\n", builder, data, flags,
                debugstr_wn(component, expected_len));
            return INET_E_INVALID_URL;
        }
    }

    return S_OK;
}

static void setup_port(const UriBuilder *builder, parse_data *data, DWORD flags) {
    if(builder->modified_props & Uri_HAS_PORT) {
        if(builder->has_port) {
            data->has_port = TRUE;
            data->port_value = builder->port;
        }
    } else if(builder->uri && builder->uri->has_port) {
        data->has_port = TRUE;
        data->port_value = builder->uri->port;
    }

    if(data->has_port)
        TRACE("(%p %p %x): Using %u as port for IUri.\n", builder, data, flags, data->port_value);
}

static HRESULT validate_path(const UriBuilder *builder, parse_data *data, DWORD flags) {
    const WCHAR *ptr = NULL;
    const WCHAR *component;
    const WCHAR **pptr;
    DWORD expected_len;
    BOOL check_len = TRUE;
    BOOL valid = FALSE;

    if(builder->path) {
        ptr = builder->path;
        expected_len = builder->path_len;
    } else if(!(builder->modified_props & Uri_HAS_PATH) &&
              builder->uri && builder->uri->path_start > -1) {
        ptr = builder->uri->canon_uri+builder->uri->path_start;
        expected_len = builder->uri->path_len;
    } else {
        static const WCHAR nullW[] = {0};
        ptr = nullW;
        check_len = FALSE;
    }

    component = ptr;
    pptr = &ptr;

    /* How the path is validated depends on what type of
     * URI it is.
     */
    valid = data->is_opaque ?
        parse_path_opaque(pptr, data, flags) : parse_path_hierarchical(pptr, data, flags);

    if(!valid || (check_len && expected_len != data->path_len)) {
        TRACE("(%p %p %x): Invalid path component %s.\n", builder, data, flags,
            debugstr_wn(component, expected_len));
        return INET_E_INVALID_URL;
    }

    TRACE("(%p %p %x): Valid path component %s len=%d.\n", builder, data, flags,
        debugstr_wn(data->path, data->path_len), data->path_len);

    return S_OK;
}

static HRESULT validate_query(const UriBuilder *builder, parse_data *data, DWORD flags) {
    const WCHAR *ptr = NULL;
    const WCHAR **pptr;
    DWORD expected_len;

    if(builder->query) {
        ptr = builder->query;
        expected_len = builder->query_len;
    } else if(!(builder->modified_props & Uri_HAS_QUERY) && builder->uri &&
              builder->uri->query_start > -1) {
        ptr = builder->uri->canon_uri+builder->uri->query_start;
        expected_len = builder->uri->query_len;
    }

    if(ptr) {
        const WCHAR *component = ptr;
        pptr = &ptr;

        if(parse_query(pptr, data, flags) && expected_len == data->query_len)
            TRACE("(%p %p %x): Valid query component %s len=%d.\n", builder, data, flags,
                debugstr_wn(data->query, data->query_len), data->query_len);
        else {
            TRACE("(%p %p %x): Invalid query component %s.\n", builder, data, flags,
                debugstr_wn(component, expected_len));
            return INET_E_INVALID_URL;
        }
    }

    return S_OK;
}

static HRESULT validate_fragment(const UriBuilder *builder, parse_data *data, DWORD flags) {
    const WCHAR *ptr = NULL;
    const WCHAR **pptr;
    DWORD expected_len;

    if(builder->fragment) {
        ptr = builder->fragment;
        expected_len = builder->fragment_len;
    } else if(!(builder->modified_props & Uri_HAS_FRAGMENT) && builder->uri &&
              builder->uri->fragment_start > -1) {
        ptr = builder->uri->canon_uri+builder->uri->fragment_start;
        expected_len = builder->uri->fragment_len;
    }

    if(ptr) {
        const WCHAR *component = ptr;
        pptr = &ptr;

        if(parse_fragment(pptr, data, flags) && expected_len == data->fragment_len)
            TRACE("(%p %p %x): Valid fragment component %s len=%d.\n", builder, data, flags,
                debugstr_wn(data->fragment, data->fragment_len), data->fragment_len);
        else {
            TRACE("(%p %p %x): Invalid fragment component %s.\n", builder, data, flags,
                debugstr_wn(component, expected_len));
            return INET_E_INVALID_URL;
        }
    }

    return S_OK;
}

static HRESULT validate_components(const UriBuilder *builder, parse_data *data, DWORD flags) {
    HRESULT hr;

    memset(data, 0, sizeof(parse_data));

    TRACE("(%p %p %x): Beginning to validate builder components.\n", builder, data, flags);

    hr = validate_scheme_name(builder, data, flags);
    if(FAILED(hr))
        return hr;

    /* Extra validation for file schemes. */
    if(data->scheme_type == URL_SCHEME_FILE) {
        if((builder->password || (builder->uri && builder->uri->userinfo_split > -1)) ||
           (builder->username || (builder->uri && builder->uri->userinfo_start > -1))) {
            TRACE("(%p %p %x): File schemes can't contain a username or password.\n",
                builder, data, flags);
            return INET_E_INVALID_URL;
        }
    }

    hr = validate_userinfo(builder, data, flags);
    if(FAILED(hr))
        return hr;

    hr = validate_host(builder, data, flags);
    if(FAILED(hr))
        return hr;

    setup_port(builder, data, flags);

    /* The URI is opaque if it doesn't have an authority component. */
    if(!data->is_relative)
        data->is_opaque = !data->username && !data->password && !data->host && !data->has_port;
    else
        data->is_opaque = !data->host && !data->has_port;

    hr = validate_path(builder, data, flags);
    if(FAILED(hr))
        return hr;

    hr = validate_query(builder, data, flags);
    if(FAILED(hr))
        return hr;

    hr = validate_fragment(builder, data, flags);
    if(FAILED(hr))
        return hr;

    TRACE("(%p %p %x): Finished validating builder components.\n", builder, data, flags);

    return S_OK;
}

static void convert_to_dos_path(const WCHAR *path, DWORD path_len,
                                WCHAR *output, DWORD *output_len)
{
    const WCHAR *ptr = path;

    if(path_len > 3 && *ptr == '/' && is_drive_path(path+1))
        /* Skip over the leading / before the drive path. */
        ++ptr;

    for(; ptr < path+path_len; ++ptr) {
        if(*ptr == '/') {
            if(output)
                *output++ = '\\';
            (*output_len)++;
        } else {
            if(output)
                *output++ = *ptr;
            (*output_len)++;
        }
    }
}

/* Generates a raw uri string using the parse_data. */
static DWORD generate_raw_uri(const parse_data *data, BSTR uri, DWORD flags) {
    DWORD length = 0;

    if(data->scheme) {
        if(uri) {
            memcpy(uri, data->scheme, data->scheme_len*sizeof(WCHAR));
            uri[data->scheme_len] = ':';
        }
        length += data->scheme_len+1;
    }

    if(!data->is_opaque) {
        /* For the "//" which appears before the authority component. */
        if(uri) {
            uri[length] = '/';
            uri[length+1] = '/';
        }
        length += 2;

        /* Check if we need to add the "\\" before the host name
         * of a UNC server name in a DOS path.
         */
        if(flags & RAW_URI_CONVERT_TO_DOS_PATH &&
           data->scheme_type == URL_SCHEME_FILE && data->host) {
            if(uri) {
                uri[length] = '\\';
                uri[length+1] = '\\';
            }
            length += 2;
        }
    }

    if(data->username) {
        if(uri)
            memcpy(uri+length, data->username, data->username_len*sizeof(WCHAR));
        length += data->username_len;
    }

    if(data->password) {
        if(uri) {
            uri[length] = ':';
            memcpy(uri+length+1, data->password, data->password_len*sizeof(WCHAR));
        }
        length += data->password_len+1;
    }

    if(data->password || data->username) {
        if(uri)
            uri[length] = '@';
        ++length;
    }

    if(data->host) {
        /* IPv6 addresses get the brackets added around them if they don't already
         * have them.
         */
        const BOOL add_brackets = data->host_type == Uri_HOST_IPV6 && *(data->host) != '[';
        if(add_brackets) {
            if(uri)
                uri[length] = '[';
            ++length;
        }

        if(uri)
            memcpy(uri+length, data->host, data->host_len*sizeof(WCHAR));
        length += data->host_len;

        if(add_brackets) {
            if(uri)
                uri[length] = ']';
            length++;
        }
    }

    if(data->has_port) {
        /* The port isn't included in the raw uri if it's the default
         * port for the scheme type.
         */
        DWORD i;
        BOOL is_default = FALSE;

        for(i = 0; i < sizeof(default_ports)/sizeof(default_ports[0]); ++i) {
            if(data->scheme_type == default_ports[i].scheme &&
               data->port_value == default_ports[i].port)
                is_default = TRUE;
        }

        if(!is_default || flags & RAW_URI_FORCE_PORT_DISP) {
            if(uri)
                uri[length] = ':';
            ++length;

            if(uri)
                length += ui2str(uri+length, data->port_value);
            else
                length += ui2str(NULL, data->port_value);
        }
    }

    /* Check if a '/' should be added before the path for hierarchical URIs. */
    if(!data->is_opaque && data->path && *(data->path) != '/') {
        if(uri)
            uri[length] = '/';
        ++length;
    }

    if(data->path) {
        if(!data->is_opaque && data->scheme_type == URL_SCHEME_FILE &&
           flags & RAW_URI_CONVERT_TO_DOS_PATH) {
            DWORD len = 0;

            if(uri)
                convert_to_dos_path(data->path, data->path_len, uri+length, &len);
            else
                convert_to_dos_path(data->path, data->path_len, NULL, &len);

            length += len;
        } else {
            if(uri)
                memcpy(uri+length, data->path, data->path_len*sizeof(WCHAR));
            length += data->path_len;
        }
    }

    if(data->query) {
        if(uri)
            memcpy(uri+length, data->query, data->query_len*sizeof(WCHAR));
        length += data->query_len;
    }

    if(data->fragment) {
        if(uri)
            memcpy(uri+length, data->fragment, data->fragment_len*sizeof(WCHAR));
        length += data->fragment_len;
    }

    if(uri)
        TRACE("(%p %p): Generated raw uri=%s len=%d\n", data, uri, debugstr_wn(uri, length), length);
    else
        TRACE("(%p %p): Computed raw uri len=%d\n", data, uri, length);

    return length;
}

static HRESULT generate_uri(const UriBuilder *builder, const parse_data *data, Uri *uri, DWORD flags) {
    HRESULT hr;
    DWORD length = generate_raw_uri(data, NULL, 0);
    uri->raw_uri = SysAllocStringLen(NULL, length);
    if(!uri->raw_uri)
        return E_OUTOFMEMORY;

    generate_raw_uri(data, uri->raw_uri, 0);

    hr = canonicalize_uri(data, uri, flags);
    if(FAILED(hr)) {
        if(hr == E_INVALIDARG)
            return INET_E_INVALID_URL;
        return hr;
    }

    uri->create_flags = flags;
    return S_OK;
}

#define URI_THIS(iface) DEFINE_THIS(Uri, IUri, iface)

static HRESULT WINAPI Uri_QueryInterface(IUri *iface, REFIID riid, void **ppv)
{
    Uri *This = URI_THIS(iface);

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        TRACE("(%p)->(IID_IUnknown %p)\n", This, ppv);
        *ppv = URI(This);
    }else if(IsEqualGUID(&IID_IUri, riid)) {
        TRACE("(%p)->(IID_IUri %p)\n", This, ppv);
        *ppv = URI(This);
    }else if(IsEqualGUID(&IID_IUriObj, riid)) {
        TRACE("(%p)->(IID_IUriObj %p)\n", This, ppv);
        *ppv = This;
        return S_OK;
    }else {
        TRACE("(%p)->(%s %p)\n", This, debugstr_guid(riid), ppv);
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI Uri_AddRef(IUri *iface)
{
    Uri *This = URI_THIS(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    return ref;
}

static ULONG WINAPI Uri_Release(IUri *iface)
{
    Uri *This = URI_THIS(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    if(!ref) {
        SysFreeString(This->raw_uri);
        heap_free(This->canon_uri);
        heap_free(This);
    }

    return ref;
}

static HRESULT WINAPI Uri_GetPropertyBSTR(IUri *iface, Uri_PROPERTY uriProp, BSTR *pbstrProperty, DWORD dwFlags)
{
    Uri *This = URI_THIS(iface);
    HRESULT hres;
    TRACE("(%p)->(%d %p %x)\n", This, uriProp, pbstrProperty, dwFlags);

    if(!pbstrProperty)
        return E_POINTER;

    if(uriProp > Uri_PROPERTY_STRING_LAST) {
        /* Windows allocates an empty BSTR for invalid Uri_PROPERTY's. */
        *pbstrProperty = SysAllocStringLen(NULL, 0);
        if(!(*pbstrProperty))
            return E_OUTOFMEMORY;

        /* It only returns S_FALSE for the ZONE property... */
        if(uriProp == Uri_PROPERTY_ZONE)
            return S_FALSE;
        else
            return S_OK;
    }

    /* Don't have support for flags yet. */
    if(dwFlags) {
        FIXME("(%p)->(%d %p %x)\n", This, uriProp, pbstrProperty, dwFlags);
        return E_NOTIMPL;
    }

    switch(uriProp) {
    case Uri_PROPERTY_ABSOLUTE_URI:
        if(This->display_modifiers & URI_DISPLAY_NO_ABSOLUTE_URI) {
            *pbstrProperty = SysAllocStringLen(NULL, 0);
            hres = S_FALSE;
        } else {
            if(This->scheme_type != URL_SCHEME_UNKNOWN && This->userinfo_start > -1) {
                if(This->userinfo_len == 0) {
                    /* Don't include the '@' after the userinfo component. */
                    *pbstrProperty = SysAllocStringLen(NULL, This->canon_len-1);
                    hres = S_OK;
                    if(*pbstrProperty) {
                        /* Copy everything before it. */
                        memcpy(*pbstrProperty, This->canon_uri, This->userinfo_start*sizeof(WCHAR));

                        /* And everything after it. */
                        memcpy(*pbstrProperty+This->userinfo_start, This->canon_uri+This->userinfo_start+1,
                               (This->canon_len-This->userinfo_start-1)*sizeof(WCHAR));
                    }
                } else if(This->userinfo_split == 0 && This->userinfo_len == 1) {
                    /* Don't include the ":@" */
                    *pbstrProperty = SysAllocStringLen(NULL, This->canon_len-2);
                    hres = S_OK;
                    if(*pbstrProperty) {
                        memcpy(*pbstrProperty, This->canon_uri, This->userinfo_start*sizeof(WCHAR));
                        memcpy(*pbstrProperty+This->userinfo_start, This->canon_uri+This->userinfo_start+2,
                               (This->canon_len-This->userinfo_start-2)*sizeof(WCHAR));
                    }
                } else {
                    *pbstrProperty = SysAllocString(This->canon_uri);
                    hres = S_OK;
                }
            } else {
                *pbstrProperty = SysAllocString(This->canon_uri);
                hres = S_OK;
            }
        }

        if(!(*pbstrProperty))
            hres = E_OUTOFMEMORY;

        break;
    case Uri_PROPERTY_AUTHORITY:
        if(This->authority_start > -1) {
            if(This->port_offset > -1 && is_default_port(This->scheme_type, This->port) &&
               This->display_modifiers & URI_DISPLAY_NO_DEFAULT_PORT_AUTH)
                /* Don't include the port in the authority component. */
                *pbstrProperty = SysAllocStringLen(This->canon_uri+This->authority_start, This->port_offset);
            else
                *pbstrProperty = SysAllocStringLen(This->canon_uri+This->authority_start, This->authority_len);
            hres = S_OK;
        } else {
            *pbstrProperty = SysAllocStringLen(NULL, 0);
            hres = S_FALSE;
        }

        if(!(*pbstrProperty))
            hres = E_OUTOFMEMORY;

        break;
    case Uri_PROPERTY_DISPLAY_URI:
        /* The Display URI contains everything except for the userinfo for known
         * scheme types.
         */
        if(This->scheme_type != URL_SCHEME_UNKNOWN && This->userinfo_start > -1) {
            *pbstrProperty = SysAllocStringLen(NULL, This->canon_len-This->userinfo_len);

            if(*pbstrProperty) {
                /* Copy everything before the userinfo over. */
                memcpy(*pbstrProperty, This->canon_uri, This->userinfo_start*sizeof(WCHAR));
                /* Copy everything after the userinfo over. */
                memcpy(*pbstrProperty+This->userinfo_start,
                   This->canon_uri+This->userinfo_start+This->userinfo_len+1,
                   (This->canon_len-(This->userinfo_start+This->userinfo_len+1))*sizeof(WCHAR));
            }
        } else
            *pbstrProperty = SysAllocString(This->canon_uri);

        if(!(*pbstrProperty))
            hres = E_OUTOFMEMORY;
        else
            hres = S_OK;

        break;
    case Uri_PROPERTY_DOMAIN:
        if(This->domain_offset > -1) {
            *pbstrProperty = SysAllocStringLen(This->canon_uri+This->host_start+This->domain_offset,
                                               This->host_len-This->domain_offset);
            hres = S_OK;
        } else {
            *pbstrProperty = SysAllocStringLen(NULL, 0);
            hres = S_FALSE;
        }

        if(!(*pbstrProperty))
            hres = E_OUTOFMEMORY;

        break;
    case Uri_PROPERTY_EXTENSION:
        if(This->extension_offset > -1) {
            *pbstrProperty = SysAllocStringLen(This->canon_uri+This->path_start+This->extension_offset,
                                               This->path_len-This->extension_offset);
            hres = S_OK;
        } else {
            *pbstrProperty = SysAllocStringLen(NULL, 0);
            hres = S_FALSE;
        }

        if(!(*pbstrProperty))
            hres = E_OUTOFMEMORY;

        break;
    case Uri_PROPERTY_FRAGMENT:
        if(This->fragment_start > -1) {
            *pbstrProperty = SysAllocStringLen(This->canon_uri+This->fragment_start, This->fragment_len);
            hres = S_OK;
        } else {
            *pbstrProperty = SysAllocStringLen(NULL, 0);
            hres = S_FALSE;
        }

        if(!(*pbstrProperty))
            hres = E_OUTOFMEMORY;

        break;
    case Uri_PROPERTY_HOST:
        if(This->host_start > -1) {
            /* The '[' and ']' aren't included for IPv6 addresses. */
            if(This->host_type == Uri_HOST_IPV6)
                *pbstrProperty = SysAllocStringLen(This->canon_uri+This->host_start+1, This->host_len-2);
            else
                *pbstrProperty = SysAllocStringLen(This->canon_uri+This->host_start, This->host_len);

            hres = S_OK;
        } else {
            *pbstrProperty = SysAllocStringLen(NULL, 0);
            hres = S_FALSE;
        }

        if(!(*pbstrProperty))
            hres = E_OUTOFMEMORY;

        break;
    case Uri_PROPERTY_PASSWORD:
        if(This->userinfo_split > -1) {
            *pbstrProperty = SysAllocStringLen(
                This->canon_uri+This->userinfo_start+This->userinfo_split+1,
                This->userinfo_len-This->userinfo_split-1);
            hres = S_OK;
        } else {
            *pbstrProperty = SysAllocStringLen(NULL, 0);
            hres = S_FALSE;
        }

        if(!(*pbstrProperty))
            return E_OUTOFMEMORY;

        break;
    case Uri_PROPERTY_PATH:
        if(This->path_start > -1) {
            *pbstrProperty = SysAllocStringLen(This->canon_uri+This->path_start, This->path_len);
            hres = S_OK;
        } else {
            *pbstrProperty = SysAllocStringLen(NULL, 0);
            hres = S_FALSE;
        }

        if(!(*pbstrProperty))
            hres = E_OUTOFMEMORY;

        break;
    case Uri_PROPERTY_PATH_AND_QUERY:
        if(This->path_start > -1) {
            *pbstrProperty = SysAllocStringLen(This->canon_uri+This->path_start, This->path_len+This->query_len);
            hres = S_OK;
        } else if(This->query_start > -1) {
            *pbstrProperty = SysAllocStringLen(This->canon_uri+This->query_start, This->query_len);
            hres = S_OK;
        } else {
            *pbstrProperty = SysAllocStringLen(NULL, 0);
            hres = S_FALSE;
        }

        if(!(*pbstrProperty))
            hres = E_OUTOFMEMORY;

        break;
    case Uri_PROPERTY_QUERY:
        if(This->query_start > -1) {
            *pbstrProperty = SysAllocStringLen(This->canon_uri+This->query_start, This->query_len);
            hres = S_OK;
        } else {
            *pbstrProperty = SysAllocStringLen(NULL, 0);
            hres = S_FALSE;
        }

        if(!(*pbstrProperty))
            hres = E_OUTOFMEMORY;

        break;
    case Uri_PROPERTY_RAW_URI:
        *pbstrProperty = SysAllocString(This->raw_uri);
        if(!(*pbstrProperty))
            hres = E_OUTOFMEMORY;
        else
            hres = S_OK;
        break;
    case Uri_PROPERTY_SCHEME_NAME:
        if(This->scheme_start > -1) {
            *pbstrProperty = SysAllocStringLen(This->canon_uri + This->scheme_start, This->scheme_len);
            hres = S_OK;
        } else {
            *pbstrProperty = SysAllocStringLen(NULL, 0);
            hres = S_FALSE;
        }

        if(!(*pbstrProperty))
            hres = E_OUTOFMEMORY;

        break;
    case Uri_PROPERTY_USER_INFO:
        if(This->userinfo_start > -1) {
            *pbstrProperty = SysAllocStringLen(This->canon_uri+This->userinfo_start, This->userinfo_len);
            hres = S_OK;
        } else {
            *pbstrProperty = SysAllocStringLen(NULL, 0);
            hres = S_FALSE;
        }

        if(!(*pbstrProperty))
            hres = E_OUTOFMEMORY;

        break;
    case Uri_PROPERTY_USER_NAME:
        if(This->userinfo_start > -1 && This->userinfo_split != 0) {
            /* If userinfo_split is set, that means a password exists
             * so the username is only from userinfo_start to userinfo_split.
             */
            if(This->userinfo_split > -1) {
                *pbstrProperty = SysAllocStringLen(This->canon_uri + This->userinfo_start, This->userinfo_split);
                hres = S_OK;
            } else {
                *pbstrProperty = SysAllocStringLen(This->canon_uri + This->userinfo_start, This->userinfo_len);
                hres = S_OK;
            }
        } else {
            *pbstrProperty = SysAllocStringLen(NULL, 0);
            hres = S_FALSE;
        }

        if(!(*pbstrProperty))
            return E_OUTOFMEMORY;

        break;
    default:
        FIXME("(%p)->(%d %p %x)\n", This, uriProp, pbstrProperty, dwFlags);
        hres = E_NOTIMPL;
    }

    return hres;
}

static HRESULT WINAPI Uri_GetPropertyLength(IUri *iface, Uri_PROPERTY uriProp, DWORD *pcchProperty, DWORD dwFlags)
{
    Uri *This = URI_THIS(iface);
    HRESULT hres;
    TRACE("(%p)->(%d %p %x)\n", This, uriProp, pcchProperty, dwFlags);

    if(!pcchProperty)
        return E_INVALIDARG;

    /* Can only return a length for a property if it's a string. */
    if(uriProp > Uri_PROPERTY_STRING_LAST)
        return E_INVALIDARG;

    /* Don't have support for flags yet. */
    if(dwFlags) {
        FIXME("(%p)->(%d %p %x)\n", This, uriProp, pcchProperty, dwFlags);
        return E_NOTIMPL;
    }

    switch(uriProp) {
    case Uri_PROPERTY_ABSOLUTE_URI:
        if(This->display_modifiers & URI_DISPLAY_NO_ABSOLUTE_URI) {
            *pcchProperty = 0;
            hres = S_FALSE;
        } else {
            if(This->scheme_type != URL_SCHEME_UNKNOWN) {
                if(This->userinfo_start > -1 && This->userinfo_len == 0)
                    /* Don't include the '@' in the length. */
                    *pcchProperty = This->canon_len-1;
                else if(This->userinfo_start > -1 && This->userinfo_len == 1 &&
                        This->userinfo_split == 0)
                    /* Don't include the ":@" in the length. */
                    *pcchProperty = This->canon_len-2;
                else
                    *pcchProperty = This->canon_len;
            } else
                *pcchProperty = This->canon_len;

            hres = S_OK;
        }

        break;
    case Uri_PROPERTY_AUTHORITY:
        if(This->port_offset > -1 &&
           This->display_modifiers & URI_DISPLAY_NO_DEFAULT_PORT_AUTH &&
           is_default_port(This->scheme_type, This->port))
            /* Only count up until the port in the authority. */
            *pcchProperty = This->port_offset;
        else
            *pcchProperty = This->authority_len;
        hres = (This->authority_start > -1) ? S_OK : S_FALSE;
        break;
    case Uri_PROPERTY_DISPLAY_URI:
        if(This->scheme_type != URL_SCHEME_UNKNOWN && This->userinfo_start > -1)
            *pcchProperty = This->canon_len-This->userinfo_len-1;
        else
            *pcchProperty = This->canon_len;

        hres = S_OK;
        break;
    case Uri_PROPERTY_DOMAIN:
        if(This->domain_offset > -1)
            *pcchProperty = This->host_len - This->domain_offset;
        else
            *pcchProperty = 0;

        hres = (This->domain_offset > -1) ? S_OK : S_FALSE;
        break;
    case Uri_PROPERTY_EXTENSION:
        if(This->extension_offset > -1) {
            *pcchProperty = This->path_len - This->extension_offset;
            hres = S_OK;
        } else {
            *pcchProperty = 0;
            hres = S_FALSE;
        }

        break;
    case Uri_PROPERTY_FRAGMENT:
        *pcchProperty = This->fragment_len;
        hres = (This->fragment_start > -1) ? S_OK : S_FALSE;
        break;
    case Uri_PROPERTY_HOST:
        *pcchProperty = This->host_len;

        /* '[' and ']' aren't included in the length. */
        if(This->host_type == Uri_HOST_IPV6)
            *pcchProperty -= 2;

        hres = (This->host_start > -1) ? S_OK : S_FALSE;
        break;
    case Uri_PROPERTY_PASSWORD:
        *pcchProperty = (This->userinfo_split > -1) ? This->userinfo_len-This->userinfo_split-1 : 0;
        hres = (This->userinfo_split > -1) ? S_OK : S_FALSE;
        break;
    case Uri_PROPERTY_PATH:
        *pcchProperty = This->path_len;
        hres = (This->path_start > -1) ? S_OK : S_FALSE;
        break;
    case Uri_PROPERTY_PATH_AND_QUERY:
        *pcchProperty = This->path_len+This->query_len;
        hres = (This->path_start > -1 || This->query_start > -1) ? S_OK : S_FALSE;
        break;
    case Uri_PROPERTY_QUERY:
        *pcchProperty = This->query_len;
        hres = (This->query_start > -1) ? S_OK : S_FALSE;
        break;
    case Uri_PROPERTY_RAW_URI:
        *pcchProperty = SysStringLen(This->raw_uri);
        hres = S_OK;
        break;
    case Uri_PROPERTY_SCHEME_NAME:
        *pcchProperty = This->scheme_len;
        hres = (This->scheme_start > -1) ? S_OK : S_FALSE;
        break;
    case Uri_PROPERTY_USER_INFO:
        *pcchProperty = This->userinfo_len;
        hres = (This->userinfo_start > -1) ? S_OK : S_FALSE;
        break;
    case Uri_PROPERTY_USER_NAME:
        *pcchProperty = (This->userinfo_split > -1) ? This->userinfo_split : This->userinfo_len;
        if(This->userinfo_split == 0)
            hres = S_FALSE;
        else
            hres = (This->userinfo_start > -1) ? S_OK : S_FALSE;
        break;
    default:
        FIXME("(%p)->(%d %p %x)\n", This, uriProp, pcchProperty, dwFlags);
        hres = E_NOTIMPL;
    }

    return hres;
}

static HRESULT WINAPI Uri_GetPropertyDWORD(IUri *iface, Uri_PROPERTY uriProp, DWORD *pcchProperty, DWORD dwFlags)
{
    Uri *This = URI_THIS(iface);
    HRESULT hres;

    TRACE("(%p)->(%d %p %x)\n", This, uriProp, pcchProperty, dwFlags);

    if(!pcchProperty)
        return E_INVALIDARG;

    /* Microsoft's implementation for the ZONE property of a URI seems to be lacking...
     * From what I can tell, instead of checking which URLZONE the URI belongs to it
     * simply assigns URLZONE_INVALID and returns E_NOTIMPL. This also applies to the GetZone
     * function.
     */
    if(uriProp == Uri_PROPERTY_ZONE) {
        *pcchProperty = URLZONE_INVALID;
        return E_NOTIMPL;
    }

    if(uriProp < Uri_PROPERTY_DWORD_START) {
        *pcchProperty = 0;
        return E_INVALIDARG;
    }

    switch(uriProp) {
    case Uri_PROPERTY_HOST_TYPE:
        *pcchProperty = This->host_type;
        hres = S_OK;
        break;
    case Uri_PROPERTY_PORT:
        if(!This->has_port) {
            *pcchProperty = 0;
            hres = S_FALSE;
        } else {
            *pcchProperty = This->port;
            hres = S_OK;
        }

        break;
    case Uri_PROPERTY_SCHEME:
        *pcchProperty = This->scheme_type;
        hres = S_OK;
        break;
    default:
        FIXME("(%p)->(%d %p %x)\n", This, uriProp, pcchProperty, dwFlags);
        hres = E_NOTIMPL;
    }

    return hres;
}

static HRESULT WINAPI Uri_HasProperty(IUri *iface, Uri_PROPERTY uriProp, BOOL *pfHasProperty)
{
    Uri *This = URI_THIS(iface);
    TRACE("(%p)->(%d %p)\n", This, uriProp, pfHasProperty);

    if(!pfHasProperty)
        return E_INVALIDARG;

    switch(uriProp) {
    case Uri_PROPERTY_ABSOLUTE_URI:
        *pfHasProperty = !(This->display_modifiers & URI_DISPLAY_NO_ABSOLUTE_URI);
        break;
    case Uri_PROPERTY_AUTHORITY:
        *pfHasProperty = This->authority_start > -1;
        break;
    case Uri_PROPERTY_DISPLAY_URI:
        *pfHasProperty = TRUE;
        break;
    case Uri_PROPERTY_DOMAIN:
        *pfHasProperty = This->domain_offset > -1;
        break;
    case Uri_PROPERTY_EXTENSION:
        *pfHasProperty = This->extension_offset > -1;
        break;
    case Uri_PROPERTY_FRAGMENT:
        *pfHasProperty = This->fragment_start > -1;
        break;
    case Uri_PROPERTY_HOST:
        *pfHasProperty = This->host_start > -1;
        break;
    case Uri_PROPERTY_PASSWORD:
        *pfHasProperty = This->userinfo_split > -1;
        break;
    case Uri_PROPERTY_PATH:
        *pfHasProperty = This->path_start > -1;
        break;
    case Uri_PROPERTY_PATH_AND_QUERY:
        *pfHasProperty = (This->path_start > -1 || This->query_start > -1);
        break;
    case Uri_PROPERTY_QUERY:
        *pfHasProperty = This->query_start > -1;
        break;
    case Uri_PROPERTY_RAW_URI:
        *pfHasProperty = TRUE;
        break;
    case Uri_PROPERTY_SCHEME_NAME:
        *pfHasProperty = This->scheme_start > -1;
        break;
    case Uri_PROPERTY_USER_INFO:
        *pfHasProperty = This->userinfo_start > -1;
        break;
    case Uri_PROPERTY_USER_NAME:
        if(This->userinfo_split == 0)
            *pfHasProperty = FALSE;
        else
            *pfHasProperty = This->userinfo_start > -1;
        break;
    case Uri_PROPERTY_HOST_TYPE:
        *pfHasProperty = TRUE;
        break;
    case Uri_PROPERTY_PORT:
        *pfHasProperty = This->has_port;
        break;
    case Uri_PROPERTY_SCHEME:
        *pfHasProperty = TRUE;
        break;
    case Uri_PROPERTY_ZONE:
        *pfHasProperty = FALSE;
        break;
    default:
        FIXME("(%p)->(%d %p): Unsupported property type.\n", This, uriProp, pfHasProperty);
        return E_NOTIMPL;
    }

    return S_OK;
}

static HRESULT WINAPI Uri_GetAbsoluteUri(IUri *iface, BSTR *pstrAbsoluteUri)
{
    TRACE("(%p)->(%p)\n", iface, pstrAbsoluteUri);
    return Uri_GetPropertyBSTR(iface, Uri_PROPERTY_ABSOLUTE_URI, pstrAbsoluteUri, 0);
}

static HRESULT WINAPI Uri_GetAuthority(IUri *iface, BSTR *pstrAuthority)
{
    TRACE("(%p)->(%p)\n", iface, pstrAuthority);
    return Uri_GetPropertyBSTR(iface, Uri_PROPERTY_AUTHORITY, pstrAuthority, 0);
}

static HRESULT WINAPI Uri_GetDisplayUri(IUri *iface, BSTR *pstrDisplayUri)
{
    TRACE("(%p)->(%p)\n", iface, pstrDisplayUri);
    return Uri_GetPropertyBSTR(iface, Uri_PROPERTY_DISPLAY_URI, pstrDisplayUri, 0);
}

static HRESULT WINAPI Uri_GetDomain(IUri *iface, BSTR *pstrDomain)
{
    TRACE("(%p)->(%p)\n", iface, pstrDomain);
    return Uri_GetPropertyBSTR(iface, Uri_PROPERTY_DOMAIN, pstrDomain, 0);
}

static HRESULT WINAPI Uri_GetExtension(IUri *iface, BSTR *pstrExtension)
{
    TRACE("(%p)->(%p)\n", iface, pstrExtension);
    return Uri_GetPropertyBSTR(iface, Uri_PROPERTY_EXTENSION, pstrExtension, 0);
}

static HRESULT WINAPI Uri_GetFragment(IUri *iface, BSTR *pstrFragment)
{
    TRACE("(%p)->(%p)\n", iface, pstrFragment);
    return Uri_GetPropertyBSTR(iface, Uri_PROPERTY_FRAGMENT, pstrFragment, 0);
}

static HRESULT WINAPI Uri_GetHost(IUri *iface, BSTR *pstrHost)
{
    TRACE("(%p)->(%p)\n", iface, pstrHost);
    return Uri_GetPropertyBSTR(iface, Uri_PROPERTY_HOST, pstrHost, 0);
}

static HRESULT WINAPI Uri_GetPassword(IUri *iface, BSTR *pstrPassword)
{
    TRACE("(%p)->(%p)\n", iface, pstrPassword);
    return Uri_GetPropertyBSTR(iface, Uri_PROPERTY_PASSWORD, pstrPassword, 0);
}

static HRESULT WINAPI Uri_GetPath(IUri *iface, BSTR *pstrPath)
{
    TRACE("(%p)->(%p)\n", iface, pstrPath);
    return Uri_GetPropertyBSTR(iface, Uri_PROPERTY_PATH, pstrPath, 0);
}

static HRESULT WINAPI Uri_GetPathAndQuery(IUri *iface, BSTR *pstrPathAndQuery)
{
    TRACE("(%p)->(%p)\n", iface, pstrPathAndQuery);
    return Uri_GetPropertyBSTR(iface, Uri_PROPERTY_PATH_AND_QUERY, pstrPathAndQuery, 0);
}

static HRESULT WINAPI Uri_GetQuery(IUri *iface, BSTR *pstrQuery)
{
    TRACE("(%p)->(%p)\n", iface, pstrQuery);
    return Uri_GetPropertyBSTR(iface, Uri_PROPERTY_QUERY, pstrQuery, 0);
}

static HRESULT WINAPI Uri_GetRawUri(IUri *iface, BSTR *pstrRawUri)
{
    TRACE("(%p)->(%p)\n", iface, pstrRawUri);
    return Uri_GetPropertyBSTR(iface, Uri_PROPERTY_RAW_URI, pstrRawUri, 0);
}

static HRESULT WINAPI Uri_GetSchemeName(IUri *iface, BSTR *pstrSchemeName)
{
    TRACE("(%p)->(%p)\n", iface, pstrSchemeName);
    return Uri_GetPropertyBSTR(iface, Uri_PROPERTY_SCHEME_NAME, pstrSchemeName, 0);
}

static HRESULT WINAPI Uri_GetUserInfo(IUri *iface, BSTR *pstrUserInfo)
{
    TRACE("(%p)->(%p)\n", iface, pstrUserInfo);
    return Uri_GetPropertyBSTR(iface, Uri_PROPERTY_USER_INFO, pstrUserInfo, 0);
}

static HRESULT WINAPI Uri_GetUserName(IUri *iface, BSTR *pstrUserName)
{
    TRACE("(%p)->(%p)\n", iface, pstrUserName);
    return Uri_GetPropertyBSTR(iface, Uri_PROPERTY_USER_NAME, pstrUserName, 0);
}

static HRESULT WINAPI Uri_GetHostType(IUri *iface, DWORD *pdwHostType)
{
    TRACE("(%p)->(%p)\n", iface, pdwHostType);
    return Uri_GetPropertyDWORD(iface, Uri_PROPERTY_HOST_TYPE, pdwHostType, 0);
}

static HRESULT WINAPI Uri_GetPort(IUri *iface, DWORD *pdwPort)
{
    TRACE("(%p)->(%p)\n", iface, pdwPort);
    return Uri_GetPropertyDWORD(iface, Uri_PROPERTY_PORT, pdwPort, 0);
}

static HRESULT WINAPI Uri_GetScheme(IUri *iface, DWORD *pdwScheme)
{
    Uri *This = URI_THIS(iface);
    TRACE("(%p)->(%p)\n", This, pdwScheme);
    return Uri_GetPropertyDWORD(iface, Uri_PROPERTY_SCHEME, pdwScheme, 0);
}

static HRESULT WINAPI Uri_GetZone(IUri *iface, DWORD *pdwZone)
{
    TRACE("(%p)->(%p)\n", iface, pdwZone);
    return Uri_GetPropertyDWORD(iface, Uri_PROPERTY_ZONE,pdwZone, 0);
}

static HRESULT WINAPI Uri_GetProperties(IUri *iface, DWORD *pdwProperties)
{
    Uri *This = URI_THIS(iface);
    TRACE("(%p)->(%p)\n", This, pdwProperties);

    if(!pdwProperties)
        return E_INVALIDARG;

    /* All URIs have these. */
    *pdwProperties = Uri_HAS_DISPLAY_URI|Uri_HAS_RAW_URI|Uri_HAS_SCHEME|Uri_HAS_HOST_TYPE;

    if(!(This->display_modifiers & URI_DISPLAY_NO_ABSOLUTE_URI))
        *pdwProperties |= Uri_HAS_ABSOLUTE_URI;

    if(This->scheme_start > -1)
        *pdwProperties |= Uri_HAS_SCHEME_NAME;

    if(This->authority_start > -1) {
        *pdwProperties |= Uri_HAS_AUTHORITY;
        if(This->userinfo_start > -1) {
            *pdwProperties |= Uri_HAS_USER_INFO;
            if(This->userinfo_split != 0)
                *pdwProperties |= Uri_HAS_USER_NAME;
        }
        if(This->userinfo_split > -1)
            *pdwProperties |= Uri_HAS_PASSWORD;
        if(This->host_start > -1)
            *pdwProperties |= Uri_HAS_HOST;
        if(This->domain_offset > -1)
            *pdwProperties |= Uri_HAS_DOMAIN;
    }

    if(This->has_port)
        *pdwProperties |= Uri_HAS_PORT;
    if(This->path_start > -1)
        *pdwProperties |= Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY;
    if(This->query_start > -1)
        *pdwProperties |= Uri_HAS_QUERY|Uri_HAS_PATH_AND_QUERY;

    if(This->extension_offset > -1)
        *pdwProperties |= Uri_HAS_EXTENSION;

    if(This->fragment_start > -1)
        *pdwProperties |= Uri_HAS_FRAGMENT;

    return S_OK;
}

static HRESULT WINAPI Uri_IsEqual(IUri *iface, IUri *pUri, BOOL *pfEqual)
{
    Uri *This = URI_THIS(iface);
    Uri *other;

    TRACE("(%p)->(%p %p)\n", This, pUri, pfEqual);

    if(!pfEqual)
        return E_POINTER;

    if(!pUri) {
        *pfEqual = FALSE;

        /* For some reason Windows returns S_OK here... */
        return S_OK;
    }

    /* Try to convert it to a Uri (allows for a more simple comparison). */
    if((other = get_uri_obj(pUri)))
        *pfEqual = are_equal_simple(This, other);
    else {
        /* Do it the hard way. */
        FIXME("(%p)->(%p %p) No support for unknown IUri's yet.\n", iface, pUri, pfEqual);
        return E_NOTIMPL;
    }

    return S_OK;
}

#undef URI_THIS

static const IUriVtbl UriVtbl = {
    Uri_QueryInterface,
    Uri_AddRef,
    Uri_Release,
    Uri_GetPropertyBSTR,
    Uri_GetPropertyLength,
    Uri_GetPropertyDWORD,
    Uri_HasProperty,
    Uri_GetAbsoluteUri,
    Uri_GetAuthority,
    Uri_GetDisplayUri,
    Uri_GetDomain,
    Uri_GetExtension,
    Uri_GetFragment,
    Uri_GetHost,
    Uri_GetPassword,
    Uri_GetPath,
    Uri_GetPathAndQuery,
    Uri_GetQuery,
    Uri_GetRawUri,
    Uri_GetSchemeName,
    Uri_GetUserInfo,
    Uri_GetUserName,
    Uri_GetHostType,
    Uri_GetPort,
    Uri_GetScheme,
    Uri_GetZone,
    Uri_GetProperties,
    Uri_IsEqual
};

static Uri* create_uri_obj(void) {
    Uri *ret = heap_alloc_zero(sizeof(Uri));
    if(ret) {
        ret->lpIUriVtbl = &UriVtbl;
        ret->ref = 1;
    }

    return ret;
}

/***********************************************************************
 *           CreateUri (urlmon.@)
 *
 * Creates a new IUri object using the URI represented by pwzURI. This function
 * parses and validates the components of pwzURI and then canonicalizes the
 * parsed components.
 *
 * PARAMS
 *  pwzURI      [I] The URI to parse, validate, and canonicalize.
 *  dwFlags     [I] Flags which can affect how the parsing/canonicalization is performed.
 *  dwReserved  [I] Reserved (not used).
 *  ppURI       [O] The resulting IUri after parsing/canonicalization occurs.
 *
 * RETURNS
 *  Success: Returns S_OK. ppURI contains the pointer to the newly allocated IUri.
 *  Failure: E_INVALIDARG if there's invalid flag combinations in dwFlags, or an
 *           invalid parameters, or pwzURI doesn't represnt a valid URI.
 *           E_OUTOFMEMORY if any memory allocation fails.
 *
 * NOTES
 *  Default flags:
 *      Uri_CREATE_CANONICALIZE, Uri_CREATE_DECODE_EXTRA_INFO, Uri_CREATE_CRACK_UNKNOWN_SCHEMES,
 *      Uri_CREATE_PRE_PROCESS_HTML_URI, Uri_CREATE_NO_IE_SETTINGS.
 */
HRESULT WINAPI CreateUri(LPCWSTR pwzURI, DWORD dwFlags, DWORD_PTR dwReserved, IUri **ppURI)
{
    const DWORD supported_flags = Uri_CREATE_ALLOW_RELATIVE|Uri_CREATE_ALLOW_IMPLICIT_WILDCARD_SCHEME|
        Uri_CREATE_ALLOW_IMPLICIT_FILE_SCHEME|Uri_CREATE_NO_CANONICALIZE|Uri_CREATE_CANONICALIZE|
        Uri_CREATE_DECODE_EXTRA_INFO|Uri_CREATE_NO_DECODE_EXTRA_INFO|Uri_CREATE_CRACK_UNKNOWN_SCHEMES|
        Uri_CREATE_NO_CRACK_UNKNOWN_SCHEMES|Uri_CREATE_PRE_PROCESS_HTML_URI|Uri_CREATE_NO_PRE_PROCESS_HTML_URI|
        Uri_CREATE_NO_IE_SETTINGS|Uri_CREATE_NO_ENCODE_FORBIDDEN_CHARACTERS|Uri_CREATE_FILE_USE_DOS_PATH;
    Uri *ret;
    HRESULT hr;
    parse_data data;

    TRACE("(%s %x %x %p)\n", debugstr_w(pwzURI), dwFlags, (DWORD)dwReserved, ppURI);

    if(!ppURI)
        return E_INVALIDARG;

    if(!pwzURI || !*pwzURI) {
        *ppURI = NULL;
        return E_INVALIDARG;
    }

    /* Check for invalid flags. */
    if(has_invalid_flag_combination(dwFlags)) {
        *ppURI = NULL;
        return E_INVALIDARG;
    }

    /* Currently unsupported. */
    if(dwFlags & ~supported_flags)
        FIXME("Ignoring unsupported flag(s) %x\n", dwFlags & ~supported_flags);

    ret = create_uri_obj();
    if(!ret) {
        *ppURI = NULL;
        return E_OUTOFMEMORY;
    }

    /* Explicitly set the default flags if it doesn't cause a flag conflict. */
    apply_default_flags(&dwFlags);

    /* Pre process the URI, unless told otherwise. */
    if(!(dwFlags & Uri_CREATE_NO_PRE_PROCESS_HTML_URI))
        ret->raw_uri = pre_process_uri(pwzURI);
    else
        ret->raw_uri = SysAllocString(pwzURI);

    if(!ret->raw_uri) {
        heap_free(ret);
        return E_OUTOFMEMORY;
    }

    memset(&data, 0, sizeof(parse_data));
    data.uri = ret->raw_uri;

    /* Validate and parse the URI into it's components. */
    if(!parse_uri(&data, dwFlags)) {
        /* Encountered an unsupported or invalid URI */
        IUri_Release(URI(ret));
        *ppURI = NULL;
        return E_INVALIDARG;
    }

    /* Canonicalize the URI. */
    hr = canonicalize_uri(&data, ret, dwFlags);
    if(FAILED(hr)) {
        IUri_Release(URI(ret));
        *ppURI = NULL;
        return hr;
    }

    ret->create_flags = dwFlags;

    *ppURI = URI(ret);
    return S_OK;
}

/***********************************************************************
 *           CreateUriWithFragment (urlmon.@)
 *
 * Creates a new IUri object. This is almost the same as CreateUri, expect that
 * it allows you to explicitly specify a fragment (pwzFragment) for pwzURI.
 *
 * PARAMS
 *  pwzURI      [I] The URI to parse and perform canonicalization on.
 *  pwzFragment [I] The explict fragment string which should be added to pwzURI.
 *  dwFlags     [I] The flags which will be passed to CreateUri.
 *  dwReserved  [I] Reserved (not used).
 *  ppURI       [O] The resulting IUri after parsing/canonicalization.
 *
 * RETURNS
 *  Success: S_OK. ppURI contains the pointer to the newly allocated IUri.
 *  Failure: E_INVALIDARG if pwzURI already contains a fragment and pwzFragment
 *           isn't NULL. Will also return E_INVALIDARG for the same reasons as
 *           CreateUri will. E_OUTOFMEMORY if any allocations fail.
 */
HRESULT WINAPI CreateUriWithFragment(LPCWSTR pwzURI, LPCWSTR pwzFragment, DWORD dwFlags,
                                     DWORD_PTR dwReserved, IUri **ppURI)
{
    HRESULT hres;
    TRACE("(%s %s %x %x %p)\n", debugstr_w(pwzURI), debugstr_w(pwzFragment), dwFlags, (DWORD)dwReserved, ppURI);

    if(!ppURI)
        return E_INVALIDARG;

    if(!pwzURI) {
        *ppURI = NULL;
        return E_INVALIDARG;
    }

    /* Check if a fragment should be appended to the URI string. */
    if(pwzFragment) {
        WCHAR *uriW;
        DWORD uri_len, frag_len;
        BOOL add_pound;

        /* Check if the original URI already has a fragment component. */
        if(StrChrW(pwzURI, '#')) {
            *ppURI = NULL;
            return E_INVALIDARG;
        }

        uri_len = lstrlenW(pwzURI);
        frag_len = lstrlenW(pwzFragment);

        /* If the fragment doesn't start with a '#', one will be added. */
        add_pound = *pwzFragment != '#';

        if(add_pound)
            uriW = heap_alloc((uri_len+frag_len+2)*sizeof(WCHAR));
        else
            uriW = heap_alloc((uri_len+frag_len+1)*sizeof(WCHAR));

        if(!uriW)
            return E_OUTOFMEMORY;

        memcpy(uriW, pwzURI, uri_len*sizeof(WCHAR));
        if(add_pound)
            uriW[uri_len++] = '#';
        memcpy(uriW+uri_len, pwzFragment, (frag_len+1)*sizeof(WCHAR));

        hres = CreateUri(uriW, dwFlags, 0, ppURI);

        heap_free(uriW);
    } else
        /* A fragment string wasn't specified, so just forward the call. */
        hres = CreateUri(pwzURI, dwFlags, 0, ppURI);

    return hres;
}

static HRESULT build_uri(const UriBuilder *builder, IUri **uri, DWORD create_flags,
                         DWORD use_orig_flags, DWORD encoding_mask)
{
    HRESULT hr;
    parse_data data;
    Uri *ret;

    if(!uri)
        return E_POINTER;

    if(encoding_mask && (!builder->uri || builder->modified_props)) {
        *uri = NULL;
        return E_NOTIMPL;
    }

    /* Decide what flags should be used when creating the Uri. */
    if((use_orig_flags & UriBuilder_USE_ORIGINAL_FLAGS) && builder->uri)
        create_flags = builder->uri->create_flags;
    else {
        if(has_invalid_flag_combination(create_flags)) {
            *uri = NULL;
            return E_INVALIDARG;
        }

        /* Set the default flags if they don't cause a conflict. */
        apply_default_flags(&create_flags);
    }

    /* Return the base IUri if no changes have been made and the create_flags match. */
    if(builder->uri && !builder->modified_props && builder->uri->create_flags == create_flags) {
        *uri = URI(builder->uri);
        IUri_AddRef(*uri);
        return S_OK;
    }

    hr = validate_components(builder, &data, create_flags);
    if(FAILED(hr)) {
        *uri = NULL;
        return hr;
    }

    ret = create_uri_obj();
    if(!ret) {
        *uri = NULL;
        return E_OUTOFMEMORY;
    }

    hr = generate_uri(builder, &data, ret, create_flags);
    if(FAILED(hr)) {
        IUri_Release(URI(ret));
        *uri = NULL;
        return hr;
    }

    *uri = URI(ret);
    return S_OK;
}

#define URIBUILDER_THIS(iface) DEFINE_THIS(UriBuilder, IUriBuilder, iface)

static HRESULT WINAPI UriBuilder_QueryInterface(IUriBuilder *iface, REFIID riid, void **ppv)
{
    UriBuilder *This = URIBUILDER_THIS(iface);

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        TRACE("(%p)->(IID_IUnknown %p)\n", This, ppv);
        *ppv = URIBUILDER(This);
    }else if(IsEqualGUID(&IID_IUriBuilder, riid)) {
        TRACE("(%p)->(IID_IUri %p)\n", This, ppv);
        *ppv = URIBUILDER(This);
    }else {
        TRACE("(%p)->(%s %p)\n", This, debugstr_guid(riid), ppv);
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI UriBuilder_AddRef(IUriBuilder *iface)
{
    UriBuilder *This = URIBUILDER_THIS(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    return ref;
}

static ULONG WINAPI UriBuilder_Release(IUriBuilder *iface)
{
    UriBuilder *This = URIBUILDER_THIS(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    if(!ref) {
        if(This->uri) IUri_Release(URI(This->uri));
        heap_free(This->fragment);
        heap_free(This->host);
        heap_free(This->password);
        heap_free(This->path);
        heap_free(This->query);
        heap_free(This->scheme);
        heap_free(This->username);
        heap_free(This);
    }

    return ref;
}

static HRESULT WINAPI UriBuilder_CreateUriSimple(IUriBuilder *iface,
                                                 DWORD        dwAllowEncodingPropertyMask,
                                                 DWORD_PTR    dwReserved,
                                                 IUri       **ppIUri)
{
    UriBuilder *This = URIBUILDER_THIS(iface);
    HRESULT hr;
    TRACE("(%p)->(%d %d %p)\n", This, dwAllowEncodingPropertyMask, (DWORD)dwReserved, ppIUri);

    hr = build_uri(This, ppIUri, 0, UriBuilder_USE_ORIGINAL_FLAGS, dwAllowEncodingPropertyMask);
    if(hr == E_NOTIMPL)
        FIXME("(%p)->(%d %d %p)\n", This, dwAllowEncodingPropertyMask, (DWORD)dwReserved, ppIUri);
    return hr;
}

static HRESULT WINAPI UriBuilder_CreateUri(IUriBuilder *iface,
                                           DWORD        dwCreateFlags,
                                           DWORD        dwAllowEncodingPropertyMask,
                                           DWORD_PTR    dwReserved,
                                           IUri       **ppIUri)
{
    UriBuilder *This = URIBUILDER_THIS(iface);
    HRESULT hr;
    TRACE("(%p)->(0x%08x %d %d %p)\n", This, dwCreateFlags, dwAllowEncodingPropertyMask, (DWORD)dwReserved, ppIUri);

    if(dwCreateFlags == -1)
        hr = build_uri(This, ppIUri, 0, UriBuilder_USE_ORIGINAL_FLAGS, dwAllowEncodingPropertyMask);
    else
        hr = build_uri(This, ppIUri, dwCreateFlags, 0, dwAllowEncodingPropertyMask);

    if(hr == E_NOTIMPL)
        FIXME("(%p)->(0x%08x %d %d %p)\n", This, dwCreateFlags, dwAllowEncodingPropertyMask, (DWORD)dwReserved, ppIUri);
    return hr;
}

static HRESULT WINAPI UriBuilder_CreateUriWithFlags(IUriBuilder *iface,
                                         DWORD        dwCreateFlags,
                                         DWORD        dwUriBuilderFlags,
                                         DWORD        dwAllowEncodingPropertyMask,
                                         DWORD_PTR    dwReserved,
                                         IUri       **ppIUri)
{
    UriBuilder *This = URIBUILDER_THIS(iface);
    HRESULT hr;
    TRACE("(%p)->(0x%08x 0x%08x %d %d %p)\n", This, dwCreateFlags, dwUriBuilderFlags,
        dwAllowEncodingPropertyMask, (DWORD)dwReserved, ppIUri);

    hr = build_uri(This, ppIUri, dwCreateFlags, dwUriBuilderFlags, dwAllowEncodingPropertyMask);
    if(hr == E_NOTIMPL)
        FIXME("(%p)->(0x%08x 0x%08x %d %d %p)\n", This, dwCreateFlags, dwUriBuilderFlags,
            dwAllowEncodingPropertyMask, (DWORD)dwReserved, ppIUri);
    return hr;
}

static HRESULT WINAPI  UriBuilder_GetIUri(IUriBuilder *iface, IUri **ppIUri)
{
    UriBuilder *This = URIBUILDER_THIS(iface);
    TRACE("(%p)->(%p)\n", This, ppIUri);

    if(!ppIUri)
        return E_POINTER;

    if(This->uri) {
        IUri *uri = URI(This->uri);
        IUri_AddRef(uri);
        *ppIUri = uri;
    } else
        *ppIUri = NULL;

    return S_OK;
}

static HRESULT WINAPI UriBuilder_SetIUri(IUriBuilder *iface, IUri *pIUri)
{
    UriBuilder *This = URIBUILDER_THIS(iface);
    TRACE("(%p)->(%p)\n", This, pIUri);

    if(pIUri) {
        Uri *uri;

        if((uri = get_uri_obj(pIUri))) {
            /* Only reset the builder if it's Uri isn't the same as
             * the Uri passed to the function.
             */
            if(This->uri != uri) {
                reset_builder(This);

                This->uri = uri;
                if(uri->has_port)
                    This->port = uri->port;

                IUri_AddRef(pIUri);
            }
        } else {
            FIXME("(%p)->(%p) Unknown IUri types not supported yet.\n", This, pIUri);
            return E_NOTIMPL;
        }
    } else if(This->uri)
        /* Only reset the builder if it's Uri isn't NULL. */
        reset_builder(This);

    return S_OK;
}

static HRESULT WINAPI UriBuilder_GetFragment(IUriBuilder *iface, DWORD *pcchFragment, LPCWSTR *ppwzFragment)
{
    UriBuilder *This = URIBUILDER_THIS(iface);
    TRACE("(%p)->(%p %p)\n", This, pcchFragment, ppwzFragment);

    if(!This->uri || This->uri->fragment_start == -1 || This->modified_props & Uri_HAS_FRAGMENT)
        return get_builder_component(&This->fragment, &This->fragment_len, NULL, 0, ppwzFragment, pcchFragment);
    else
        return get_builder_component(&This->fragment, &This->fragment_len, This->uri->canon_uri+This->uri->fragment_start,
                                     This->uri->fragment_len, ppwzFragment, pcchFragment);
}

static HRESULT WINAPI UriBuilder_GetHost(IUriBuilder *iface, DWORD *pcchHost, LPCWSTR *ppwzHost)
{
    UriBuilder *This = URIBUILDER_THIS(iface);
    TRACE("(%p)->(%p %p)\n", This, pcchHost, ppwzHost);

    if(!This->uri || This->uri->host_start == -1 || This->modified_props & Uri_HAS_HOST)
        return get_builder_component(&This->host, &This->host_len, NULL, 0, ppwzHost, pcchHost);
    else {
        if(This->uri->host_type == Uri_HOST_IPV6)
            /* Don't include the '[' and ']' around the address. */
            return get_builder_component(&This->host, &This->host_len, This->uri->canon_uri+This->uri->host_start+1,
                                         This->uri->host_len-2, ppwzHost, pcchHost);
        else
            return get_builder_component(&This->host, &This->host_len, This->uri->canon_uri+This->uri->host_start,
                                         This->uri->host_len, ppwzHost, pcchHost);
    }
}

static HRESULT WINAPI UriBuilder_GetPassword(IUriBuilder *iface, DWORD *pcchPassword, LPCWSTR *ppwzPassword)
{
    UriBuilder *This = URIBUILDER_THIS(iface);
    TRACE("(%p)->(%p %p)\n", This, pcchPassword, ppwzPassword);

    if(!This->uri || This->uri->userinfo_split == -1 || This->modified_props & Uri_HAS_PASSWORD)
        return get_builder_component(&This->password, &This->password_len, NULL, 0, ppwzPassword, pcchPassword);
    else {
        const WCHAR *start = This->uri->canon_uri+This->uri->userinfo_start+This->uri->userinfo_split+1;
        DWORD len = This->uri->userinfo_len-This->uri->userinfo_split-1;
        return get_builder_component(&This->password, &This->password_len, start, len, ppwzPassword, pcchPassword);
    }
}

static HRESULT WINAPI UriBuilder_GetPath(IUriBuilder *iface, DWORD *pcchPath, LPCWSTR *ppwzPath)
{
    UriBuilder *This = URIBUILDER_THIS(iface);
    TRACE("(%p)->(%p %p)\n", This, pcchPath, ppwzPath);

    if(!This->uri || This->uri->path_start == -1 || This->modified_props & Uri_HAS_PATH)
        return get_builder_component(&This->path, &This->path_len, NULL, 0, ppwzPath, pcchPath);
    else
        return get_builder_component(&This->path, &This->path_len, This->uri->canon_uri+This->uri->path_start,
                                     This->uri->path_len, ppwzPath, pcchPath);
}

static HRESULT WINAPI UriBuilder_GetPort(IUriBuilder *iface, BOOL *pfHasPort, DWORD *pdwPort)
{
    UriBuilder *This = URIBUILDER_THIS(iface);
    TRACE("(%p)->(%p %p)\n", This, pfHasPort, pdwPort);

    if(!pfHasPort) {
        if(pdwPort)
            *pdwPort = 0;
        return E_POINTER;
    }

    if(!pdwPort) {
        *pfHasPort = FALSE;
        return E_POINTER;
    }

    *pfHasPort = This->has_port;
    *pdwPort = This->port;
    return S_OK;
}

static HRESULT WINAPI UriBuilder_GetQuery(IUriBuilder *iface, DWORD *pcchQuery, LPCWSTR *ppwzQuery)
{
    UriBuilder *This = URIBUILDER_THIS(iface);
    TRACE("(%p)->(%p %p)\n", This, pcchQuery, ppwzQuery);

    if(!This->uri || This->uri->query_start == -1 || This->modified_props & Uri_HAS_QUERY)
        return get_builder_component(&This->query, &This->query_len, NULL, 0, ppwzQuery, pcchQuery);
    else
        return get_builder_component(&This->query, &This->query_len, This->uri->canon_uri+This->uri->query_start,
                                     This->uri->query_len, ppwzQuery, pcchQuery);
}

static HRESULT WINAPI UriBuilder_GetSchemeName(IUriBuilder *iface, DWORD *pcchSchemeName, LPCWSTR *ppwzSchemeName)
{
    UriBuilder *This = URIBUILDER_THIS(iface);
    TRACE("(%p)->(%p %p)\n", This, pcchSchemeName, ppwzSchemeName);

    if(!This->uri || This->uri->scheme_start == -1 || This->modified_props & Uri_HAS_SCHEME_NAME)
        return get_builder_component(&This->scheme, &This->scheme_len, NULL, 0, ppwzSchemeName, pcchSchemeName);
    else
        return get_builder_component(&This->scheme, &This->scheme_len, This->uri->canon_uri+This->uri->scheme_start,
                                     This->uri->scheme_len, ppwzSchemeName, pcchSchemeName);
}

static HRESULT WINAPI UriBuilder_GetUserName(IUriBuilder *iface, DWORD *pcchUserName, LPCWSTR *ppwzUserName)
{
    UriBuilder *This = URIBUILDER_THIS(iface);
    TRACE("(%p)->(%p %p)\n", This, pcchUserName, ppwzUserName);

    if(!This->uri || This->uri->userinfo_start == -1 || This->uri->userinfo_split == 0 ||
       This->modified_props & Uri_HAS_USER_NAME)
        return get_builder_component(&This->username, &This->username_len, NULL, 0, ppwzUserName, pcchUserName);
    else {
        const WCHAR *start = This->uri->canon_uri+This->uri->userinfo_start;

        /* Check if there's a password in the userinfo section. */
        if(This->uri->userinfo_split > -1)
            /* Don't include the password. */
            return get_builder_component(&This->username, &This->username_len, start,
                                         This->uri->userinfo_split, ppwzUserName, pcchUserName);
        else
            return get_builder_component(&This->username, &This->username_len, start,
                                         This->uri->userinfo_len, ppwzUserName, pcchUserName);
    }
}

static HRESULT WINAPI UriBuilder_SetFragment(IUriBuilder *iface, LPCWSTR pwzNewValue)
{
    UriBuilder *This = URIBUILDER_THIS(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_w(pwzNewValue));
    return set_builder_component(&This->fragment, &This->fragment_len, pwzNewValue, '#',
                                 &This->modified_props, Uri_HAS_FRAGMENT);
}

static HRESULT WINAPI UriBuilder_SetHost(IUriBuilder *iface, LPCWSTR pwzNewValue)
{
    UriBuilder *This = URIBUILDER_THIS(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_w(pwzNewValue));

    /* Host name can't be set to NULL. */
    if(!pwzNewValue)
        return E_INVALIDARG;

    return set_builder_component(&This->host, &This->host_len, pwzNewValue, 0,
                                 &This->modified_props, Uri_HAS_HOST);
}

static HRESULT WINAPI UriBuilder_SetPassword(IUriBuilder *iface, LPCWSTR pwzNewValue)
{
    UriBuilder *This = URIBUILDER_THIS(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_w(pwzNewValue));
    return set_builder_component(&This->password, &This->password_len, pwzNewValue, 0,
                                 &This->modified_props, Uri_HAS_PASSWORD);
}

static HRESULT WINAPI UriBuilder_SetPath(IUriBuilder *iface, LPCWSTR pwzNewValue)
{
    UriBuilder *This = URIBUILDER_THIS(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_w(pwzNewValue));
    return set_builder_component(&This->path, &This->path_len, pwzNewValue, 0,
                                 &This->modified_props, Uri_HAS_PATH);
}

static HRESULT WINAPI UriBuilder_SetPort(IUriBuilder *iface, BOOL fHasPort, DWORD dwNewValue)
{
    UriBuilder *This = URIBUILDER_THIS(iface);
    TRACE("(%p)->(%d %d)\n", This, fHasPort, dwNewValue);

    This->has_port = fHasPort;
    This->port = dwNewValue;
    This->modified_props |= Uri_HAS_PORT;
    return S_OK;
}

static HRESULT WINAPI UriBuilder_SetQuery(IUriBuilder *iface, LPCWSTR pwzNewValue)
{
    UriBuilder *This = URIBUILDER_THIS(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_w(pwzNewValue));
    return set_builder_component(&This->query, &This->query_len, pwzNewValue, '?',
                                 &This->modified_props, Uri_HAS_QUERY);
}

static HRESULT WINAPI UriBuilder_SetSchemeName(IUriBuilder *iface, LPCWSTR pwzNewValue)
{
    UriBuilder *This = URIBUILDER_THIS(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_w(pwzNewValue));

    /* Only set the scheme name if it's not NULL or empty. */
    if(!pwzNewValue || !*pwzNewValue)
        return E_INVALIDARG;

    return set_builder_component(&This->scheme, &This->scheme_len, pwzNewValue, 0,
                                 &This->modified_props, Uri_HAS_SCHEME_NAME);
}

static HRESULT WINAPI UriBuilder_SetUserName(IUriBuilder *iface, LPCWSTR pwzNewValue)
{
    UriBuilder *This = URIBUILDER_THIS(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_w(pwzNewValue));
    return set_builder_component(&This->username, &This->username_len, pwzNewValue, 0,
                                 &This->modified_props, Uri_HAS_USER_NAME);
}

static HRESULT WINAPI UriBuilder_RemoveProperties(IUriBuilder *iface, DWORD dwPropertyMask)
{
    const DWORD accepted_flags = Uri_HAS_AUTHORITY|Uri_HAS_DOMAIN|Uri_HAS_EXTENSION|Uri_HAS_FRAGMENT|Uri_HAS_HOST|
                                 Uri_HAS_PASSWORD|Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_QUERY|
                                 Uri_HAS_USER_INFO|Uri_HAS_USER_NAME;

    UriBuilder *This = URIBUILDER_THIS(iface);
    TRACE("(%p)->(0x%08x)\n", This, dwPropertyMask);

    if(dwPropertyMask & ~accepted_flags)
        return E_INVALIDARG;

    if(dwPropertyMask & Uri_HAS_FRAGMENT)
        UriBuilder_SetFragment(iface, NULL);

    /* Even though you can't set the host name to NULL or an
     * empty string, you can still remove it... for some reason.
     */
    if(dwPropertyMask & Uri_HAS_HOST)
        set_builder_component(&This->host, &This->host_len, NULL, 0,
                              &This->modified_props, Uri_HAS_HOST);

    if(dwPropertyMask & Uri_HAS_PASSWORD)
        UriBuilder_SetPassword(iface, NULL);

    if(dwPropertyMask & Uri_HAS_PATH)
        UriBuilder_SetPath(iface, NULL);

    if(dwPropertyMask & Uri_HAS_PORT)
        UriBuilder_SetPort(iface, FALSE, 0);

    if(dwPropertyMask & Uri_HAS_QUERY)
        UriBuilder_SetQuery(iface, NULL);

    if(dwPropertyMask & Uri_HAS_USER_NAME)
        UriBuilder_SetUserName(iface, NULL);

    return S_OK;
}

static HRESULT WINAPI UriBuilder_HasBeenModified(IUriBuilder *iface, BOOL *pfModified)
{
    UriBuilder *This = URIBUILDER_THIS(iface);
    TRACE("(%p)->(%p)\n", This, pfModified);

    if(!pfModified)
        return E_POINTER;

    *pfModified = This->modified_props > 0;
    return S_OK;
}

#undef URIBUILDER_THIS

static const IUriBuilderVtbl UriBuilderVtbl = {
    UriBuilder_QueryInterface,
    UriBuilder_AddRef,
    UriBuilder_Release,
    UriBuilder_CreateUriSimple,
    UriBuilder_CreateUri,
    UriBuilder_CreateUriWithFlags,
    UriBuilder_GetIUri,
    UriBuilder_SetIUri,
    UriBuilder_GetFragment,
    UriBuilder_GetHost,
    UriBuilder_GetPassword,
    UriBuilder_GetPath,
    UriBuilder_GetPort,
    UriBuilder_GetQuery,
    UriBuilder_GetSchemeName,
    UriBuilder_GetUserName,
    UriBuilder_SetFragment,
    UriBuilder_SetHost,
    UriBuilder_SetPassword,
    UriBuilder_SetPath,
    UriBuilder_SetPort,
    UriBuilder_SetQuery,
    UriBuilder_SetSchemeName,
    UriBuilder_SetUserName,
    UriBuilder_RemoveProperties,
    UriBuilder_HasBeenModified,
};

/***********************************************************************
 *           CreateIUriBuilder (urlmon.@)
 */
HRESULT WINAPI CreateIUriBuilder(IUri *pIUri, DWORD dwFlags, DWORD_PTR dwReserved, IUriBuilder **ppIUriBuilder)
{
    UriBuilder *ret;

    TRACE("(%p %x %x %p)\n", pIUri, dwFlags, (DWORD)dwReserved, ppIUriBuilder);

    if(!ppIUriBuilder)
        return E_POINTER;

    ret = heap_alloc_zero(sizeof(UriBuilder));
    if(!ret)
        return E_OUTOFMEMORY;

    ret->lpIUriBuilderVtbl = &UriBuilderVtbl;
    ret->ref = 1;

    if(pIUri) {
        Uri *uri;

        if((uri = get_uri_obj(pIUri))) {
            IUri_AddRef(pIUri);
            ret->uri = uri;

            if(uri->has_port)
                /* Windows doesn't set 'has_port' to TRUE in this case. */
                ret->port = uri->port;

        } else {
            heap_free(ret);
            *ppIUriBuilder = NULL;
            FIXME("(%p %x %x %p): Unknown IUri types not supported yet.\n", pIUri, dwFlags,
                (DWORD)dwReserved, ppIUriBuilder);
            return E_NOTIMPL;
        }
    }

    *ppIUriBuilder = URIBUILDER(ret);
    return S_OK;
}

/* Merges the base path with the relative path and stores the resulting path
 * and path len in 'result' and 'result_len'.
 */
static HRESULT merge_paths(parse_data *data, const WCHAR *base, DWORD base_len, const WCHAR *relative,
                           DWORD relative_len, WCHAR **result, DWORD *result_len, DWORD flags)
{
    const WCHAR *end = NULL;
    DWORD base_copy_len = 0;
    WCHAR *ptr;

    if(base_len) {
        /* Find the characters the will be copied over from
         * the base path.
         */
        end = str_last_of(base, base+(base_len-1), '/');
        if(!end && data->scheme_type == URL_SCHEME_FILE)
            /* Try looking for a '\\'. */
            end = str_last_of(base, base+(base_len-1), '\\');
    }

    if(end) {
        base_copy_len = (end+1)-base;
        *result = heap_alloc((base_copy_len+relative_len+1)*sizeof(WCHAR));
    } else
        *result = heap_alloc((relative_len+1)*sizeof(WCHAR));

    if(!(*result)) {
        *result_len = 0;
        return E_OUTOFMEMORY;
    }

    ptr = *result;
    if(end) {
        memcpy(ptr, base, base_copy_len*sizeof(WCHAR));
        ptr += base_copy_len;
    }

    memcpy(ptr, relative, relative_len*sizeof(WCHAR));
    ptr += relative_len;
    *ptr = '\0';

    *result_len = (ptr-*result);
    return S_OK;
}

static HRESULT combine_uri(Uri *base, Uri *relative, DWORD flags, IUri **result) {
    Uri *ret;
    HRESULT hr;
    parse_data data;
    DWORD create_flags = 0, len = 0;

    memset(&data, 0, sizeof(parse_data));

    /* Base case is when the relative Uri has a scheme name,
     * if it does, then 'result' will contain the same data
     * as the relative Uri.
     */
    if(relative->scheme_start > -1) {
        data.uri = SysAllocString(relative->raw_uri);
        if(!data.uri) {
            *result = NULL;
            return E_OUTOFMEMORY;
        }

        parse_uri(&data, 0);

        ret = create_uri_obj();
        if(!ret) {
            *result = NULL;
            return E_OUTOFMEMORY;
        }

        ret->raw_uri = data.uri;
        hr = canonicalize_uri(&data, ret, 0);
        if(FAILED(hr)) {
            IUri_Release(URI(ret));
            *result = NULL;
            return hr;
        }

        apply_default_flags(&create_flags);
        ret->create_flags = create_flags;

        *result = URI(ret);
    } else {
        WCHAR *path = NULL;
        DWORD raw_flags = 0;

        if(base->scheme_start > -1) {
            data.scheme = base->canon_uri+base->scheme_start;
            data.scheme_len = base->scheme_len;
            data.scheme_type = base->scheme_type;
        } else {
            data.is_relative = TRUE;
            data.scheme_type = URL_SCHEME_UNKNOWN;
            create_flags |= Uri_CREATE_ALLOW_RELATIVE;
        }

        if(base->authority_start > -1) {
            if(base->userinfo_start > -1 && base->userinfo_split != 0) {
                data.username = base->canon_uri+base->userinfo_start;
                data.username_len = (base->userinfo_split > -1) ? base->userinfo_split : base->userinfo_len;
            }

            if(base->userinfo_split > -1) {
                data.password = base->canon_uri+base->userinfo_start+base->userinfo_split+1;
                data.password_len = base->userinfo_len-base->userinfo_split-1;
            }

            if(base->host_start > -1) {
                data.host = base->canon_uri+base->host_start;
                data.host_len = base->host_len;
                data.host_type = base->host_type;
            }

            if(base->has_port) {
                data.has_port = TRUE;
                data.port_value = base->port;
            }
        } else if(base->scheme_type != URL_SCHEME_FILE)
            data.is_opaque = TRUE;

        if(relative->path_start == -1 || !relative->path_len) {
            if(base->path_start > -1) {
                data.path = base->canon_uri+base->path_start;
                data.path_len = base->path_len;
            } else if((base->path_start == -1 || !base->path_len) && !data.is_opaque) {
                /* Just set the path as a '/' if the base didn't have
                 * one and if it's an hierarchical URI.
                 */
                static const WCHAR slashW[] = {'/',0};
                data.path = slashW;
                data.path_len = 1;
            }

            if(relative->query_start > -1) {
                data.query = relative->canon_uri+relative->query_start;
                data.query_len = relative->query_len;
            } else if(base->query_start > -1) {
                data.query = base->canon_uri+base->query_start;
                data.query_len = base->query_len;
            }
        } else {
            const WCHAR *ptr, **pptr;
            DWORD path_offset = 0, path_len = 0;

            /* There's two possibilities on what will happen to the path component
             * of the result IUri. First, if the relative path begins with a '/'
             * then the resulting path will just be the relative path. Second, if
             * relative path doesn't begin with a '/' then the base path and relative
             * path are merged together.
             */
            if(relative->path_len && *(relative->canon_uri+relative->path_start) == '/') {
                WCHAR *tmp = NULL;
                BOOL copy_drive_path = FALSE;

                /* If the relative IUri's path starts with a '/', then we
                 * don't use the base IUri's path. Unless the base IUri
                 * is a file URI, in which case it uses the drive path of
                 * the base IUri (if it has any) in the new path.
                 */
                if(base->scheme_type == URL_SCHEME_FILE) {
                    if(base->path_len > 3 && *(base->canon_uri+base->path_start) == '/' &&
                       is_drive_path(base->canon_uri+base->path_start+1)) {
                        path_len += 3;
                        copy_drive_path = TRUE;
                    }
                }

                path_len += relative->path_len;

                path = heap_alloc((path_len+1)*sizeof(WCHAR));
                if(!path) {
                    *result = NULL;
                    return E_OUTOFMEMORY;
                }

                tmp = path;

                /* Copy the base paths, drive path over. */
                if(copy_drive_path) {
                    memcpy(tmp, base->canon_uri+base->path_start, 3*sizeof(WCHAR));
                    tmp += 3;
                }

                memcpy(tmp, relative->canon_uri+relative->path_start, relative->path_len*sizeof(WCHAR));
                path[path_len] = '\0';
            } else {
                /* Merge the base path with the relative path. */
                hr = merge_paths(&data, base->canon_uri+base->path_start, base->path_len,
                                 relative->canon_uri+relative->path_start, relative->path_len,
                                 &path, &path_len, flags);
                if(FAILED(hr)) {
                    *result = NULL;
                    return hr;
                }

                /* If the resulting IUri is a file URI, the drive path isn't
                 * reduced out when the dot segments are removed.
                 */
                if(path_len >= 3 && data.scheme_type == URL_SCHEME_FILE && !data.host) {
                    if(*path == '/' && is_drive_path(path+1))
                        path_offset = 2;
                    else if(is_drive_path(path))
                        path_offset = 1;
                }
            }

            /* Check if the dot segments need to be removed from the path. */
            if(!(flags & URL_DONT_SIMPLIFY) && !data.is_opaque) {
                DWORD offset = (path_offset > 0) ? path_offset+1 : 0;
                DWORD new_len = remove_dot_segments(path+offset,path_len-offset);

                if(new_len != path_len) {
                    WCHAR *tmp = heap_realloc(path, (path_offset+new_len+1)*sizeof(WCHAR));
                    if(!tmp) {
                        heap_free(path);
                        *result = NULL;
                        return E_OUTOFMEMORY;
                    }

                    tmp[new_len+offset] = '\0';
                    path = tmp;
                    path_len = new_len+offset;
                }
            }

            /* Make sure the path component is valid. */
            ptr = path;
            pptr = &ptr;
            if((data.is_opaque && !parse_path_opaque(pptr, &data, 0)) ||
               (!data.is_opaque && !parse_path_hierarchical(pptr, &data, 0))) {
                heap_free(path);
                *result = NULL;
                return E_INVALIDARG;
            }
        }

        if(relative->fragment_start > -1) {
            data.fragment = relative->canon_uri+relative->fragment_start;
            data.fragment_len = relative->fragment_len;
        }

        if(flags & URL_DONT_SIMPLIFY)
            raw_flags |= RAW_URI_FORCE_PORT_DISP;
        if(flags & URL_FILE_USE_PATHURL)
            raw_flags |= RAW_URI_CONVERT_TO_DOS_PATH;

        len = generate_raw_uri(&data, data.uri, raw_flags);
        data.uri = SysAllocStringLen(NULL, len);
        if(!data.uri) {
            heap_free(path);
            *result = NULL;
            return E_OUTOFMEMORY;
        }

        generate_raw_uri(&data, data.uri, raw_flags);

        ret = create_uri_obj();
        if(!ret) {
            SysFreeString(data.uri);
            heap_free(path);
            *result = NULL;
            return E_OUTOFMEMORY;
        }

        if(flags & URL_DONT_SIMPLIFY)
            create_flags |= Uri_CREATE_NO_CANONICALIZE;
        if(flags & URL_FILE_USE_PATHURL)
            create_flags |= Uri_CREATE_FILE_USE_DOS_PATH;

        ret->raw_uri = data.uri;
        hr = canonicalize_uri(&data, ret, create_flags);
        if(FAILED(hr)) {
            IUri_Release(URI(ret));
            *result = NULL;
            return hr;
        }

        if(flags & URL_DONT_SIMPLIFY)
            ret->display_modifiers |= URI_DISPLAY_NO_DEFAULT_PORT_AUTH;

        apply_default_flags(&create_flags);
        ret->create_flags = create_flags;
        *result = URI(ret);

        heap_free(path);
    }

    return S_OK;
}

/***********************************************************************
 *           CoInternetCombineIUri (urlmon.@)
 */
HRESULT WINAPI CoInternetCombineIUri(IUri *pBaseUri, IUri *pRelativeUri, DWORD dwCombineFlags,
                                     IUri **ppCombinedUri, DWORD_PTR dwReserved)
{
    HRESULT hr;
    Uri *relative, *base;
    TRACE("(%p %p %x %p %x)\n", pBaseUri, pRelativeUri, dwCombineFlags, ppCombinedUri, (DWORD)dwReserved);

    if(!ppCombinedUri)
        return E_INVALIDARG;

    if(!pBaseUri || !pRelativeUri) {
        *ppCombinedUri = NULL;
        return E_INVALIDARG;
    }

    relative = get_uri_obj(pRelativeUri);
    base = get_uri_obj(pBaseUri);
    if(!relative || !base) {
        *ppCombinedUri = NULL;
        FIXME("(%p %p %x %p %x) Unknown IUri types not supported yet.\n",
            pBaseUri, pRelativeUri, dwCombineFlags, ppCombinedUri, (DWORD)dwReserved);
        return E_NOTIMPL;
    }

    hr = combine_uri(base, relative, dwCombineFlags, ppCombinedUri);
    if(hr == E_NOTIMPL)
        FIXME("(%p %p %x %p %x): stub\n", pBaseUri, pRelativeUri, dwCombineFlags, ppCombinedUri, (DWORD)dwReserved);
    return hr;
}
