/* Exact ports of njplus.c:943-960. */
#include <stdint.h>

void core_np_set_memory(unsigned char* memory, unsigned int size, char value)
{
    while (size-- != 0) {
        *memory++ = (unsigned char)value;
    }
}

void core_np_set_memory_l(unsigned int* memory, unsigned int size, int value)
{
    while (size-- != 0) {
        *memory++ = (unsigned int)value;
    }
}
