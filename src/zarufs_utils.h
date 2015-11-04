#ifndef _ZARUFS_UTILS_H_
#define _ZARUFS_UTILS_H_

#define ZARUFS_DEBUG

#ifdef ZARUFS_DEBUG
#define DBGPRINT(msg, args...) do { \
    printk(KERN_INFO msg, ##args); \
  } while(0)
#else
#define DBGPRINT(msg, args...) do {} while (0)
#endif

#define ZARUFS_ERROR(msg, args...) do {         \
    printk(KERN_ERR msg, ##args);              \
} while(0)

#endif //_ZARUFS_UTILS_H_
