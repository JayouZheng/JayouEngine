//
// IObject.h
//

#pragma once

#include <string>
#include "../../Math/Transform.h"

using namespace Math;

namespace Core
{
	struct IObject
	{
		std::string Name;
		AffineTransform Transform;
	};
}