/*
 *  Symbols Ut Video references that neither the solvers nor this build supply.
 *
 *  The four log entry points are Ut Video's debug log, which writes to a named
 *  pipe on Windows and to a file elsewhere. utv_logl is left out of the build
 *  rather than ported: LogWriter.cpp calls getprogname(), which emscripten does
 *  not have, and a side module has nowhere useful to write anyway. The
 *  signatures below are the ones LogWriter.h and LogUtil.h declare, so the C++
 *  mangling matches what the codec calls; IsLogWriterInitialized() returns
 *  false, so WriteLog is in fact never reached.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* LogWriter.h declares this and IsLogWriterInitialized() compares it to -1;
 * -1 means "no log", which is what a side module wants. */
int fdLogSock = -1;

int InitializeLogWriter(void) { return 0; }
int UninitializeLogWriter(void) { return 0; }
int WriteLog(const char *p) { (void)p; return 0; }

void FormatBinary(char *pszDst, const void *pData, size_t cb, size_t cbLimit)
{
	(void)pData; (void)cb; (void)cbLimit;
	if (pszDst) pszDst[0] = '\0';
}

extern "C" {

/* Ut Video asks for the page size to align its frame buffers. wasm pages are
 * 64 KiB, but this value is only an alignment hint, and emscripten's own heap
 * hands out 64 KiB-aligned blocks well before that matters. */
int getpagesize(void) { return 65536; }

void __assert_fail(const char *expr, const char *file, unsigned int line, const char *func)
{
	fprintf(stderr, "[UTVideoDec] assertion failed: %s at %s:%u in %s\n",
	        expr ? expr : "?", file ? file : "?", line, func ? func : "?");
	abort();
}

}
