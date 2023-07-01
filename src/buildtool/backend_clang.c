static build_result_t clang_build(build_context_t *context, const source_files_t *files, const build_job_t *job)
{
    (void)context;
    (void)files;
    (void)job;

    fprintf(stderr, "clang backend not yet implemented! shucks!\n");
    return BUILD_OTHER_FAILURE;
}

static backend_i backend_clang = {
	.name  = "clang",
    .build = clang_build,
};
