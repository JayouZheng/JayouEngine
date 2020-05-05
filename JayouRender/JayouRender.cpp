//
// JayouRender.cpp
//

#define _WINDOWS

#include "Core/JayouEngine.h"
#include "Core/Common/Interface/IScene.h"
#include "Core/Common/Interface/IGeoImporter.h"
#include "Core/Common/SmartPtr.h"

// IO Test.
#include <iostream>

using namespace Core;
using namespace Utility::GeometryManager;

int main(int argc, char* argv[])
{
	CoreModule coreModule;
	SmartPtr<IJayouEngine> engine = coreModule.GetJayouEngine();

	if (engine != nullptr)
	{
		engine->Init(ES_WithEditor);
		SmartPtr<IScene> scene = engine->GetScene();

		if (scene == nullptr)
		{
			std::cout << "scene is null..." << std::endl;
		}

		SmartPtr<IGeoImporter> importer = engine->GetAssimpImporter();
		ImportGeoDesc geoDesc;
		geoDesc.Name = "MyTest";
		if (importer->Import(geoDesc, "Cerberus_LP.FBX"))
		{
			Geometry geo = importer->GetGeometry("MyTest_0");
			std::cout << "fbx imported success..." << std::endl;
		}
		else
		{
			std::cout << "fbx imported failed, can't find resource..." << std::endl;
		}
	}
	else
	{
		std::cout << "engine is null..." << std::endl;
	}

	system("pause");
	return 0;
}
