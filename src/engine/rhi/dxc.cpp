#define WIN32_LEAN_AND_MEAN 
#include <windows.h>
#include <d3dcommon.h>
#include <dxcapi.h>

#include <stdint.h>

#include "dxc.h"

enum dxc_init_state_t
{
	DxcInitState_None,
	DxcInitState_Success,
	DxcInitState_Failure,
};

static dxc_init_state_t dxc_init_state;

static IDxcCompiler3      *dxc_compiler;
static IDxcUtils          *dxc_utils;
static IDxcIncludeHandler *dxc_include_handler;

static bool dxc_initialized()
{
	if (dxc_init_state == DxcInitState_None)
	{
		dxc_init_state = DxcInitState_Failure;

		HMODULE dxc_dll = LoadLibraryA("dxcompiler.dll");
		if (dxc_dll == NULL) goto fail;

		DxcCreateInstanceProc dxc_create_instance = (DxcCreateInstanceProc)GetProcAddress(dxc_dll, "DxcCreateInstance");
		if (!dxc_create_instance) goto fail;

		HRESULT hr;
		hr = dxc_create_instance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxc_compiler));
		if (FAILED(hr)) goto fail;

		hr = dxc_create_instance(CLSID_DxcUtils, IID_PPV_ARGS(&dxc_utils));
		if (FAILED(hr)) goto fail;

		hr = dxc_utils->CreateDefaultIncludeHandler(&dxc_include_handler);
		if (FAILED(hr)) goto fail;

		dxc_init_state = DxcInitState_Success;
fail:;
		 // I'm too lazy to release stuff on failure, who cares... if we failed here we're fucked anyway
	}

	return dxc_init_state == DxcInitState_Success;
}

int dxc_compile(const char *source, uint32_t source_count, const wchar_t **arguments, uint32_t arguments_count, ID3D10Blob **result_blob, ID3D10Blob **error_blob)
{
	HRESULT hr = E_FAIL;

	if (!dxc_initialized())
	{
		return hr;
	}

	DxcBuffer source_buffer;
	source_buffer.Ptr      = source;
	source_buffer.Size     = source_count;
	source_buffer.Encoding = 0;

	IDxcResult *compile_result = nullptr;
	hr = dxc_compiler->Compile(&source_buffer, arguments, arguments_count, dxc_include_handler, IID_PPV_ARGS(&compile_result));

	if (SUCCEEDED(hr))
	{
		IDxcBlobUtf8 *errors = nullptr;
		hr = compile_result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);

		if (SUCCEEDED(hr))
		{
			if (errors && error_blob)
			{
				*error_blob = (ID3D10Blob *)errors;
			}

			if (compile_result->HasOutput(DXC_OUT_OBJECT))
			{
				IDxcBlob *blob = nullptr;
				hr = compile_result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&blob), nullptr);

				if (SUCCEEDED(hr))
				{
					*result_blob = (ID3D10Blob *)blob;
				}
			}
		}

		compile_result->Release();
	}

	return hr;
}
