//
// FrameResource.h
//

#pragma once

#include "Utility.h"
#include "UploadBuffer.h"
#include "../Math/Math.h"
#include "Interface/IObject.h"
#include "GeometryManager.h"

#include <type_traits>

using namespace Core;
using namespace Math;
using namespace DirectX;
using namespace WinUtility::GeometryManager;

// Currently as D3DCore, some of them need to be moved into Core (Cross Platform).
namespace D3DCore
{
	// Note: Constant Buffer Limited to 64KB.
	const uint32 MaxLights = 1024;
	const uint16 G_NUM_OBJECTCONSTANT = 64;

	enum ELightType
	{
		LT_Directional,
		LT_Point,
		LT_Spot
	};

	/////////////////////////////
	struct LightDesc
	{
		std::string Name = "Default";
		std::string PathName = "JayouEngine_BuiltIn_Light";
		bool bCanbeDeleted = true;

		Vector3 Translation = { 0.0f };
		Vector3 Rotation = { 0.0f };
		Vector3 Scale = { 1.0f };

		ELightType LightType = LT_Directional;

		XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };  // point/spot light only
		XMFLOAT3 Strength = { 5.5f, 5.5f, 5.5f };
		XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };// directional/spot light only

		float FalloffStart = 10.0f;                          // point/spot light only	
		float FalloffEnd = 50.0f;                           // point/spot light only	
		float SpotPower = 64.0f;                            // spot light only
	};


	struct Light : public IObject
	{

		ELightType LightType;

		Vector3 Translation = { 0.0f };
		Vector3 Rotation = { 0.0f };
		Vector3 Scale = { 1.0f };
		Matrix4 World = Matrix4(EIdentityTag::kIdentity);

		XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };  // point/spot light only
		XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };
		XMFLOAT3 Direction = { 0.0f, -1.0f, 0.25f };// directional/spot light only

		float FalloffStart = 1.0f;                          // point/spot light only	
		float FalloffEnd = 10.0f;                           // point/spot light only	
		float SpotPower = 64.0f;                            // spot light only

		RenderItem* LightRItem = nullptr;

		static int32 Count;

		Light()
		{
			Index = Count++;
		}

		void SetTransFormMatrix(const Vector3& InTranslation, const Vector3& InRotation, const Vector3& InScale)
		{
			World = Matrix4(AffineTransform(InTranslation).Rotation(InRotation));
			Translation = InTranslation;
			Rotation = InRotation;
			Scale = InScale;

			Position = InTranslation;
			Direction = XMFLOAT3(Vector3(World.Get3x3()*Direction));
		}
	};
	/////////////////////////////
	
	/////////////////////////////
	struct MaterialDesc
	{
		std::string Name = "Default";
		std::string PathName = "JayouEngine_BuiltIn_Material";
		bool bCanbeDeleted = true;

		XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };

		Vector3 Translation = { 0.0f };
		Vector3 Rotation = { 0.0f };
		Vector3 Scale = { 1.0f };

		float Roughness = 1.0f;
		float Metallicity = 1.0f;

		int32 DiffuseMapIndex = -1; // if -1, Vertex Color.
		int32 NormalMapIndex = -1;  // if -1, Vertex Normal.
		int32 ORMMapIndex = -1;     // AO, Roughness, Metallicity.

	};

	struct Material : public IObject
	{

		XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };

		// Used in texture mapping.
		Matrix4 MatTransform = Matrix4(EIdentityTag::kIdentity);
		Vector3 Translation = { 0.0f };
		Vector3 Rotation = { 0.0f };
		Vector3 Scale = { 1.0f };

		float Roughness = 0.5f;
		float Metallicity = 0.5f;

		int32 DiffuseMapIndex = -1; // if -1, Vertex Color.
		int32 NormalMapIndex = -1;  // if -1, Vertex Normal.
		int32 ORMMapIndex = -1;     // AO, Roughness, Metallicity.

		static int32 Count;

		Material()
		{
			Index = Count++;
		}

		void SetTransFormMatrix(const Vector3& InTranslation, const Vector3& InRotation, const Vector3& InScale)
		{
			MatTransform = Matrix4(AffineTransform(InTranslation).Rotation(InRotation).Scale(InScale));
			Translation = InTranslation;
			Rotation = InRotation;
			Scale = InScale;
		}
	};	
	/////////////////////////////

	struct PassConstant
	{
		Matrix4 View = Matrix4(EIdentityTag::kIdentity);
		Matrix4 InvView = Matrix4(EIdentityTag::kIdentity);
		Matrix4 Proj = Matrix4(EIdentityTag::kIdentity);
		Matrix4 InvProj = Matrix4(EIdentityTag::kIdentity);
		Matrix4 ViewProj = Matrix4(EIdentityTag::kIdentity);
		Matrix4 InvViewProj = Matrix4(EIdentityTag::kIdentity);
		Matrix4 ShadowTransform = Matrix4(EIdentityTag::kIdentity);

		XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };

		uint32 NumDirLights;

		XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
		XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };

		float NearZ = 0.0f;
		float FarZ = 0.0f;
		float TotalTime = 0.0f;
		float DeltaTime = 0.0f;

		XMFLOAT4 AmbientFactor = { 0.2f, 0.2f, 0.2f, 1.0f };
		XMFLOAT4 FogColor = { 0.7f, 0.7f, 0.7f, 1.0f };
		XMFLOAT4 WireframeColor = XMFLOAT4(Colors::Green);

		float gFogStart = 5.0f;
		float gFogRange = 150.0f;

		uint32 NumPointLights;
		uint32 NumSpotLights;

		int32 CubeMapIndex = -1;
		int32 PassConstantPad0;
		int32 PassConstantPad1;
		int32 PassConstantPad2;
	};

	struct ObjectConstant
	{
		Matrix4 World = Matrix4(EIdentityTag::kIdentity);
		Matrix4 InvTWorld = Matrix4(EIdentityTag::kIdentity);

		int32 MaterialIndex = -1;
		int32 ObjectConstantPad0;
		int32 ObjectConstantPad1;
		int32 ObjectConstantPad2;
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
	struct MaterialData
	{
		XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };

		// Used in texture mapping.
		Matrix4 MatTransform = Matrix4(EIdentityTag::kIdentity);

		float Roughness = 0.5f;
		float Metallicity = 0.5f;
		float MaterialPad0;
		float MaterialPad1;

		int32 DiffuseMapIndex = -1; // if -1, Vertex Color.
		int32 NormalMapIndex = -1;  // if -1, Vertex Normal.
		int32 ORMMapIndex = -1;     // AO, Roughness, Metallicity.
		int32 MaterialPad2;
	};

	struct LightData
	{
		XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };
		float FalloffStart = 1.0f;                          // point/spot light only

		XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };// directional/spot light only
		float FalloffEnd = 10.0f;                           // point/spot light only

		XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };  // point/spot light only
		float SpotPower = 64.0f;                            // spot light only
	};

	class FrameResource
	{
	public:

		FrameResource(ID3D12Device* device) : m_d3dDevice(device) {}
		FrameResource(const FrameResource& res) = delete;
		FrameResource& operator=(const FrameResource& res) = delete;
		~FrameResource() { m_d3dDevice = nullptr; }

		template<typename TConstantType>
		void ResizeBuffer(uint32 count)
		{
			if (count < 1)
				return;
			if (std::is_same<PassConstant, TConstantType>::value) m_passCBuffer = std::make_unique<UploadBuffer<PassConstant>>(m_d3dDevice, count, true);
			else if (std::is_same<ObjectConstant, TConstantType>::value) m_objectCBuffer = std::make_unique<UploadBuffer<ObjectConstant>>(m_d3dDevice, count, true);
			else if (std::is_same<ObjectConstantArray, TConstantType>::value) m_objectArrayCBuffer = std::make_unique<UploadBuffer<ObjectConstantArray>>(m_d3dDevice, count, true);
			else if (std::is_same<MaterialData, TConstantType>::value) m_matSBuffer = std::make_unique<UploadBuffer<MaterialData>>(m_d3dDevice, count, false);
			else if (std::is_same<LightData, TConstantType>::value) m_litSBuffer = std::make_unique<UploadBuffer<LightData>>(m_d3dDevice, count, false);
		}

		// Not Support For MaterialData & LightData.
		template<typename TConstantType>
		void CreateConstantBufferView(D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandle, int32 offset = 0)
		{
			if (std::is_same<MaterialData, TConstantType>::value)
				return;
			if (std::is_same<LightData, TConstantType>::value)
				return;

			uint32 ConstantBufferByteSize = Utility::CalcConstantBufferByteSize(sizeof(TConstantType));
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
			else if (std::is_same<MaterialData, TConstantType>::value) m_matSBuffer->CopyData<TConstantType>(elementIndex, data);
			else if (std::is_same<LightData, TConstantType>::value) m_litSBuffer->CopyData<TConstantType>(elementIndex, data);
		}

		template<typename TConstantType>
		D3D12_GPU_VIRTUAL_ADDRESS GetBufferGPUVirtualAddress()
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress;

			if (std::is_same<PassConstant, TConstantType>::value) cbAddress = m_passCBuffer->Resource()->GetGPUVirtualAddress();
			else if (std::is_same<ObjectConstant, TConstantType>::value) cbAddress = m_objectCBuffer->Resource()->GetGPUVirtualAddress();
			else if (std::is_same<ObjectConstantArray, TConstantType>::value) cbAddress = m_objectArrayCBuffer->Resource()->GetGPUVirtualAddress();
			else if (std::is_same<MaterialData, TConstantType>::value) cbAddress = m_matSBuffer->Resource()->GetGPUVirtualAddress();
			else if (std::is_same<LightData, TConstantType>::value) cbAddress = m_litSBuffer->Resource()->GetGPUVirtualAddress();

			return cbAddress;
		}

	private:

		std::unique_ptr<UploadBuffer<PassConstant>> m_passCBuffer = nullptr;
		std::unique_ptr<UploadBuffer<ObjectConstant>> m_objectCBuffer = nullptr;
		std::unique_ptr<UploadBuffer<ObjectConstantArray>> m_objectArrayCBuffer = nullptr;
		std::unique_ptr<UploadBuffer<MaterialData>> m_matSBuffer = nullptr;
		std::unique_ptr<UploadBuffer<LightData>> m_litSBuffer = nullptr;

		ID3D12Device* m_d3dDevice = nullptr;
	};
	
}
