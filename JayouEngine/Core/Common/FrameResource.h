//
// FrameResource.h
//

#pragma once

#include "Utility.h"
#include "UploadBuffer.h"
#include <type_traits>

// Currently as D3DCore, some of them need to be moved into Core (Cross Platform).
namespace D3DCore
{
	// Note: Constant Buffer Limited to 64KB.

	const uint16 G_NUM_OBJECTCONSTANT = 2;

	struct PassConstant
	{
		Matrix4 ViewProj = Matrix4(EIdentityTag::kIdentity);
		float Overflow = 1000.0f;
	};

	struct ObjectConstant
	{
		Matrix4 World = Matrix4(EIdentityTag::kIdentity);		
		int PerFSceneSBufferOffset = 0;
	};

	struct ObjectConstantArray // Limited to 64KB.
	{
		ObjectConstant Array[G_NUM_OBJECTCONSTANT];

		ObjectConstantArray()
		{
			ZeroMemory(Array, sizeof(Array));
		}
	};

	// Aim for structures with sizes divisible by 128 bits (sizeof float4). 
	// NVIDIA. Understanding Structured Buffer Performance.
	struct StructureBuffer
	{
		Vector4 Color;
	};

	class FrameResource
	{
	public:

		FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount = 0, UINT arrayCount = 0, UINT SBufferCount = 0)
		{
			if (passCount > 0) m_passCBuffer = std::make_unique<UploadBuffer<PassConstant>>(device, passCount, true);
			if (objectCount > 0) m_objectCBuffer = std::make_unique<UploadBuffer<ObjectConstant>>(device, objectCount, true);
			if (arrayCount > 0) m_objectArrayCBuffer = std::make_unique<UploadBuffer<ObjectConstantArray>>(device, arrayCount, true);
			if (SBufferCount > 0) m_structureBuffer = std::make_unique<UploadBuffer<StructureBuffer>>(device, SBufferCount, false);

			m_d3dDevice = device;
		}
		FrameResource(const FrameResource& res) = delete;
		FrameResource& operator=(const FrameResource& res) = delete;
		~FrameResource() { m_d3dDevice = nullptr; }

		template<typename TConstantType>
		void ResizeBuffer(UINT count)
		{
			if (count < 1)
				return;
			if (std::is_same<PassConstant, TConstantType>::value) m_passCBuffer = std::make_unique<UploadBuffer<PassConstant>>(m_d3dDevice, count, true);
			else if (std::is_same<ObjectConstant, TConstantType>::value) m_objectCBuffer = std::make_unique<UploadBuffer<ObjectConstant>>(m_d3dDevice, count, true);
			else if (std::is_same<ObjectConstantArray, TConstantType>::value) m_objectArrayCBuffer = std::make_unique<UploadBuffer<ObjectConstantArray>>(m_d3dDevice, count, true);
			else if (std::is_same<StructureBuffer, TConstantType>::value) m_structureBuffer = std::make_unique<UploadBuffer<StructureBuffer>>(m_d3dDevice, count, false);
		}

		// Not Support For StructureBuffer.
		template<typename TConstantType>
		void CreateConstantBufferView(D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandle, INT offset = 0)
		{
			if (std::is_same<StructureBuffer, TConstantType>::value)
				return;
			UINT ConstantBufferByteSize = Utility::CalcConstantBufferByteSize(sizeof(TConstantType));
			D3D12_GPU_VIRTUAL_ADDRESS ConstantBufferAddress = GetBufferGPUVirtualAddress<TConstantType>();
				
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = ConstantBufferAddress + offset * ConstantBufferByteSize;
			cbvDesc.SizeInBytes = Utility::CalcConstantBufferByteSize(sizeof(TConstantType));

			m_d3dDevice->CreateConstantBufferView(&cbvDesc, cpuDescHandle);
		}

		template<typename TConstantType>
		void CopyData(int elementIndex, const TConstantType& data)
		{
			if (std::is_same<PassConstant, TConstantType>::value) m_passCBuffer->CopyData<TConstantType>(elementIndex, data);
			else if (std::is_same<ObjectConstant, TConstantType>::value) m_objectCBuffer->CopyData<TConstantType>(elementIndex, data);
			else if (std::is_same<ObjectConstantArray, TConstantType>::value) m_objectArrayCBuffer->CopyData<TConstantType>(elementIndex, data);
			else if (std::is_same<StructureBuffer, TConstantType>::value) m_structureBuffer->CopyData<TConstantType>(elementIndex, data);
		}

		template<typename TConstantType>
		D3D12_GPU_VIRTUAL_ADDRESS GetBufferGPUVirtualAddress()
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress;

			if (std::is_same<PassConstant, TConstantType>::value) cbAddress = m_passCBuffer->Resource()->GetGPUVirtualAddress();
			else if (std::is_same<ObjectConstant, TConstantType>::value) cbAddress = m_objectCBuffer->Resource()->GetGPUVirtualAddress();
			else if (std::is_same<ObjectConstantArray, TConstantType>::value) cbAddress = m_objectArrayCBuffer->Resource()->GetGPUVirtualAddress();
			else if (std::is_same<StructureBuffer, TConstantType>::value) cbAddress = m_structureBuffer->Resource()->GetGPUVirtualAddress();

			return cbAddress;
		}

	private:

		std::unique_ptr<UploadBuffer<PassConstant>> m_passCBuffer = nullptr;
		std::unique_ptr<UploadBuffer<ObjectConstant>> m_objectCBuffer = nullptr;
		std::unique_ptr<UploadBuffer<ObjectConstantArray>> m_objectArrayCBuffer = nullptr;
		std::unique_ptr<UploadBuffer<StructureBuffer>> m_structureBuffer = nullptr;

		ID3D12Device* m_d3dDevice = nullptr;
	};
	
}
