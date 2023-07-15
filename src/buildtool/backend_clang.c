static compile_error_t clang_compile(build_context_t *context, const compile_params_t *params, const source_files_t *files_, object_collection_t *objects)
{
    (void)context;
    (void)params;
    (void)files_;
    (void)objects;

    fprintf(stderr, "clang backend not yet implemented! shucks!\n");
    return BUILD_ERROR_OTHER_FAILURE;
}

static link_error_t clang_link(build_context_t *context, const link_params_t *params, const object_collection_t *objects)
{
    (void)context;
    (void)params;
    (void)objects;

    fprintf(stderr, "clang backend not yet implemented! shucks!\n");
    return BUILD_ERROR_OTHER_FAILURE;
}

static backend_i backend_clang = {
	.name    = "clang",
    .compile = clang_compile,
    .link    = clang_link,
};
