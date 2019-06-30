#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <klib.h>

#define DEV

#define Log_write(format, ...)

#ifdef DEV

#define printflog(format, ...) \
  do { \
    printf(format, ## __VA_ARGS__); \
    Log_write(format, ## __VA_ARGS__); \
  } while (0)

#else

#define printflog(format, ...) \
  do { \
    Log_write(format, ## __VA_ARGS__); \
  } while (0)

#endif

#define Log(format, ...) \
    printflog("\33[1;34m[%s,%d,%s] " format "\33[0m\n", \
        __FILE__, __LINE__, __func__, ## __VA_ARGS__)

#define Info(format, ...) \
  printflog("\33[1;44m\33[1;37m \33[0m\33[1;34m(%s,%d,%s)\33[0m " format "\33[0m\n", \
      __FILE__, __LINE__, __func__, ## __VA_ARGS__)

#define Pass(format, ...) \
		printflog("\33[1;42m\33[1;37m \33[0m\33[1;34m(%s,%d,%s)\33[0m " format "\33[0m\n", \
				__FILE__, __LINE__, __func__, ## __VA_ARGS__)

#define Warning(format, ...) \
		printflog("\33[1;43m\33[1;37m \33[0m\33[1;34m(%s,%d,%s)\33[0m " format "\33[0m\n", \
				__FILE__, __LINE__, __func__, ## __VA_ARGS__)

#define Error(format, ...) \
		printflog("\33[1;41m\33[1;37m \33[0m\33[1;34m(%s,%d,%s)\33[0m " format "\33[0m\n", \
				__FILE__, __LINE__, __func__, ## __VA_ARGS__)

#define InfoN(format, ...) \
		printflog("\33[1;44m\33[1;37m \33[0m " format "\33[0m\n", \
				 ## __VA_ARGS__)
#define WarningN(format, ...) \
		printflog("\33[1;43m\33[1;37m \33[0m " format "\33[0m\n", \
				 ## __VA_ARGS__)
#define ErrorN(format, ...) \
		printflog("\33[1;41m\33[1;37m \33[0m " format "\33[0m\n", \
				 ## __VA_ARGS__)
#define PassN(format, ...) \
		printflog("\33[1;42m\33[1;37m \33[0m " format "\33[0m\n", \
				 ## __VA_ARGS__)

#define Assert(cond, format, ...) \
  do { \
    if (!(cond)) { \
      Error(format, ## __VA_ARGS__); \
      assert(cond); \
    } \
  } while (0)

#define Panic(format, ...) \
  Assert(0, format, ## __VA_ARGS__)

#define TODO() Panic("%s", "please implement me")

#endif
