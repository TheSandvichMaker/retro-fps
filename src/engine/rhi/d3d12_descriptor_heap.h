#pragma once

typedef struct d3d12_descriptor_t
{
	D3D12_CPU_DESCRIPTOR_HANDLE cpu;
	D3D12_GPU_DESCRIPTOR_HANDLE gpu;
	uint32_t stride;
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
