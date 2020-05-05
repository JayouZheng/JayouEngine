//
// AssimpImporter.cpp
//

#include "AssimpImporter.h"

#include <assimp/Importer.hpp>  // C++ m_importer interface
#include <assimp/scene.h>       // Output data structure

bool Core::AssimpImporter::Import(const ImportGeoDesc& InGeoDesc, const std::string& InFile)
{
	Assimp::Importer importer;

	const aiScene* scene = importer.ReadFile(InFile.data(), InGeoDesc.PPSFlags);

	if (!scene)
	{
		m_errorString.push(std::string(importer.GetErrorString()));
		return false;
	}

	const float maxFloat = std::numeric_limits<float>::max();

	XMFLOAT3 vMinf3(+maxFloat, +maxFloat, +maxFloat);
	XMFLOAT3 vMaxf3(-maxFloat, -maxFloat, -maxFloat);

	XMVECTOR vMin = XMLoadFloat3(&vMinf3);
	XMVECTOR vMax = XMLoadFloat3(&vMaxf3);

	float scale   = InGeoDesc.Scale;
	int numMeshes = scene->mNumMeshes;
	std::vector<Geometry> geometries(numMeshes);
	for (int i = 0; i < numMeshes; ++i)
	{
		if (scene->mMeshes[i]->mPrimitiveTypes != aiPrimitiveType_TRIANGLE)
			continue;

		// Vertex Array.
		int numVertices = scene->mMeshes[i]->mNumVertices;
		std::vector<Vertex> vertices(numVertices);
		for (int j = 0; j < numVertices; ++j)
		{
			vertices[j].Position.x = scale * scene->mMeshes[i]->mVertices[j].x;
			vertices[j].Position.y = scale * scene->mMeshes[i]->mVertices[j].y;
			vertices[j].Position.z = scale * scene->mMeshes[i]->mVertices[j].z;

			vertices[j].Normal.x = scene->mMeshes[i]->mNormals[j].x;
			vertices[j].Normal.y = scene->mMeshes[i]->mNormals[j].y;
			vertices[j].Normal.z = scene->mMeshes[i]->mNormals[j].z;

			vertices[j].TangentU.x = scene->mMeshes[i]->mTangents[j].x;
			vertices[j].TangentU.y = scene->mMeshes[i]->mTangents[j].y;
			vertices[j].TangentU.z = scene->mMeshes[i]->mTangents[j].z;

			vertices[j].TexC.x = scene->mMeshes[i]->mTextureCoords[0][j].x;
			vertices[j].TexC.y = scene->mMeshes[i]->mTextureCoords[0][j].y;

			XMVECTOR pos = XMLoadFloat3(&vertices[j].Position);

			vMin = XMVectorMin(vMin, pos);
			vMax = XMVectorMax(vMax, pos);
		}

		// Index Array.
		int numFaces = scene->mMeshes[i]->mNumFaces;
		std::vector<uint32> indices(numFaces * 3);
		for (int m = 0; m < numFaces; ++m)
		{
			int numIndices = scene->mMeshes[i]->mFaces[m].mNumIndices;
			for (int n = 0; n < numIndices; ++n)
			{
				indices[n + m * 3] = scene->mMeshes[i]->mFaces[m].mIndices[n];
			}
		}

		// Bounds.
		XMFLOAT3 origin;		
		XMFLOAT3 boxExtent;	
		float    sphereRadius;

		XMStoreFloat3(&origin, 0.5f*(vMin + vMax));
		XMStoreFloat3(&boxExtent, 0.5f*(vMax - vMin));
		sphereRadius = Math::Length(Vector3(.5f*(vMax - vMin)));

		geometries[i].Name = InGeoDesc.Name + "_" + std::to_string(i);
		geometries[i].Data.Vertices  = vertices;
		geometries[i].Data.Indices32 = indices;
		geometries[i].Bounds = BoxSphereBounds(origin, boxExtent, sphereRadius);
	}

	for (auto geo : geometries)
	{
		m_geometries[geo.Name] = geo;
	}

	importer.FreeScene();
	return true;
}

Geometry Core::AssimpImporter::GetGeometry(const std::string& InGeoName)
{
	return m_geometries[InGeoName];
}

void Core::AssimpImporter::FreeCachedData()
{
	m_geometries.clear();
}
