#ifndef __IO_H__
#define __IO_H__

#include <common.h>

filestream* fsOpen(const char* path, int flags);

void fsClose(filestream* stream);

int fsGetChar(filestream* stream);

char* fsGetLine(filestream* stream, char* buf, int size);

void fsPutChar(filestream* stream, char c);

void fsPutLine(filestream* stream, const char* s);

int fsPrintf(filestream *stream, const char *fmt, ...);

void fsFlush(filestream* stream);

#endif