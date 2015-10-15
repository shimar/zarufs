/* zarufs.c */
#include <linux/module.h>

#include "../include/zarufs.h"

static int __init init_zarufs(void) {
  return 0;
}

static void __exit exit_zarufs(void) {
  return;
}

module_init(init_zarufs);
module_exit(exit_zarufs);

MODULE_LICENSE("GPL");
