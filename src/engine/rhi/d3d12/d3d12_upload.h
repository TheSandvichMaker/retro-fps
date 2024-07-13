#pragma once

enum { RhiMaxUploadSubmissions = 32 };                // must be pow2
enum { RhiUploadRingBufferSize = 64u * 1024 * 1024 }; // must be pow2

typedef struct d3d12_upload_submission_t
{
	ID3D12CommandAllocator    *allocator;
	ID3D12GraphicsCommandList *command_list;
	uint64_t                   fence_value;
	uint32_t                   offset;
	uint32_t                   size;
	size_t                     head;
} d3d12_upload_submission_t;

typedef struct d3d12_upload_ring_buffer_t
{
	mutex_t                   mutex;
	ID3D12CommandQueue       *queue;
	ID3D12Fence              *fence;
	uint64_t                  fence_value;
	d3d12_upload_submission_t submissions[RhiMaxUploadSubmissions];
	uint32_t                  submission_tail;
	uint32_t                  submission_head;
	ID3D12Resource           *buffer;
	uint8_t                  *buffer_base;
	size_t                    capacity;
	size_t                    mask;
	size_t                    tail; 
	size_t                    head; 
} d3d12_upload_ring_buffer_t;

typedef struct d3d12_upload_context_t
{
	ID3D12Resource            *buffer;
	ID3D12GraphicsCommandList *command_list;
	size_t                     offset;
	void                      *pointer;
	d3d12_upload_submission_t *submission;
} d3d12_upload_context_t;

fn bool                   d3d12_upload_ring_buffer_init  (ID3D12Device *device, d3d12_upload_ring_buffer_t *ring_buffer);
fn d3d12_upload_context_t d3d12_upload_begin             (size_t size, size_t align);
fn uint64_t               d3d12_upload_end               (const d3d12_upload_context_t *ctx);
fn void                   d3d12_flush_ring_buffer_uploads(void);
fn void                   d3d12_retire_ring_buffer_entries(void);