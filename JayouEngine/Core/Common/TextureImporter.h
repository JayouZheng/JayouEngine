//
// TextureImporter.h
//

#pragma once

#include "Utility.h"
#include "Interface/IObject.h"

using namespace Core;

namespace Utility
{
	struct ImportTexDesc
	{
		std::string Name;
		std::string PathName;

		bool bIsHDR;
	};

	struct Texture : public IObject
	{

		bool  bIsHDR = false;

		const void* Data;

		DXGI_FORMAT Format;
		uint64 Width;
		uint32 Height;

		uint64 NumBytes;
		uint64 RowBytes;
		uint64 NumRows;

		ComPtr<ID3D12Resource> Resource = nullptr;
		ComPtr<ID3D12Resource> UploadHeap = nullptr;

		CD3DX12_CPU_DESCRIPTOR_HANDLE HCPUDescriptor;
		CD3DX12_GPU_DESCRIPTOR_HANDLE HGPUDescriptor;

		static int32 Count;

		Texture()
		{
			Index = Count++;
		}

		void Free()
		{
			delete Data;
			Data = nullptr;
		}

		~Texture()
		{
			if (Data != nullptr)
			{
				Free();
			}
		}
	};

	class TextureImporter
	{
	public:

		void LoadTexture(Texture* OutTexture);
		void CreateDefaultTexture(Texture* OutTexture, uint64 InWidth, uint32 InHeight);

	protected:

		std::queue<std::string> m_errorString;
	};
}