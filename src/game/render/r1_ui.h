#pragma once

typedef struct r1_ui_rect_t
{
	v2_t origin;
	v2_t radius;
} r1_ui_rect_t;

typedef enum r1_ui_command_kind_t
{
	R1UiCommand_none,
	R1UiCommand_box,
	R1UiCommand_image,
	R1UiCommand_circle,
	R1UiCommand_COUNT,
} r1_ui_command_kind_t;

typedef struct r1_ui_clip_rect_t
{
	r1_ui_rect_t rect;
	v4_t         roundedness;
} r1_ui_clip_rect_t;

typedef struct r1_ui_colors_t
{
	v4_t colors[4];
} r1_ui_colors_t;

typedef struct r1_ui_command_box_t
{
	r1_ui_rect_t   rect;             // + 4 = 4
	v4_t           roundedness;      // + 4 = 8
	r1_ui_colors_t colors;           // + 4 = 12
	float          shadow_radius;    // + 1 = 13
	float          shadow_amount;    // + 1 = 14
	float          inner_radius;     // + 1 = 15
} r1_ui_command_box_t;

typedef struct r1_ui_command_image_t
{
	r1_ui_rect_t      rect;
	r1_ui_rect_t      uvs;
	rhi_texture_srv_t texture;
} r1_ui_command_image_t;

typedef struct r1_ui_command_t
{
	uint8_t  kind;
	uint8_t  primitive_group;
	uint16_t clip_rect;

	union
	{
		r1_ui_command_box_t   box;
		r1_ui_command_image_t image;
	};
} r1_ui_command_t;

#define R1UiCommandBufferSize (MB(2))
#define R1UiClipRectCapacity  (2048)

typedef struct r1_ui_command_list_t
{
	uint32_t           capacity;
	uint32_t           command_count;
	r1_ui_command_t   *commands_base;
	r1_ui_command_t   *commands_at;
	uint32_t          *keys_at;

	uint16_t           clip_rect_capacity;
	uint16_t           clip_rect_count;
	r1_ui_clip_rect_t *clip_rects;
} r1_ui_command_list_t;

typedef struct r1_ui_render_state_t
{
	rhi_pso_t pso;

	rhi_buffer_t command_buffer;
	rhi_buffer_t clip_rect_buffer;
} r1_ui_render_state_t;

fn_local uint32_t r1_make_ui_sort_key(uint8_t layer, uint8_t sub_layer, uint16_t index)
{
	uint32_t result = index;
	result |= sub_layer << 16;
	result |= layer     << 24;
	return result;
}

fn r1_ui_rect_t make_r1_rect(rect2_t rect);

fn uint16_t r1_push_ui_clip_rect(r1_ui_command_list_t *list, rect2_t rect, v4_t roundedness);

fn r1_ui_command_t *r1_push_ui_command(r1_ui_command_list_t *list, r1_ui_command_kind_t kind, uint32_t sort_key);

fn void r1_push_ui_box(r1_ui_command_list_t *list, uint8_t group, uint16_t clip_rect, rect2_t rect, v4_t roundedness, 
					   r1_ui_colors_t colors, float shadow_radius, float shadow_amount, float inner_radius);

fn void r1_push_ui_image(r1_ui_command_list_t *list, uint8_t group, uint16_t clip_rect, rect2_t rect, rect2_t uvs, 
						 rhi_texture_srv_t texture);

fn void r1_create_ui_render_state(r1_ui_render_state_t *state);
fn void r1_render_ui_new(r1_ui_render_state_t *state, rhi_command_list_t *rhi_list, r1_ui_command_list_t *ui_list, rhi_texture_t rt);
