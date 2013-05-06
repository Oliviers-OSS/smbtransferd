#if HAVE_CONFIG_H
#include "config.h"
#endif
#undef malloc

#include <sys/types.h>

void *malloc ();

/* Allocate an N-byte block of memory from the heap.
 * If N is zero, allocate a 1-byte block. 
 * zero length bug in some implementations of malloc like RHEL5
 */

void * rpl_malloc (size_t n) {
  if (0 == n) {
      n = 1;
  }
  return malloc (n);
}

