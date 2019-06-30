#ifndef __PATH_H__
#define __PATH_H__

char *path_join(const char *base, const char *child);

// return new alloc string, f("/abc/def/")=f("/abc/def")="/abc/"
char *path_get_directory(const char *path);

// return new alloc string, f("/abc/def/")=f("/abc/def")="def"
char *path_get_name(const char *path);

// return new alloc string, or NULL when end, ptr is NULL if no next
char *path_next_name(const char *path, const char** ptr);

#endif