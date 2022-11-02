#ifndef BACKEND_H
#define BACKEND_H

typedef struct backend_i
{
    build_result_t (*build)(build_context_t *context, const source_files_t *files, const build_job_t *job);
} backend_i;

#endif /* BACKEND_H */
