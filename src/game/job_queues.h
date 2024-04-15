#ifndef GAME_JOB_QUEUES_H
#define GAME_JOB_QUEUES_H

#include "core/api_types.h"
#include "core/thread.h"

DREAM_LOCAL void init_game_job_queues(void);
DREAM_LOCAL job_queue_t high_priority_job_queue;
DREAM_LOCAL job_queue_t low_priority_job_queue;

#endif
