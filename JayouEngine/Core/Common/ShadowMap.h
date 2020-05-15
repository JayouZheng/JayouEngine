//
// ShadowMap.h
//

#pragma once

#include "D3DDeviceResources.h"

namespace D3DCore
{
	class ShadowMap
	{
	public:
		ShadowMap(ID3D12Device* device,
			int32 width, int32 height);

		ShadowMap(const ShadowMap& rhs) = delete;
		ShadowMap& operator=(const ShadowMap& rhs) = delete;
		~ShadowMap() = default;

		int32 GetWidth()const;
		int32 GetHeight()const;
		DXGI_FORMAT GetFormat() const;
		ID3D12Resource* Resource();
		CD3DX12_GPU_DESCRIPTOR_HANDLE Srv()const;
		CD3DX12_CPU_DESCRIPTOR_HANDLE Dsv()const;

		D3D12_VIEWPORT Viewport()const;
		D3D12_RECT ScissorRect()const;

		void BuildDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
			CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
			CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv);

		void OnResize(int32 newWidth, int32 newHeight);

	private:
		void BuildDescriptors();
		void BuildResource();

	private:

		ID3D12Device* m_d3dDevice = nullptr;

		D3D12_VIEWPORT m_viewport;
		D3D12_RECT m_scissorRect;

		int32 m_width = 0;
		int32 m_height = 0;
		DXGI_FORMAT m_format = DXGI_FORMAT_R24G8_TYPELESS;

		CD3DX12_CPU_DESCRIPTOR_HANDLE m_hCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE m_hGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE m_hCpuDsv;

		Microsoft::WRL::ComPtr<ID3D12Resource> m_shadowMap = nullptr;
	};
}
