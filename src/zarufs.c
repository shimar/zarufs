/* zarufs.c */
#include <linux/module.h>

#include "../include/zarufs.h"
#include "zarufs_utils.h"

static int __init init_zarufs(void) {
  DBGPRINT("[ZARUFS] Hello, World.\n");
  return 0;
}

static void __exit exit_zarufs(void) {
  DBGPRINT("[ZARUFS] GoodBye!.\n");
  return;
}

module_init(init_zarufs);
module_exit(exit_zarufs);

MODULE_LICENSE("GPL");
