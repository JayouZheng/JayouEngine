//
// CubeMap.h
//

#pragma once

#include "D3DDeviceResources.h"
#include "Camera.h"

namespace D3DCore
{
	enum class CubeMapFace : int
	{
		PositiveX = 0,
		NegativeX = 1,
		PositiveY = 2,
		NegativeY = 3,
		PositiveZ = 4,
		NegativeZ = 5
	};

	class CubeMap
	{
	public:
		CubeMap(ID3D12Device* device,
			UINT width, UINT height,
			DXGI_FORMAT format, UINT16 DepthOrArraySize, UINT16 MipLevels);

		CubeMap(const CubeMap& rhs) = delete;
		CubeMap& operator=(const CubeMap& rhs) = delete;
		~CubeMap() = default;

		ID3D12Resource* Resource();
		CD3DX12_GPU_DESCRIPTOR_HANDLE Srv();
		CD3DX12_CPU_DESCRIPTOR_HANDLE Rtv(int32 InOffset = 0);

		D3D12_VIEWPORT Viewport()const;
		D3D12_RECT ScissorRect()const;

		void BuildDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
			CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
			CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv);

		void OnResize(UINT newWidth, UINT newHeight);

		void BuildCubeFaceCamera(float x, float y, float z);

		const Camera& GetCamera(int32 Index) const;

		DXGI_FORMAT GetFormat() const;

	private:
		void BuildDescriptors();
		void BuildResource();

	private:

		ID3D12Device* m_d3dDevice = nullptr;

		D3D12_VIEWPORT m_viewport;
		D3D12_RECT m_scissorRect;

		UINT m_width = 0;
		UINT m_height = 0;
		DXGI_FORMAT m_format = DXGI_FORMAT_R8G8B8A8_UNORM;
		UINT16 m_depthOrArraySize;
		UINT16 m_mipLevels;

		CD3DX12_CPU_DESCRIPTOR_HANDLE m_hCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE m_hGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE m_hCpuRtv;

		ComPtr<ID3D12Resource> m_cubeMap = nullptr;
		UINT m_rtvDescriptorSize;
		Camera m_cubeMapCamera[6];
	};

}
 