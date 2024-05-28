typedef struct pack_context_t pack_context_t;

typedef struct pack_job_t
{
	pack_asset_kind_t kind;
	uint64_t header_index;
	string_t path;
} pack_job_t;

typedef struct pack_job_group_t
{
	pack_context_t *context;

	pack_job_t *jobs;
	size_t      jobs_count;
} pack_job_group_t;

typedef struct pack_context_t
{
	arena_t arena;

	stretchy_buffer(pack_job_t) jobs;

	mutex_t pack_file_mutex;
	pack_file_t *pack_file;

	bool dispatched;
} pack_context_t;

fn_local pack_context_t *create_pack_context(void)
{
	pack_context_t *context = m_bootstrap(pack_context_t, arena);
	context->jobs = sb_init(&context->arena, 128, pack_job_t);

	context->pack_file         = m_bootstrap(pack_file_t, arena);
	context->pack_file->header = m_alloc_struct(&context->pack_file->arena, pack_header_t);

	pack_header_t *header = context->pack_file->header;
	header->magic   = PACK_TAG('d', 'f', 'p', 'k');
	header->version = PackVer_MAX - 1;

	return context;
}

fn_local pack_job_t *add_pack_job(pack_context_t *context, pack_asset_kind_t kind, string_t path)
{
	ASSERT(!context->dispatched);

	pack_header_t *header = context->pack_file->header;
	header->assets_count += 1;

	uint64_t header_index = 0;

	switch (kind)
	{
		case PackAsset_texture: { header_index = header->textures_count; header->textures_count += 1; } break;
		case PackAsset_sound:   { header_index = header->sounds_count;   header->sounds_count   += 1; } break;
	}

	pack_job_t *job = sb_add(context->jobs);
	job->kind         = kind;
	job->header_index = header_index;
	job->path         = string_copy(&context->arena, path);

	return job;
}

fn_local void process_pack_job(job_context_t *job_context, void *userdata);

fn_local uint64_t get_pointer_offset_u64(void *base, void *data)
{
	if (data == NULL)
	{
		return 0;
	}

	ASSERT((uintptr_t)base <= (uintptr_t)data);

	uintptr_t result = (uintptr_t)data - (uintptr_t)base;
	return (uint64_t)result;
}

fn_local void dispatch_pack_jobs(pack_context_t *context)
{
	ASSERT(!context->dispatched);

	pack_file_t   *file   = context->pack_file;
	pack_header_t *header = context->pack_file->header;

	pack_texture_t *textures = m_alloc_array(&file->arena, header->textures_count, pack_texture_t);
	pack_sound_t   *sounds   = m_alloc_array(&file->arena, header->sounds_count,   pack_sound_t);

	header->textures_offset = get_pointer_offset_u64(header, textures);
	header->sounds_offset   = get_pointer_offset_u64(header, sounds);

	uint32_t jobs_count        = sb_count(context->jobs);
	uint32_t dispatch_count    = 16;
	uint32_t jobs_per_dispatch = (jobs_count + dispatch_count - 1) / dispatch_count;

	size_t      jobs_left = jobs_count;
	pack_job_t *jobs      = context->jobs;
	for (size_t i = 0; i < dispatch_count; i++)
	{
		pack_job_group_t *group = m_alloc_struct(&context->arena, pack_job_group_t);
		group->context    = context;
		group->jobs       = jobs;
		group->jobs_count = MIN(jobs_left, jobs_per_dispatch);

		add_job_to_queue(low_priority_job_queue, process_pack_job, group);

		jobs_left -= group->jobs_count;
		jobs      += group->jobs_count;
	}

	context->dispatched = true;
}

fn_local void wait_on_pack_jobs(pack_context_t *context)
{
	ASSERT(context->dispatched);

	if (sb_count(context->jobs) > 0)
	{
		wait_on_queue(low_priority_job_queue);
	}
}

void pack_assets(string_t source_directory)
{
	pack_context_t *context = create_pack_context();

	m_scoped_temp
	{
		for (fs_entry_t *entry = fs_scan_directory(temp, source_directory, FsScanDirectory_recursive);
			 entry;
			 entry = fs_entry_next(entry))
		{
			if (entry->kind == FsEntryKind_directory)
			{
				continue;
			}

			string_t ext = string_extension(entry->name);

			if (string_match_nocase(ext, S(".png")) ||
				string_match_nocase(ext, S(".jpg")) ||
				string_match_nocase(ext, S(".tga")))
			{
				add_pack_job(context, PackAsset_texture, entry->path);
			}
			else if (string_match_nocase(ext, S(".wav")))
			{
				add_pack_job(context, PackAsset_sound, entry->path);
			}
		}
	}

	dispatch_pack_jobs(context);
	wait_on_pack_jobs (context);

	pack_file_t *pack_file = context->pack_file;

	uint64_t header_offset = get_pointer_offset_u64(pack_file->arena.buffer, pack_file->header);

	string_t pack_file_data = {
		.data  = (char *)pack_file->header,
		.count = m_size_used(&pack_file->arena) - header_offset,
	};

	fs_write_entire_file(S("pack_test.pak"), pack_file_data);
}

void process_pack_job(job_context_t *job_context, void *userdata)
{
	(void)job_context;

	pack_job_group_t *group   = userdata;
	pack_context_t   *context = group->context;

	for (size_t job_index = 0; job_index < group->jobs_count; job_index++)
	{
		arena_t *temp = m_get_temp_scope_begin(NULL, 0);

		pack_job_t *job = &group->jobs[job_index];

		mutex_t       *pack_mutex = &context->pack_file_mutex;
		arena_t       *pack_arena = &context->pack_file->arena;
		pack_file_t   *pack_file  =  context->pack_file;
		pack_header_t *header     =  context->pack_file->header;
		
		switch (job->kind)
		{
			case PackAsset_texture:
			{
				pack_texture_t *textures = (pack_texture_t *)((uint8_t *)header + header->textures_offset);

				// TODO: There is a totally unnecessary copy into temp with this call, 
				// just call image loading libraries in this code directly!
				image_t image = load_image_from_disk(temp, job->path, 4);
				ASSERT_MSG(image.pixels, "Failed to read image %.*s!", Sx(job->path));

				size_t image_footprint = image.info.h*image.pitch;

				mutex_lock(pack_mutex);
				void *image_data = m_alloc_nozero(&pack_file->arena, image_footprint, 16);
				mutex_unlock(pack_mutex);

				copy_memory(image_data, image.pixels, image_footprint);

				pack_texture_t *texture = &textures[job->header_index];
				texture->w            = (uint16_t)image.info.w;
				texture->h            = (uint16_t)image.info.h;
				texture->d            = 1;
				texture->dimensions   = 2;
				texture->pixel_format = image.info.format;
				texture->data_offset  = get_pointer_offset_u64(header, image_data);
			} break;

			case PackAsset_sound:
			{
				pack_sound_t *sounds = (pack_sound_t *)((uint8_t *)header + header->sounds_offset);

				// TODO: There is a totally unnecessary copy into temp with this call, 
				// just call audio loading code in this code directly!
				waveform_t waveform = load_waveform_from_disk(temp, job->path);
				ASSERT_MSG(waveform.frames, "Failed to read waveform %.*s!", Sx(job->path));

				ASSERT_MSG(waveform.sample_rate == DREAM_MIX_SAMPLE_RATE, 
						   "Sound %.*s has the wrong sample rate. Expected %u, but it's %u!", 
						   Sx(job->path), DREAM_MIX_SAMPLE_RATE, waveform.sample_rate);

				uint32_t sample_count = (uint32_t)(waveform.channel_count*waveform.frame_count);
				size_t sound_footprint = sample_count*sizeof(uint16_t);

				mutex_lock(pack_mutex);
				void *sound_data = m_alloc_nozero(pack_arena, sound_footprint, 16);
				mutex_unlock(pack_mutex);

				copy_memory(sound_data, waveform.frames, sound_footprint);

				pack_sound_t *sound = &sounds[job->header_index];
				sound->sample_count   = sample_count;
				sound->channel_count  = waveform.channel_count;
				sound->samples_offset = get_pointer_offset_u64(header, sound_data);
			} break;
		}

		log(AssetPacker, Spam, "Processed file %.*s", Sx(job->path));

		m_scope_end(temp);
	}
}
