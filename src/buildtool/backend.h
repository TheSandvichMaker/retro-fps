#ifndef BACKEND_H
#define BACKEND_H

typedef struct backend_i
{
	char *name;
    compile_error_t (*compile)(build_context_t *context, const compile_params_t *params, const source_files_t *files, object_collection_t *objects);
    link_error_t (*link)(build_context_t *context, const link_params_t *params, const object_collection_t *objects);
} backend_i;

#endif /* BACKEND_H */
