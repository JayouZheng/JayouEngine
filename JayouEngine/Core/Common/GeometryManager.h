//
// GeometryManager.h
//

#pragma once

#include "Utility.h"
#include "../Math/Math.h"
#include "Interface/IObject.h"

using namespace Math;
using namespace Core;
using namespace DirectX;

namespace Utility
{
	namespace GeometryManager
	{
		enum EBuiltInGeoType
		{
			BG_Box,
			BG_Sphere_1,
			BG_Sphere_2,
			BG_Cylinder,
			BG_Plane
		};

		enum RenderLayer
		{
			Opaque,
			Transparent,
			AlphaTested,
			ScreenQuad,
			Line,
			Selectable,
			Selected,
			SkySphere,
			All,
			Count
		};

		enum EVertexFormat
		{
			VF_ColorVertex,
			VF_Vertex
		};

		struct BuiltInGeoDesc
		{
			std::string Name;
			std::string PathName = "JayouEngine_BuiltIn_Geometry";

			Vector3 Translation = { 0.0f };
			Vector3 Rotation = { 0.0f };
			Vector3 Scale = { 1.0f };

			float Depth = 10.0f;
			float Width = 10.0f;
			float Height = 10.0f;
			
			float Radius = 20.0f;
			float TopRadius = 2.5f;
			float BottomRadius = 2.5f;

			int32 SliceCount = 5;
			int32 StackCount = 5;
			int32 NumSubdivisions = 1;

			EBuiltInGeoType GeoType = BG_Box;
			XMFLOAT4 Color = XMFLOAT4(Colors::Gray);
		};

		struct ColorVertex
		{			
			XMFLOAT4 Color = XMFLOAT4(Colors::LightGray);
			XMFLOAT3 Position;

			ColorVertex() {}
			ColorVertex(
				const XMFLOAT3& p,
				const XMFLOAT4& c) :
				Position(p),
				Color(c) {}
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

			XMFLOAT4 Color = XMFLOAT4(Colors::LightGray);
			XMFLOAT3 Position;
			XMFLOAT3 Normal;
			XMFLOAT3 TangentU;
			XMFLOAT2 TexC;
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

			BoxSphereBounds CalcBounds()
			{
				const float maxFloat = std::numeric_limits<float>::max();

				Vector3 vmin(+maxFloat);
				Vector3 vmax(-maxFloat);

				for (auto vertex : Vertices)
				{
					Vector3 pos = vertex.Position;
					vmin = Math::Min(vmin, pos);
					vmax = Math::Max(vmax, pos);
				}

				// Bounds.
				XMFLOAT3 origin;
				XMFLOAT3 boxExtent;
				float    sphereRadius;

				origin = 0.5f*(vmin + vmax);
				boxExtent = 0.5f*(vmax - vmin);
				sphereRadius = Math::Length(Vector3(.5f*(vmax - vmin)));

				return BoxSphereBounds(origin, boxExtent, sphereRadius);
			}

			void SetColor(const XMFLOAT4& InColor)
			{
				for (auto& vertex : Vertices)
				{
					vertex.Color = InColor;
				}
			}

		private:
			std::vector<uint16> mIndices16;
		};

		// Defines a subrange of geometry in a D3DRenderData.  This is for when multiple
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

			// Bounding box of the geometry defined by this section.
			BoxSphereBounds Bounds;

			template<typename TVertex, typename TIndex>
			void CalcBounds(const std::vector<TVertex>& vertices, const std::vector<TIndex>& indices)
			{
				uint32 maxVertexId = 0;
				for (uint32 i = StartIndexLocation; i < (StartIndexLocation + IndexCountPerInstance); ++i)
				{
					maxVertexId = Math::Max(maxVertexId, (uint32)indices[i]);
				}

				const float maxFloat = std::numeric_limits<float>::max();

				Vector3 vmin(+maxFloat);
				Vector3 vmax(-maxFloat);

				for (uint32 i = BaseVertexLocation; i <= maxVertexId; ++i)
				{
					Vector3 pos = vertices[i].Position;
					vmin = Math::Min(vmin, pos);
					vmax = Math::Max(vmax, pos);
				}

				// Bounds.
				XMFLOAT3 origin;
				XMFLOAT3 boxExtent;
				float    sphereRadius;

				origin = 0.5f*(vmin + vmax);
				boxExtent = 0.5f*(vmax - vmin);
				sphereRadius = Math::Length(Vector3(.5f*(vmax - vmin)));

				Bounds = BoxSphereBounds(origin, boxExtent, sphereRadius);
			}
		};	

		struct Geometry : public IObject
		{
			GeometryData<Vertex> Data;
			BoxSphereBounds      Bounds;

			void CalcBounds()
			{
				Bounds = Data.CalcBounds();
			}

			void SetColor(const XMFLOAT4& InColor)
			{
				for (auto& vertex : Data.Vertices)
				{
					vertex.Color = InColor;
				}
			}

			GeometryData<ColorVertex> GetSimpleColorGeometry(const XMFLOAT4& InColor)
			{
				SetColor(InColor);
				GeometryData<ColorVertex> colorGeo;
				for (auto vertex : Data.Vertices)
				{				
					colorGeo.Vertices.push_back(ColorVertex(vertex.Position, InColor));				
				}
				colorGeo.Indices32 = Data.Indices32;
				return colorGeo;
			}

			// Animation...
			// Material...
			// Texture...
		};
		
	}
}

// Currently as WinUtility, some of them need to be moved into Utility (Cross Platform).
namespace WinUtility
{
	namespace GeometryManager
	{
		using namespace Utility::GeometryManager;

		struct D3DRenderData
		{
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

			EVertexFormat VertexFormat = VF_Vertex;

			// A D3DRenderData may store multiple geometries in one vertex/index buffer.
			// Use this container to define the Submesh geometries so we can draw
			// the Submeshes individually.
			std::unordered_map<std::string, Section> Sections;	

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

		struct RenderItem : public IObject
		{
		public:

			// Also as RenderItem Index.
			int32  MaterialIndex = 0;

			uint32 NumVertices;
			uint32 NumIndices;

			XMFLOAT4 VertexColor = XMFLOAT4(Colors::Gray);

			// Transform Matrix.
			Matrix4                        TransFormMatrix = Matrix4(kIdentity);
			Vector3                        Translation = { 0.0f };
			Vector3                        Rotation = { 0.0f };
			Vector3                        Scale = { 1.0f };

			BoxSphereBounds                Bounds;

			// Primitive topology.
			D3D12_PRIMITIVE_TOPOLOGY       PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

			std::unique_ptr<D3DRenderData> RenderData = nullptr;

			bool                           bIntersectBoundingOnly = false;
			bool                           bCastShadow = true;

			GeometryData<Vertex>           CachedGeometryData;
			BuiltInGeoDesc                 CachedBuiltInGeoDesc;

			static int32 Count;

			RenderItem()
			{
				Index = Count++;
			}

			void SetTransFormMatrix(const Vector3& InTranslation, const Vector3& InRotation, const Vector3& InScale)
			{
				TransFormMatrix = Matrix4(AffineTransform(InTranslation).Rotation(InRotation).Scale(InScale));
				Translation = InTranslation;
				Rotation = InRotation;
				Scale = InScale;
			}
		};

		class GeometryCreator
		{
		public:

			///<summary>
			/// Creates an mxn grid in the xz-plane with m rows and n columns, centered
			/// at the origin with the specified width and depth.
			///</summary>
			static GeometryData<ColorVertex> CreateLineGrid(float width, float depth, uint32 m, uint32 n, 
				const XMFLOAT4& InColorX = XMFLOAT4(Colors::Red),
				const XMFLOAT4& InColorZ = XMFLOAT4(Colors::Lime),
				const XMFLOAT4& InColorCell = XMFLOAT4(Colors::Gray),
				const XMFLOAT4& InColorBlock = XMFLOAT4(Colors::Blue));

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
			static GeometryData<Vertex> CreatePlane(float width, float depth, uint32 m, uint32 n);

			///<summary>
			/// Creates a quad aligned with the screen.  This is useful for postprocessing and screen effects.
			///</summary>
			static GeometryData<Vertex> CreateQuad(float x, float y, float w, float h, float depth);

			static GeometryData<ColorVertex> CreateDirLightGeo();
			static GeometryData<ColorVertex> CreatePointLightGeo();
			static GeometryData<ColorVertex> CreateSpotLightGeo();

		private:

			static void Subdivide(GeometryData<Vertex>& meshData);
			static Vertex MidPoint(const Vertex& v0, const Vertex& v1);
			static void BuildCylinderTopCap(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount, GeometryData<Vertex>& meshData);
			static void BuildCylinderBottomCap(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount, GeometryData<Vertex>& meshData);
		};

	}
}

