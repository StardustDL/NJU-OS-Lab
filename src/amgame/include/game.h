#include <am.h>
#include <amdev.h>
#include <klib.h>

// #define DEBUG

#ifdef DEBUG
#include </home/liang/labs/os-workbench/common/debug.h>

static void show_key(uint32_t keycode)
{
#define KEYNAME(key) \
  [_KEY_##key] = #key,
  static const char *key_names[] = {
      _KEYS(KEYNAME)};
  if (keycode != _KEY_NONE)
  {
    printf("Key pressed: %s\n", key_names[keycode]);
  }
}
#endif

#define SIDE 4

typedef uint8_t bool;
#define true 1
#define false 0

static void swap(uint32_t *a, uint32_t *b)
{
  uint32_t t = *a;
  *a = *b;
  *b = t;
}

static int read_press_key()
{
  uint32_t keycode = 0;
  do
  {
    keycode = read_key();
  } while ((keycode & 0x8000) == 0);
  return keycode & ~(0x8000);
}

static void puts(const char *str)
{
  printf("%s\n", str);
}