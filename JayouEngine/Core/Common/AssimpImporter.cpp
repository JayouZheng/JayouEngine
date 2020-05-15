//
// AssimpImporter.cpp
//

#include "AssimpImporter.h"

#include <assimp/Importer.hpp>  // C++ m_importer interface
#include <assimp/scene.h>       // Output data structure

bool Core::AssimpImporter::Import(const ImportGeoDesc& InGeoDesc)
{
	FreeCachedData();

	Assimp::Importer importer;

	const aiScene* scene = importer.ReadFile(InGeoDesc.PathName.data(), InGeoDesc.PPSFlags);

	if (!scene)
	{
		m_errorString.push(std::string(importer.GetErrorString()));
		return false;
	}

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
			vertices[j].Position.x = scene->mMeshes[i]->mVertices[j].x;
			vertices[j].Position.y = scene->mMeshes[i]->mVertices[j].y;
			vertices[j].Position.z = scene->mMeshes[i]->mVertices[j].z;

			if (scene->mMeshes[i]->mNormals != nullptr)
			{
				vertices[j].Normal.x = scene->mMeshes[i]->mNormals[j].x;
				vertices[j].Normal.y = scene->mMeshes[i]->mNormals[j].y;
				vertices[j].Normal.z = scene->mMeshes[i]->mNormals[j].z;
			}
	
			if (scene->mMeshes[i]->mTangents != nullptr)
			{
				vertices[j].TangentU.x = scene->mMeshes[i]->mTangents[j].x;
				vertices[j].TangentU.y = scene->mMeshes[i]->mTangents[j].y;
				vertices[j].TangentU.z = scene->mMeshes[i]->mTangents[j].z;
			}
			
			if (scene->mMeshes[i]->mTextureCoords != nullptr)
			{
				if (scene->mMeshes[i]->mTextureCoords[0] != nullptr)
				{
					vertices[j].TexC.x = scene->mMeshes[i]->mTextureCoords[0][j].x;
					vertices[j].TexC.y = scene->mMeshes[i]->mTextureCoords[0][j].y;
				}
			}			
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

		geometries[i].Name = InGeoDesc.Name + "_" + std::to_string(i);
		geometries[i].PathName = InGeoDesc.PathName;
		geometries[i].Data.Vertices  = vertices;
		geometries[i].Data.Indices32 = indices;
		geometries[i].Bounds = geometries[i].Data.CalcBounds();
	}

	for (auto geo : geometries)
	{
		if (!geo.Data.Vertices.empty() && !geo.Data.Indices32.empty())
		{
			m_geometries[geo.Name] = geo;
		}		
	}

	importer.FreeScene();
	return true;
}

Geometry Core::AssimpImporter::GetGeometry(const std::string& InGeoName)
{
	if (m_geometries.find(InGeoName + "_0") != m_geometries.end())
	{
		return m_geometries[InGeoName + "_0"];
	}
	return m_geometries[InGeoName];
}

const std::unordered_map<std::string, Geometry>& Core::AssimpImporter::GetAllGeometries() const
{
	return m_geometries;
}

void Core::AssimpImporter::FreeCachedData()
{
	m_geometries.clear();
}
