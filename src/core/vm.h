#ifndef VIRTUAL_MEMORY_H
#define VIRTUAL_MEMORY_H

#include "api_types.h"

void *vm_reserve (void *address, size_t size);
bool  vm_commit  (void *address, size_t size);
void  vm_decommit(void *address, size_t size);
void  vm_release (void *address);

#endif /* VIRTUAL_MEMORY_H */
