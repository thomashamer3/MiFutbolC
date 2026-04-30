/* sqlite3_exports.c
 * Fallback definitions for sqlite3 version symbols to satisfy the
 * linker in environments where the amalgamation does not export them
 * as expected. This file is only used as a safety net.
 */

#include "sqlite3.h"

#ifndef SQLITE3_VERSION_FALLBACK
#define SQLITE3_VERSION_FALLBACK 1

/* Provide the version/sourceid symbols when missing. The macros
 * SQLITE_API and SQLITE_EXTERN resolve to appropriate linkage
 * specifiers in the amalgamation; defining them here ensures the
 * linker finds the symbols if the amalgamation didn't export them.
 */
SQLITE_API const char sqlite3_version[] = SQLITE_VERSION;

#endif /* SQLITE3_VERSION_FALLBACK */
