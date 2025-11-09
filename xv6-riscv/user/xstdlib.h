#ifndef _XSTDLIB_H_
#define _XSTDLIB_H_

#include "kernel/types.h"

void* xcalloc(uint num, uint size);

void* xbsearch(const void* key, const void* base, uint num, uint size, int (*compar)(const void*, const void*));
void xqsort(void* base, uint num, uint size, int (*compar)(const void*, const void*));

int xatoi(const char* str);
double xatof(const char* str);

#endif 


