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

		bool Import(const ImportGeoDesc& InGeoDesc) override;

		Geometry GetGeometry(const std::string& InGeoName) override;

		const std::unordered_map<std::string, Geometry>& GetAllGeometries() const;

		void FreeCachedData() override;

	protected:

		std::queue<std::string> m_errorString;

		std::unordered_map<std::string, Geometry> m_geometries;
	};
}
