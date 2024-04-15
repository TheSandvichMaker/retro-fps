#include "assets.h"

void build_maps(string_t source_maps, string_t dest)
{
	arena_t *temp = m_get_temp(NULL, 0);

    printf("\n");
    printf("=================================================\n");
    printf("                  BUILD MAPS                     \n");
    printf("=================================================\n");
    printf("\n");
    for (fs_entry_t *entry = fs_scan_directory(temp, source_maps, FS_SCAN_RECURSIVE);
         entry;
         entry = fs_entry_next(entry))
    {
        string_t name = string_strip_extension(entry->name);
        if (string_match(string_extension(entry->name), strlit(".map")))
        {
            string_t run = string_format(temp, "tools/ericwa/bin/qbsp.exe %.*s/%.*s", strexpand(source_maps), strexpand(entry->name));
            fprintf(stderr, "Running command: %.*s\n", strexpand(run));

            int exit_code;
            if (os_execute(run, &exit_code))
            {
                if (!fs_move(string_format(temp, "%.*s/%.*s.bsp", strexpand(source_maps), strexpand(name)),
                             string_format(temp, "%.*s/%.*s.bsp", strexpand(dest), strexpand(name))))
                {
                    fprintf(stderr, "Failed to move .bsp to asset folder\n");
                }

                if (!fs_move(string_format(temp, "%.*s/%.*s.pts", strexpand(source_maps), strexpand(name)),
                             string_format(temp, "%.*s/%.*s.pts", strexpand(dest), strexpand(name))))
                {
                    fprintf(stderr, "Failed to move .pts to assert folder\n");
                }

                fs_create_directory(string_format(temp, "%.*s/generated"));
                if (!fs_move(string_format(temp, "%.*s/%.*s.log", strexpand(source_maps), strexpand(name)),
                             string_format(temp, "%.*s/generated/%.*s.log", strexpand(source_maps), strexpand(name))))
                {
                    fprintf(stderr, "Failed to move .log to generated folder\n");
                }
            }
            else
            {
                fprintf(stderr, "Failed to run qbsp.exe on map '%.*s'\n", strexpand(entry->name));
            }
        }
    }
}

void copy_assets(void)
{
	fs_copy_directory(strlit("src/assets/*"), strlit("run/gamedata"));
}
