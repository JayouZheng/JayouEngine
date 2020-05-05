//
// GeometryManager.h
//

#pragma once

#include "D3DDeviceResources.h"

#include "Interface/IObject.h"

using namespace Core;
using namespace D3DCore;
using namespace DirectX;

namespace Utility
{
	namespace GeometryManager
	{
		struct ColorVertex
		{
			XMFLOAT3 Pos;
			XMFLOAT4 Color;
		};

		struct Vertex
		{
			Vertex() {}
			Vertex(
				const XMFLOAT3& p,
				const XMFLOAT3& n,
				const XMFLOAT3& t,
				const XMFLOAT2& uv) :
				Position(p),
				Normal(n),
				TangentU(t),
				TexC(uv) {}
			Vertex(
				float px, float py, float pz,
				float nx, float ny, float nz,
				float tx, float ty, float tz,
				float u, float v) :
				Position(px, py, pz),
				Normal(nx, ny, nz),
				TangentU(tx, ty, tz),
				TexC(u, v) {}

			XMFLOAT3 Position;
			XMFLOAT3 Normal;
			XMFLOAT3 TangentU;
			XMFLOAT2 TexC;
		};

		template<typename TVertex>
		struct GeometryData
		{
			std::vector<TVertex> Vertices;
			std::vector<uint32> Indices32;

			std::vector<uint16>& GetIndices16()
			{
				if (mIndices16.empty())
				{
					mIndices16.resize(Indices32.size());
					for (size_t i = 0; i < Indices32.size(); ++i)
						mIndices16[i] = static_cast<uint16>(Indices32[i]);
				}

				return mIndices16;
			}

		private:
			std::vector<uint16> mIndices16;
		};

		struct BoxSphereBounds
		{
		public:

			/** Holds the origin of the bounding box and sphere. */
			XMFLOAT3 Origin;

			/** Holds the extent of the bounding box. */
			XMFLOAT3 BoxExtent;

			/** Holds the radius of the bounding sphere. */
			float SphereRadius;

			BoundingBox BoxBounds;
			BoundingSphere SphereBounds;

			/** Default constructor. */
			BoxSphereBounds() { }

			/**
			 * Creates and initializes a new instance from the specified parameters.
			 *
			 * @param InOrigin origin of the bounding box and sphere.
			 * @param InBoxExtent half size of box.
			 * @param InSphereRadius radius of the sphere.
			 */
			BoxSphereBounds(const XMFLOAT3& InOrigin, const XMFLOAT3& InBoxExtent, float InSphereRadius)
				: Origin(InOrigin)
				, BoxExtent(InBoxExtent)
				, SphereRadius(InSphereRadius)
			{
				BoxBounds = BoundingBox(Origin, BoxExtent);
				SphereBounds = BoundingSphere(Origin, SphereRadius);
			}
		};

		// Defines a subrange of geometry in a D3DGeometry.  This is for when multiple
		// geometries are stored in one vertex and index buffer.  It provides the offsets
		// and data needed to draw a subset of geometry stores in the vertex and index
		// buffers.
		struct Section
		{
			// DrawIndexedInstanced parameters.
			uint32 IndexCountPerInstance = 0;
			uint32 InstanceCount = 1;
			uint32 StartIndexLocation = 0;
			int32  BaseVertexLocation = 0;
			uint32 StartInstanceLocation = 0;

			// Bounding box of the geometry defined by this submesh.
			BoxSphereBounds Bounds;
		};

		struct Geometry : public IObject
		{
			GeometryData<Vertex> Data;
			BoxSphereBounds  Bounds;

			// Animation...
			// Material...
			// Texture...
		};

		struct Light : public IObject
		{
			DirectX::XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };
			float FalloffStart = 1.0f;                          // point/spot light only
			DirectX::XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };// directional/spot light only
			float FalloffEnd = 10.0f;                           // point/spot light only
			DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };  // point/spot light only
			float SpotPower = 64.0f;                            // spot light only
		};
	}
}

// Currently as WinUtility, some of them need to be moved into Utility (Cross Platform).
namespace WinUtility
{
	namespace GeometryManager
	{
		using namespace Utility::GeometryManager;

		struct D3DGeometry
		{
			// Give it a name so we can look it up by name.
			std::string Name;

			// System memory copies.  Use Blobs because the vertex/index format can be generic.
			// It is up to the client to cast appropriately.
			// Using D3DCreateBlob(...), CopyMemory(...).
			ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
			ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

			// GPU Buffer.
			ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
			ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

			// Uploader Buffer...CPU to GPU.
			ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
			ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

			// Data about the buffers.
			uint32 VertexByteStride = 0;
			uint32 VertexBufferByteSize = 0;
			DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;
			uint32 IndexBufferByteSize = 0;

			// A D3DGeometry may store multiple geometries in one vertex/index buffer.
			// Use this container to define the Submesh geometries so we can draw
			// the Submeshes individually.
			std::unordered_map<std::string, Section> DrawArgs;	

			D3D12_VERTEX_BUFFER_VIEW VertexBufferView() const
			{
				D3D12_VERTEX_BUFFER_VIEW vbv;
				vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
				vbv.StrideInBytes = VertexByteStride;
				vbv.SizeInBytes = VertexBufferByteSize;

				return vbv;
			}

			D3D12_INDEX_BUFFER_VIEW IndexBufferView() const
			{
				D3D12_INDEX_BUFFER_VIEW ibv;
				ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
				ibv.Format = IndexFormat;
				ibv.SizeInBytes = IndexBufferByteSize;

				return ibv;
			}

			// We can free this memory after we finish upload to the GPU.
			void DisposeUploaders()
			{
				VertexBufferUploader = nullptr;
				IndexBufferUploader = nullptr;
			}
		};

		struct RenderItem
		{
		public:

			RenderItem() { ObjectCBufferIndex = ObjectCount++; }
			RenderItem(const RenderItem& item) = delete;

			static int32 ObjectCount;
			int32 ObjectCBufferIndex; // Also as RenderItem Index.

			// Give it a name so we can look it up by name.
			std::string Name;

			std::unique_ptr<D3DGeometry> Geometry = nullptr;
			
			Matrix4 Local = Matrix4(EIdentityTag::kIdentity);
			Matrix4 World = Matrix4(EIdentityTag::kIdentity);

			bool bObjectDataChanged = true;

			// Primitive topology.
			D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

			///////////////////////////////////////////////////////
			// Custom Data Field. / Per FScene Structure Buffer Offset.
			int PerFSceneSBufferOffset = 0;
			///////////////////////////////////////////////////////

			// NOTE: NO Section.
			template<typename _T1, typename _T2>
			void CreateCommonGeometry(D3DDeviceResources* deviceResources, const std::string& name, const std::vector< _T1>& vertices, const std::vector< _T2>& indices)
			{
				const UINT vbByteSize = (UINT)vertices.size() * sizeof(_T1);
				const UINT ibByteSize = (UINT)indices.size() * sizeof(_T2);

				Name = name;

				Geometry = std::make_unique<D3DGeometry>();

				ThrowIfFailedV1(D3DCreateBlob(vbByteSize, &Geometry->VertexBufferCPU));
				CopyMemory(Geometry->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

				ThrowIfFailedV1(D3DCreateBlob(ibByteSize, &Geometry->IndexBufferCPU));
				CopyMemory(Geometry->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

				deviceResources->CreateDefaultBuffer(vertices.data(), vbByteSize,
					&Geometry->VertexBufferGPU, &Geometry->VertexBufferUploader);
				deviceResources->CreateDefaultBuffer(indices.data(), ibByteSize,
					&Geometry->IndexBufferGPU, &Geometry->IndexBufferUploader);

				// Vertex Buffer View Data.
				Geometry->VertexByteStride = sizeof(_T1);
				Geometry->VertexBufferByteSize = vbByteSize;

				// Index Buffer View Data.
				if (std::is_same<_T2, uint16>::value)
					Geometry->IndexFormat = DXGI_FORMAT_R16_UINT;
				else if ((std::is_same<_T2, uint32>::value))
					Geometry->IndexFormat = DXGI_FORMAT_R32_UINT;
				Geometry->IndexBufferByteSize = ibByteSize;

				Section submesh;
				submesh.IndexCountPerInstance = (UINT)indices.size();
				submesh.InstanceCount = 1;
				submesh.StartIndexLocation = 0;
				submesh.BaseVertexLocation = 0;
				submesh.StartInstanceLocation = 0;

				Geometry->DrawArgs[name] = submesh;
			}
							
			template<typename TLambda = PFVOID>
			void Draw(ID3D12GraphicsCommandList* commandList, const TLambda& lambda = defalut)
			{
				commandList->IASetVertexBuffers(0, 1, &Geometry->VertexBufferView());
				commandList->IASetIndexBuffer(&Geometry->IndexBufferView());
				commandList->IASetPrimitiveTopology(PrimitiveType);

				// Set/Bind Per Object Data.
				lambda();
				
				for (auto& e : Geometry->DrawArgs)
				{
					Section& submesh = e.second;
					commandList->DrawIndexedInstanced(submesh.IndexCountPerInstance,
						submesh.InstanceCount,
						submesh.StartIndexLocation,
						submesh.BaseVertexLocation,
						submesh.StartInstanceLocation);
				}
			}

		private:

			static void defalut() {}
		};

		class GeometryCreator
		{
		public:

			///<summary>
			/// Creates an mxn grid in the xz-plane with m rows and n columns, centered
			/// at the origin with the specified width and depth.
			///</summary>
			static GeometryData<ColorVertex> CreateLineGrid(float width, float depth, uint32 m, uint32 n);

			static GeometryData<ColorVertex> CreateDefaultBox();

			///<summary>
			/// Creates a box centered at the origin with the given dimensions, where each
			/// face has m rows and n columns of vertices.
			///</summary>
			static GeometryData<Vertex> CreateBox(float width, float height, float depth, uint32 numSubdivisions);

			///<summary>
			/// Creates a sphere centered at the origin with the given radius.  The
			/// slices and stacks parameters control the degree of tessellation.
			///</summary>
			static GeometryData<Vertex> CreateSphere(float radius, uint32 sliceCount, uint32 stackCount);

			///<summary>
			/// Creates a geosphere centered at the origin with the given radius.  The
			/// depth controls the level of tessellation.
			///</summary>
			static GeometryData<Vertex> CreateGeosphere(float radius, uint32 numSubdivisions);

			///<summary>
			/// Creates a cylinder parallel to the y-axis, and centered about the origin.
			/// The bottom and top radius can vary to form various cone shapes rather than true
			/// cylinders.  The slices and stacks parameters control the degree of tessellation.
			///</summary>
			static GeometryData<Vertex> CreateCylinder(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount);

			///<summary>
			/// Creates an mxn grid in the xz-plane with m rows and n columns, centered
			/// at the origin with the specified width and depth.
			///</summary>
			static GeometryData<Vertex> CreateGrid(float width, float depth, uint32 m, uint32 n);

			///<summary>
			/// Creates a quad aligned with the screen.  This is useful for postprocessing and screen effects.
			///</summary>
			static GeometryData<Vertex> CreateQuad(float x, float y, float w, float h, float depth);

		private:

			static void Subdivide(GeometryData<Vertex>& meshData);
			static Vertex MidPoint(const Vertex& v0, const Vertex& v1);
			static void BuildCylinderTopCap(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount, GeometryData<Vertex>& meshData);
			static void BuildCylinderBottomCap(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount, GeometryData<Vertex>& meshData);
		};

	}
}

