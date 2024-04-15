// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

job_queue_t high_priority_job_queue;
job_queue_t low_priority_job_queue;

void init_game_job_queues(void)
{
	size_t core_count = query_processor_count();

	// FIXME: dumb code for idiots all of this sucks dont worry about it
	// FIXME: dumb code for idiots all of this sucks dont worry about it
	// FIXME: dumb code for idiots all of this sucks dont worry about it
	// FIXME: dumb code for idiots all of this sucks dont worry about it
	high_priority_job_queue = create_job_queue(core_count - 2, 1024);
	low_priority_job_queue  = create_job_queue(2, 1024);
}
