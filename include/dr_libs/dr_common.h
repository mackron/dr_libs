#ifndef dr_common_h
#define dr_common_h

#include <stddef.h> /* For size_t. */

typedef struct {
  void* pUserData;
  void* (*onMalloc)(size_t sz, void* pUserData);
  void* (*onRealloc)(void* p, size_t sz, void* pUserData);
  void (*onFree)(void* p, void* pUserData);
} drlibs_allocation_callbacks;

#endif
