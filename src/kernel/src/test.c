#include <common.h>
#include <klib.h>
#include <test.h>
#include <debug.h>

void test_alloc()
{
  static char *rs_l10 = "0123456789";

  WarningN("Test PMM on cpu %d", _cpu());

  {
    char *s_l10 = pmm->alloc(11);
    strcpy(s_l10, rs_l10);
    int len = strlen(s_l10);
    assert(len == 10);
    for (int i = 0; i < len; i++)
      assert(rs_l10[i] == s_l10[i]);
    pmm->free(s_l10);
  }
  {
    char *s_l10 = pmm->alloc(11);
    strcpy(s_l10, rs_l10);
    int len1 = strlen(s_l10);
    assert(len1 == 10);
    for (int i = 0; i < len1; i++)
      assert(rs_l10[i] == s_l10[i]);
    char *s_l20 = pmm->alloc(21);
    strcpy(s_l20, rs_l10);
    int len2 = strlen(s_l20);
    assert(len2 == 10);
    for (int i = 0; i < len2; i++)
      assert(s_l20[i] == s_l10[i]);
    pmm->free(s_l20);
    pmm->free(s_l10);
  }

  PassN("Test PMM PASS on cpu %d", _cpu());
}