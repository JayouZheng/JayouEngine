//
// CubeMap.cpp
//

#include "CubeMap.h"

using namespace D3DCore;
 
CubeMap::CubeMap(ID3D12Device* device, 
	                       UINT width, UINT height,
                           DXGI_FORMAT format, UINT16 DepthOrArraySize, UINT16 MipLevels)
{
	m_d3dDevice = device;

	m_rtvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	m_width = width;
	m_height = height;
	m_format = format;
	
	m_depthOrArraySize = DepthOrArraySize;
	m_mipLevels = MipLevels;

	m_viewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
	m_scissorRect = { 0, 0, (int)width, (int)height };

	BuildResource();
}

ID3D12Resource*  CubeMap::Resource()
{
	return m_cubeMap.Get();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE CubeMap::Srv()
{
	return m_hGpuSrv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE CubeMap::Rtv(int32 InOffset /*= 0*/)
{
	return m_hCpuRtv.Offset(InOffset, m_rtvDescriptorSize);
}

D3D12_VIEWPORT CubeMap::Viewport()const
{
	return m_viewport;
}

D3D12_RECT CubeMap::ScissorRect()const
{
	return m_scissorRect;
}

void CubeMap::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
	                                CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
	                                CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv)
{
	// Save references to the descriptors. 
	m_hCpuSrv = hCpuSrv;
	m_hGpuSrv = hGpuSrv;

	m_hCpuRtv = hCpuRtv;
	
	//  Create the descriptors
	BuildDescriptors();
}

void CubeMap::OnResize(UINT newWidth, UINT newHeight)
{
	if((m_width != newWidth) || (m_height != newHeight))
	{
		m_width = newWidth;
		m_height = newHeight;

		BuildResource();

		// New resource, so we need new descriptors to that resource.
		BuildDescriptors();
	}
}
 
void CubeMap::BuildDescriptors()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = m_format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = m_mipLevels;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

	// Create SRV to the entire cubemap resource.
	m_d3dDevice->CreateShaderResourceView(m_cubeMap.Get(), &srvDesc, m_hCpuSrv);

	// Create RTV to each cube face.
	for(int i = 0; i < m_depthOrArraySize / m_mipLevels; ++i)
	{
		// 3    3    3
		// 2    2    2
		// 1    1    1  // ArraySize = 9, Mips = 3
		// if mips = 1, mipSlice all = 0. It is a Horizontal Line.
		for (int j = 0; j < m_mipLevels; ++j)
		{
			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			rtvDesc.Format = m_format;
			rtvDesc.Texture2DArray.MipSlice = j;
			rtvDesc.Texture2DArray.PlaneSlice = 0;

			// Render target to ith element.
			rtvDesc.Texture2DArray.FirstArraySlice = i; // Vertical Line.

			// Only view one element of the array.
			rtvDesc.Texture2DArray.ArraySize = 1;

			// Create RTV to ith cubemap face.
			m_d3dDevice->CreateRenderTargetView(m_cubeMap.Get(), &rtvDesc, m_hCpuRtv);
			m_hCpuRtv.Offset(1, m_rtvDescriptorSize);
		}		
	}
}

void CubeMap::BuildResource()
{
	// Note, compressed formats cannot be used for UAV.  We get error like:
	// ERROR: ID3D11Device::CreateTexture2D: The format (0x4d, BC3_UNORM) 
	// cannot be bound as an UnorderedAccessView, or cast to a format that
	// could be bound as an UnorderedAccessView.  Therefore this format 
	// does not support D3D11_BIND_UNORDERED_ACCESS.

	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = m_width;
	texDesc.Height = m_height;
	texDesc.DepthOrArraySize = m_depthOrArraySize;
	texDesc.MipLevels = m_mipLevels;
	texDesc.Format = m_format;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = m_format;
	memcpy(clearValue.Color, DirectX::Colors::LightSteelBlue, sizeof(float) * 4);

	ThrowIfFailedV1(m_d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		&clearValue,
		IID_PPV_ARGS(&m_cubeMap)));
}

void CubeMap::BuildCubeFaceCamera(float x, float y, float z)
{
	// Generate the cube map about the given position.
	XMFLOAT3 center(x, y, z);
	XMFLOAT3 worldUp(0.0f, 1.0f, 0.0f);

	// Look along each coordinate axis.
	XMFLOAT3 targets[6] =
	{
		XMFLOAT3(x - 1.0f, y, z), // +X
		XMFLOAT3(x + 1.0f, y, z), // -X
		XMFLOAT3(x, y - 1.0f, z), // +Y
		XMFLOAT3(x, y + 1.0f, z), // -Y
		XMFLOAT3(x, y, z + 1.0f), // +Z
		XMFLOAT3(x, y, z - 1.0f)  // -Z
	};

	// Use world up vector (0,1,0) for all directions except +Y/-Y.  In these cases, we
	// are looking down +Y or -Y, so we need a different "up" vector.
	XMFLOAT3 ups[6] =
	{
		XMFLOAT3(0.0f, -1.0f, 0.0f),  // +X
		XMFLOAT3(0.0f, -1.0f, 0.0f),  // -X
		XMFLOAT3(0.0f, 0.0f, -1.0f),  // +Y
		XMFLOAT3(0.0f, 0.0f, +1.0f),  // -Y
		XMFLOAT3(0.0f, -1.0f, 0.0f),  // +Z
		XMFLOAT3(0.0f, -1.0f, 0.0f)	  // -Z
	};

	for (int i = 0; i < 6; ++i)
	{
		m_cubeMapCamera[i].LookAt(center, targets[i], ups[i]);
		m_cubeMapCamera[i].SetFrustum(0.5f*XM_PI, 1, 1, 0.1f, 1000.0f);
		m_cubeMapCamera[i].UpdateViewMatrix();
	}
}

const Camera& CubeMap::GetCamera(int32 Index) const
{
	return m_cubeMapCamera[Index];
}

DXGI_FORMAT CubeMap::GetFormat() const
{
	return m_format;
}
