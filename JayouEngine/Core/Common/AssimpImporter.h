//
// AssimpImporter.h
//

#pragma once

#include "Interface/IGeoImporter.h"

namespace Core
{
	class AssimpImporter : public IGeoImporter
	{
	public:

		AssimpImporter() = default;

		bool Import(const ImportGeoDesc& InGeoDesc, const std::string& InFile) override;

		Geometry GetGeometry(const std::string& InGeoName) override;

		void FreeCachedData() override;

	protected:

		std::queue<std::string> m_errorString;

		std::unordered_map<std::string, Geometry> m_geometries;
	};
}
