// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

#define D3D12_CHECK_HR(in_hr, on_fail)                            \
	{                                                             \
		HRESULT __hr = in_hr;                                     \
		if (FAILED(__hr))                                         \
		{                                                         \
			win32_hresult_error_box(__hr, "D3D12 Call Failed!");  \
			on_fail;                                              \
		}                                                         \
	}

#define D3D12_LOG_FAILURE(in_hr, in_context)                                    \
	{                                                                           \
		HRESULT __hr = in_hr;                                                   \
		if (FAILED(__hr))                                                       \
		{                                                                       \
			m_scoped_temp                                                       \
			{                                                                   \
				string_t hr_formatted = win32_format_error(temp, __hr);         \
				log(RHI_D3D12, Error, in_context ": %.*s", Sx(hr_formatted));   \
			}                                                                   \
		}                                                                       \
	}

#define COM_SAFE_RELEASE(obj) com_safe_release_(&((IUnknown *)(obj)))
fn_local void com_safe_release_(IUnknown **obj_ptr)
{
	if (*obj_ptr)
	{
		IUnknown_Release(*obj_ptr);
		*obj_ptr = NULL;
	}
}

fn ID3D12Resource *d3d12_create_upload_buffer(ID3D12Device *device, uint32_t size, void *initial_data, string_t debug_name);
fn void     d3d12_set_debug_name(ID3D12Object *object, string_t name);
fn string_t d3d12_get_debug_name(arena_t *arena, ID3D12Object *object);
