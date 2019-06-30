#ifndef __DEBUG_H__
#define __DEBUG_H__

#define Log_write(format, ...)

#define printflog(format, ...) \
  do { \
    printf(format, ## __VA_ARGS__); \
    Log_write(format, ## __VA_ARGS__); \
  } while (0)

#define Log(format, ...) \
    printflog("\33[1;34m[%s,%d,%s] " format "\33[0m\n", \
        __FILE__, __LINE__, __func__, ## __VA_ARGS__)

#define Info(format, ...) \
		printflog("[\33[1;35mInfo\33[0m]\33[1;34m(%s,%d,%s)\33[0m " format "\33[0m\n", \
				__FILE__, __LINE__, __func__, ## __VA_ARGS__)

#define InfoN(format, ...) \
		printflog("[\33[1;35mInfo\33[0m] " format "\33[0m\n", \
				 ## __VA_ARGS__)
#define WarningN(format, ...) \
		printflog("[\33[1;35mInfo\33[0m] " format "\33[0m\n", \
				 ## __VA_ARGS__)
#define ErrorN(format, ...) \
		printflog("[\33[1;35mInfo\33[0m] " format "\33[0m\n", \
				 ## __VA_ARGS__)

#define Warning(format, ...) \
		printflog("[\33[1;33mWarning\33[0m]\33[1;34m(%s,%d,%s)\33[0m " format "\33[0m\n", \
				__FILE__, __LINE__, __func__, ## __VA_ARGS__)

#define Error(format, ...) \
		printflog("[\33[1;31mError\33[0m]\33[1;34m(%s,%d,%s)\33[0m " format "\33[0m\n", \
				__FILE__, __LINE__, __func__, ## __VA_ARGS__)

#define Assert(cond, ...) \
  do { \
    if (!(cond)) { \
      fflush(stdout); \
      fprintf(stderr, "\33[1;31m"); \
      fprintf(stderr, __VA_ARGS__); \
      fprintf(stderr, "\33[0m\n"); \
      assert(cond); \
    } \
  } while (0)

#define panic(format, ...) \
  Assert(0, format, ## __VA_ARGS__)

#define TODO() panic("please implement me")

#endif
