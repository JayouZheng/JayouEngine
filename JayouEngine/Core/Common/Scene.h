//
// Scene.h
//

#pragma once

#include "Interface/IScene.h"

namespace Core
{
	class Scene_Impl : public IScene
	{
	public:

		Scene_Impl();

		virtual void Add(IObject*) override;
	};
}
