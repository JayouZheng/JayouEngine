//
// Scene.cpp
//

#include "Scene.h"
#include "Interface/IObject.h"

// IO Test
#include <iostream>

using namespace Core;

Scene_Impl::Scene_Impl()
{

}

void Scene_Impl::Add(IObject* InObject)
{
	std::cout << "Scene Add Object [" << InObject->Name << "]" << std::endl;
}
