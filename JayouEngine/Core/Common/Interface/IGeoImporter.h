//
// IGeoImporter.h
//

#pragma once

#include "../TypeDef.h"
#include "../GeometryManager.h"
using namespace Utility::GeometryManager;

namespace Core
{
	struct ImportGeoDesc
	{
		std::string  Name = "Default";
		float        Scale = 1.0f;

		// aiPostProcessSteps
		uint32       PPSFlags =
			aiProcess_CalcTangentSpace |
			aiProcess_Triangulate |
			aiProcess_JoinIdenticalVertices | // If your application deals with indexed geometry, this step is compulsory.
			aiProcess_ConvertToLeftHanded |   // For D3D App.
			aiProcess_SortByPType;
	};

	class IGeoImporter
	{
	public:

		virtual bool Import(const ImportGeoDesc& InGeoDesc, const std::string& InFile) = 0;

		virtual Geometry GetGeometry(const std::string& InGeoName) = 0;

		virtual void FreeCachedData() = 0;
	};
}
