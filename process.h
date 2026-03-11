#ifndef COMPAT_PROCESS_H
#define COMPAT_PROCESS_H

#ifdef _WIN32
int _getpid(void);
#else
#include <unistd.h>

static inline int _getpid(void)
{
    return (int)getpid();
}

#endif

#endif
