#pragma once

typedef struct d3d12_buffer_arena_t 
{
	alignas(CACHE_LINE_SIZE) atomic uint32_t at;
	alignas(CACHE_LINE_SIZE)

	ID3D12Resource           *buffer;
	D3D12_GPU_VIRTUAL_ADDRESS gpu_base;
	char                     *cpu_base;
	uint32_t                  capacity;
} d3d12_buffer_arena_t;

typedef struct d3d12_buffer_allocation_t
{
	ID3D12Resource           *buffer;
	size_t                    offset;
	void                     *cpu;
	D3D12_GPU_VIRTUAL_ADDRESS gpu;
} d3d12_buffer_allocation_t;

fn void                      d3d12_arena_init   (ID3D12Device *device, d3d12_buffer_arena_t *arena, uint32_t capacity, string_t debug_name);
fn void                      d3d12_arena_release(d3d12_buffer_arena_t *arena);
fn d3d12_buffer_allocation_t d3d12_arena_alloc  (d3d12_buffer_arena_t *arena, uint32_t size, uint32_t alignment);
fn void                      d3d12_arena_reset  (d3d12_buffer_arena_t *arena);
