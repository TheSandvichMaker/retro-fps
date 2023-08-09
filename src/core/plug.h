#ifndef PLUG_H
#define PLUG_H

#include "core/api_types.h"

typedef struct plug_io_t
{
	bool reload;
} plug_io_t;

typedef void *(*plug_load_t)(plug_io_t *io);
typedef void (*plug_unload_t)(plug_io_t *io);

#define DREAM_PLUG_LOAD(name) void *plug_load__##name(plug_io_t *io)
#define DREAM_PLUG_UNLOAD(name) void plug_unload__##name(plug_io_t *io)

#define DREAM_DECLARE_PLUG(name)           \
	DREAM_DLLEXPORT DREAM_PLUG_LOAD(name); \
	DREAM_DLLEXPORT DREAM_PLUG_UNLOAD(name);

#endif /* PLUG_H */
