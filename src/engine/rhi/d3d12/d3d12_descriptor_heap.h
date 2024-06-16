#pragma once

typedef struct d3d12_descriptor_t
{
	D3D12_CPU_DESCRIPTOR_HANDLE cpu;
	D3D12_GPU_DESCRIPTOR_HANDLE gpu;
	uint32_t index;
} d3d12_descriptor_t;

typedef struct d3d12_descriptor_range_t
{
	D3D12_CPU_DESCRIPTOR_HANDLE cpu;
	D3D12_GPU_DESCRIPTOR_HANDLE gpu;
	uint32_t count;
	uint32_t stride;
	uint32_t index;
} d3d12_descriptor_range_t;

//
// descriptor arena
//

typedef struct d3d12_descriptor_arena_t
{
	ID3D12DescriptorHeap *heap;
	D3D12_CPU_DESCRIPTOR_HANDLE cpu_base;
	D3D12_GPU_DESCRIPTOR_HANDLE gpu_base;
	uint32_t at;
	uint32_t capacity;
	uint32_t stride;
} d3d12_descriptor_arena_t;

fn void d3d12_descriptor_arena_init(ID3D12Device *device,
									d3d12_descriptor_arena_t *arena, 
									D3D12_DESCRIPTOR_HEAP_TYPE type, 
									UINT capacity, 
									bool shader_visible);
fn void d3d12_descriptor_arena_release(d3d12_descriptor_arena_t *arena);
fn d3d12_descriptor_t d3d12_descriptor_arena_allocate(d3d12_descriptor_arena_t *arena);
fn d3d12_descriptor_range_t d3d12_descriptor_arena_allocate_range(d3d12_descriptor_arena_t *arena, uint32_t count);

//
// descriptor heap
//

// TODO: this is stupid because frame_index for each pending free is massively redundant 
// but it makes the code easier - replace with not stupid eventually
typedef struct d3d12_pending_free_t
{
	uint32_t index;
	uint32_t frame_index;
} d3d12_pending_free_t;

typedef struct d3d12_descriptor_heap_t
{
	string_t debug_name;

	ID3D12DescriptorHeap *heap;

#if DREAM_SLOW
	rhi_buffer_t  *debug_buffer_map;
	rhi_texture_t *debug_texture_map;
#endif

	D3D12_CPU_DESCRIPTOR_HANDLE cpu_base;
	D3D12_GPU_DESCRIPTOR_HANDLE gpu_base;

	d3d12_pending_free_t *pending_free_indices;
	uint32_t              pending_free_head;
	uint32_t              pending_free_tail;

	uint32_t  free_count;
	uint32_t *free_indices;

	uint32_t capacity;
	uint32_t stride;

	mutex_t mutex;
} d3d12_descriptor_heap_t;

fn void d3d12_descriptor_heap_init(ID3D12Device               *device,
								   arena_t                    *rhi_arena,
								   d3d12_descriptor_heap_t    *heap,
								   D3D12_DESCRIPTOR_HEAP_TYPE  type,
								   UINT                        persistent_capacity,
								   UINT                        transient_capacity,
								   bool                        shader_visible,
								   string_t                    debug_name);
fn void d3d12_descriptor_heap_release(d3d12_descriptor_heap_t *heap);
fn void d3d12_descriptor_heap_flush_pending_frees(d3d12_descriptor_heap_t *heap, uint32_t frame_index);

fn d3d12_descriptor_t d3d12_allocate_descriptor_persistent(d3d12_descriptor_heap_t *heap);
fn d3d12_descriptor_t d3d12_allocate_descriptor_persistent_for_buffer (d3d12_descriptor_heap_t *heap, rhi_buffer_t  buffer);
fn d3d12_descriptor_t d3d12_allocate_descriptor_persistent_for_texture(d3d12_descriptor_heap_t *heap, rhi_texture_t texture);
fn void               d3d12_free_descriptor_persistent    (d3d12_descriptor_heap_t *heap, uint32_t index);

// TODO: Implement
// fn d3d12_descriptor_t d3d12_allocate_descriptor_transient(d3d12_descriptor_heap_t *heap);
