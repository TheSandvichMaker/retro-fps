ID3D12Resource *d3d12_create_upload_buffer(ID3D12Device *device, uint32_t size, void *initial_data, const wchar_t *debug_name)
{
	D3D12_HEAP_PROPERTIES heap_properties = {
		.Type = D3D12_HEAP_TYPE_UPLOAD,
	};

	D3D12_RESOURCE_DESC desc = {
		.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER,
		.Width            = size,
		.Height           = 1,
		.DepthOrArraySize = 1,
		.MipLevels        = 1,
		.SampleDesc       = { .Count = 1, .Quality = 0 },
		.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
		.Format           = DXGI_FORMAT_UNKNOWN,
	};

	ID3D12Resource *buffer;
	HRESULT hr = ID3D12Device_CreateCommittedResource(device,
													  &heap_properties,
													  D3D12_HEAP_FLAG_NONE,
													  &desc,
													  D3D12_RESOURCE_STATE_COMMON,
													  NULL,
													  &IID_ID3D12Resource,
													  &buffer);
	D3D12_CHECK_HR(hr, return NULL);

	if (initial_data)
	{
		void *mapped = NULL;

		hr = ID3D12Resource_Map(buffer, 0, &(D3D12_RANGE){ 0 }, &mapped);
		D3D12_CHECK_HR(hr, return NULL);

		memcpy(mapped, initial_data, size);

		ID3D12Resource_Unmap(buffer, 0, NULL);
	}

	if (debug_name)
	{
		ID3D12Object_SetName((ID3D12Object *)buffer, debug_name);
	}

	return buffer;
}

