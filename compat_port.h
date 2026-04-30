#ifndef COMPAT_PORT_H
#define COMPAT_PORT_H

#ifndef SQLITE_THREADSAFE
#define SQLITE_THREADSAFE 0
#endif

#ifndef _WIN32

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef __STDC_WANT_LIB_EXT1__
#define __STDC_WANT_LIB_EXT1__ 1
#endif

#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Provide a portable strnlen if the platform header doesn't declare one. */
#ifndef HAVE_STRNLEN
static inline size_t strnlen(const char *s, size_t maxlen)
{
    const char *end = memchr(s, '\0', maxlen);
    return end ? (size_t)(end - s) : maxlen;
}
#endif

typedef int errno_t;
typedef size_t rsize_t;

/* Some strict C environments hide POSIX prototypes unless feature macros are set. */
struct tm *localtime_r(const time_t *timep, struct tm *result);

static inline char *compat_strdup_impl(const char *s)
{
    if (!s)
    {
        return NULL;
    }

    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (!p)
    {
        return NULL;
    }
    memcpy(p, s, n);
    return p;
}

#ifndef strdup
#define strdup compat_strdup_impl
#endif

#ifndef _TRUNCATE
#define _TRUNCATE ((size_t)-1)
#endif

#ifndef _countof
#define _countof(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#ifndef __STDC_LIB_EXT1__
static inline int fopen_s(FILE **fp, const char *filename, const char *mode)
{
    if (!fp || !filename || !mode)
    {
        return EINVAL;
    }
    *fp = fopen(filename, mode);
    return (*fp != NULL) ? 0 : errno;
}

static inline int strcpy_s(char *dest, size_t destsz, const char *src)
{
    if (!dest || !src || destsz == 0)
    {
        return EINVAL;
    }

    size_t len = strlen(src);
    if (len >= destsz)
    {
        dest[0] = '\0';
        return ERANGE;
    }

    memcpy(dest, src, len + 1);
    return 0;
}

static inline int strcat_s(char *dest, size_t destsz, const char *src)
{
    if (!dest || !src || destsz == 0)
    {
        return EINVAL;
    }

    size_t dlen = strnlen(dest, destsz);
    if (dlen >= destsz)
    {
        return ERANGE;
    }

    size_t remaining = destsz - dlen;
    size_t slen = strlen(src);
    if (slen >= remaining)
    {
        dest[0] = '\0';
        return ERANGE;
    }

    memcpy(dest + dlen, src, slen + 1);
    return 0;
}

static inline int strncpy_s(char *dest, size_t destsz, const char *src, size_t count)
{
    if (!dest || !src || destsz == 0)
    {
        return EINVAL;
    }

    size_t copy_len = 0;
    if (count == _TRUNCATE)
    {
        copy_len = strnlen(src, destsz - 1);
    }
    else
    {
        copy_len = strnlen(src, count);
        if (copy_len >= destsz)
        {
            dest[0] = '\0';
            return ERANGE;
        }
    }

    memcpy(dest, src, copy_len);
    dest[copy_len] = '\0';
    return 0;
}

static inline int strncat_s(char *dest, size_t destsz, const char *src, size_t count)
{
    if (!dest || !src || destsz == 0)
    {
        return EINVAL;
    }

    size_t dlen = strnlen(dest, destsz);
    if (dlen >= destsz)
    {
        return ERANGE;
    }

    size_t remaining = destsz - dlen - 1;
    size_t to_copy = 0;
    if (count == _TRUNCATE)
    {
        to_copy = strnlen(src, remaining);
    }
    else
    {
        to_copy = strnlen(src, count);
        if (to_copy > remaining)
        {
            return ERANGE;
        }
    }

    memcpy(dest + dlen, src, to_copy);
    dest[dlen + to_copy] = '\0';
    return 0;
}

static inline int strerror_s(char *buffer, size_t numberOfElements, int errnum)
{
    if (!buffer || numberOfElements == 0)
    {
        return EINVAL;
    }

    /* Use the most portable strerror_r handling possible:
     * - glibc with _GNU_SOURCE: strerror_r returns char * (GNU variant)
     * - POSIX: strerror_r returns int
     * - fallback: use strerror() (not thread-safe) if strerror_r is unavailable
     */
#if defined(__GLIBC__) && defined(_GNU_SOURCE)
    char *msg = strerror_r(errnum, buffer, numberOfElements);
    if (msg != buffer && msg != NULL)
    {
        size_t len = strnlen(msg, numberOfElements - 1);
        memcpy(buffer, msg, len);
        buffer[len] = '\0';
    }
    return 0;
#elif defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE)
    int rc = strerror_r(errnum, buffer, numberOfElements);
    if (rc != 0)
    {
        buffer[0] = '\0';
    }
    return rc;
#else
    const char *msg = strerror(errnum);
    if (!msg)
    {
        buffer[0] = '\0';
        return EINVAL;
    }
    size_t len = strnlen(msg, numberOfElements - 1);
    memcpy(buffer, msg, len);
    buffer[len] = '\0';
    return 0;
#endif
}

static inline int localtime_s(struct tm *tm_out, const time_t *time_in)
{
    if (!tm_out || !time_in)
    {
        return EINVAL;
    }

    return (localtime_r(time_in, tm_out) != NULL) ? 0 : errno;
}

static inline size_t strlen_s(const char *s, size_t maxlen)
{
    if (!s || maxlen == 0)
    {
        return 0;
    }

    size_t len = 0;
    while (s[len] != '\0' && len < maxlen)
    {
        len++;
    }
    return len;
}
#endif

static inline int is_length_modifier(char c)
{
    return c == 'h' || c == 'l' || c == 'L' || c == 'j' || c == 'z' || c == 't';
}

static inline int insert_width_limit(char **dst, size_t *dst_left, va_list args)
{
    rsize_t bufsize = (rsize_t)va_arg(args, unsigned int);
    unsigned int limit = (bufsize > 0 && bufsize < 1000) ? (unsigned int)(bufsize - 1) : 255;
    *dst += snprintf(*dst, *dst_left, "%u", limit);
    *dst_left = (*dst_left > strlen(*dst)) ? *dst_left - (size_t)strlen(*dst) : 0;
    return 1;
}

static inline void copy_format_specifier(const char **src, char **dst, size_t *dst_left)
{
    if (**src == 's' || **src == 'c')
    {
        return;
    }
    if (**src)
    {
        *(*dst)++ = *(*src)++;
        (*dst_left)--;
    }
}

static inline int sscanf_s(const char *s, const char *format, ...)
{
    va_list args;
    va_start(args, format);

    char fmt_buf[256] = {0};
    const char *src = format;
    char *dst = fmt_buf;
    size_t dst_left = 255;

    while (*src && dst_left > 1)
    {
        if (*src != '%')
        {
            *dst++ = *src++;
            dst_left--;
            continue;
        }

        *dst++ = '%';
        dst_left--;

        src++;
        if (*src == '%')
        {
            *dst++ = '%';
            dst_left--;
            src++;
            continue;
        }

        if (*src == 's' || *src == 'c')
        {
            insert_width_limit(&dst, &dst_left, args);
        }

        while (is_length_modifier(*src))
        {
            *dst++ = *src++;
            dst_left--;
        }

        copy_format_specifier(&src, &dst, &dst_left);
    }

    int result = vsscanf(s, fmt_buf, args);
    va_end(args);
    return result;
}

#endif /* !_WIN32 */

#endif /* COMPAT_PORT_H */
