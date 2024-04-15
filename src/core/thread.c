#include "core/thread.h"
#include "core/atomics.h"
#include "core/assert.h"

// Wait group implementation referenced (more accurately: copied) from Odin's standard library

void wait_group_add(wait_group_t *group, int64_t delta)
{
	if (delta == 0)
	{
		return;
	}

	mutex_lock(&group->mutex);

	atomic_add_i64(&group->counter, delta);

	if (group->counter < 0)
	{
		FATAL_ERROR("Wait group counter went negative!");
	}

	if (group->counter == 0)
	{
		cond_wake_all(&group->cond);

		if (group->counter != 0)
		{
			FATAL_ERROR("Wait group misuse: wait_group_add called concurrently with wait_group_wait");
		}
	}

	mutex_unlock(&group->mutex);
}

void wait_group_done(wait_group_t *group)
{
	wait_group_add(group, -1);
}

void wait_group_wait(wait_group_t *group)
{
	mutex_lock(&group->mutex);

	if (group->counter != 0)
	{
		cond_sleep(&group->cond, &group->mutex);

		if (group->counter != 0)
		{
			FATAL_ERROR("Wait group misuse: wait_group_add called concurrently with wait_group_wait");
		}
	}
}
