#ifndef __BUFFER_H__
#define __BUFFER_H__

#include <common.h>
#include <debug.h>

void buffer_init(buffer *buf, size_t cap);

void buffer_free(buffer *buf);

int buffer_isempty(buffer *buf);

// return -1 if empty else uint8_t
int buffer_peek(buffer *buf);

// return -1 if empty else uint8_t
int buffer_next(buffer *buf);

// return -1 if failed
int buffer_push(buffer *buf, char c);

#endif