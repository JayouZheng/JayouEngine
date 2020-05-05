//
// IScene.h
//

#pragma once

#include "../TypeDef.h"
#include "../GeometryManager.h"

#include "IObject.h"
using namespace Utility::GeometryManager;

namespace Core
{
	class IScene
	{
	public:

		virtual void Add(IObject*) = 0;
	};
}