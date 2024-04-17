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
