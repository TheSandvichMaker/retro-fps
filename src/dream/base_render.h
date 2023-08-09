#ifndef DREAM_BASE_RENDER_H
#define DREAM_BASE_RENDER_H

typedef struct r_view_t
{
	m4x4_t view;          // 64
	m4x4_t proj;          // 128
	rect2i16_t clip_rect; // 136
} r_view_t;

typedef uint16_t r_cmd_kind_t;
typedef enum r_cmd_kind_enum_t
{
	R_CMD_IMMEDIATE,
	R_CMD_PRETTY_RECT,
	R_CMD_BUILTIN_COUNT,

	R_CMD_USER = (1 << 15),
} r_cmd_kind_enum_t;

typedef struct r_cmd_header_t
{
	r_cmd_kind_t kind;
	uint16_t     size;
	uint32_t     view_offset;
	uint32_t     user_flags;
} r_cmd_header_t;

typedef uint8_t r_topology_t;
typedef enum r_topology_enum_t
{
	R_TOPOLOGY_LINELIST,
	R_TOPOLOGY_TRIANGELIST,
	R_TOPOLOGY_COUNT,
} r_topology_enum_t;

typedef uint8_t r_cull_mode_t;
typedef enum r_cull_mode_enum_t
{
	R_CULL_NONE,
	R_CULL_BACK,
	R_CULL_FRONT,
	R_CULL_COUNT,
} r_cull_mode_enum_t;

typedef uint8_t r_blend_mode_t;
typedef enum r_blend_mode_enum_t
{
	R_BLEND_NONE,
	R_BLEND_PREMUL_ALPHA,
	R_BLEND_ADDITIVE,
	R_BLEND_BUILTIN_COUNT,

	R_BLEND_USER = (1 << 7),
} r_blend_mode_enum_t;

typedef uint16_t r_immediate_shader_t;
typedef enum r_immediate_shader_enum_t
{
	R_IMMEDIATE_SHADER_FLAT,
	R_IMMEDIATE_SHADER_DEBUG_LIGHTING,
	R_IMMEDIATE_SHADER_TEXT,
	R_IMMEDIATE_SHADER_BUILTIN_COUNT,

	R_IMMEDIATE_SHADER_USER = (1 << 15),
} r_immediate_shader_enum_t;

typedef struct r_cmd_immediate_t
{
	resource_handle_t    texture;    // 8
	r_immediate_shader_t shader;     // 10
	r_cull_mode_t        cull_mode;  // 11
	r_blend_mode_t       blend_mode; // 12
	r_topology_t         topology;   // 13

	PAD(3);                          // 16

	uint32_t index_count;            // 20
	uint32_t index_offset;           // 24
	uint32_t vertex_count;           // 28
	uint32_t vertex_offset;          // 32
} r_cmd_immediate_t;

typedef struct r_cmd_pretty_rect_t
{
	rect2i16_t rect;          // 8
	uint32_t   roundedness;   // 12
	uint32_t   color;         // 16
	uint8_t    shadow_amount; // 17
	uint8_t    shadow_radius; // 18
	uint8_t    inner_radius;  // 19
	PAD(1);
} r_cmd_pretty_rect_t;

typedef struct r_command_list_t
{
	uint8_t *head;
	uint8_t *tail;
	uint8_t *head_at;
	uint8_t *tail_at;
} r_command_list_t;

uint8_t *at = start;

while (at < end)
{
	r_command_header_t *header = (r_command_header_t *)at;

	switch (header->kind)
	{
		case R_COMMAND_SOMETHING:
		{
			r_command_something_t *command = (r_command_something_t *)(header + 1);
			do_something(command);
		} break;

		case R_COMMAND_SOMETHING_ELSE:
		{
			r_command_something_else_t *command = (r_command_something_else_t *)(header + 1);
			do_something_else(command);
		} break;
	}

	at += header->size;
}

#endif
