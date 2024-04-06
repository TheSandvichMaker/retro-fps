#include "core.h"

uintptr_t align_forward(uintptr_t address, uintptr_t align)
{
    uintptr_t result = (address + (align-1)) & (-(intptr_t)align);
    return result;
}

uintptr_t align_backward(uintptr_t address, uintptr_t align)
{
    uintptr_t result = address & (-(intptr_t)align);
    return result;
}

void *align_address(void *address, uintptr_t align)
{
    void *result = (void *)align_forward((uintptr_t)address, align);
    return result;
}

void set_memory(void *memory, size_t size, char value)
{
    unsigned char *data = (unsigned char *)memory;
#ifdef _MSC_VER
    __stosb(data, value, size);
#else
    while (size--)
    {
        *data++ = value;
    }
#endif
}

void copy_memory(void *dest_init, const void *source_init, size_t size)
{
    if (source_init == dest_init) return;

    const unsigned char *source = (const unsigned char *)source_init;
    unsigned char *dest = (unsigned char *)dest_init;

    ASSERT(dest + size <= source || dest >= source + size);

// #ifdef _MSC_VER
//     __movsb(dest, source, size);
// #elif defined(__i386__) || defined(__x86_64__)
//     __asm__ __volatile__("rep movsb" : "+c"(size), "+S"(source), "+D"(dest): : "memory");
// #else
    while (size--)
    {
        *dest++ = *source++;
    }
// #endif
}
