//
// IScene.h
//

#pragma once

namespace Core
{
	class IObject;

	class IScene
	{
	public:

		virtual void Add(IObject*) = 0;

		virtual ~IScene() {}
	};
}