#ifndef RECVX_W32_STDLIB_H
#define RECVX_W32_STDLIB_H
#include <stddef.h>
int abs(int); long labs(long); int rand(void); void srand(unsigned);
void* malloc(size_t); void free(void*); void exit(int);
int atoi(const char*);
#endif
