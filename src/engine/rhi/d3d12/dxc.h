#pragma once

// C-api DXC wrapper, because microsoft bad.

#if __cplusplus
#define DXC_WRAPPER_API extern "C"
#else
#define DXC_WRAPPER_API extern
#endif

typedef struct ID3D10Blob ID3D10Blob;

// returns HRESULT but I don't want to typedef it here
DXC_WRAPPER_API int dxc_compile(const char *source, uint32_t source_count, const wchar_t **arguments, uint32_t argument_count, ID3D10Blob **result_blob, ID3D10Blob **error_blob);

