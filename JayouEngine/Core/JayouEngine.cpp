//
// JayouEngine.cpp
//

#include "JayouEngine.h"
#include "Common/Interface/IScene.h"
#include "Common/Interface/IDeviceResources.h"
#include "Common/TypeDef.h"
#include "Common/AssimpImporter.h"

// IO Test.
#include <iostream>

using namespace Core;

class JayouEngine_Impl : public IJayouEngine
{
	virtual void Init() override;
	virtual void Init(const EEngineState&) override;
	virtual void Init(const EngineInitParams&) override;
	virtual IScene* GetScene() override;
	virtual IGeoImporter* GetAssimpImporter() override;
};

void JayouEngine_Impl::Init()
{
	this->Init(EngineInitParams(DT_Default));
}

void JayouEngine_Impl::Init(const EEngineState& InState)
{
	EngineInitParams params;
	params.EngineState = InState;
	this->Init(params);
}

void JayouEngine_Impl::Init(const EngineInitParams& InParams)
{
	std::cout << "Jayou Engine Init..." << std::endl;
}

IScene* JayouEngine_Impl::GetScene()
{
	std::cout << "Jayou Engine GetScene..." << std::endl;
	return nullptr;
}

IGeoImporter* JayouEngine_Impl::GetAssimpImporter()
{
	return new AssimpImporter();
}

#ifdef __cplusplus
extern "C"
{  // only need to export C interface if
			  // used by C++ source code
#endif

	ENGINE_API IJayouEngine* __cdecl GetJayouEngine(void)
	{
		return new JayouEngine_Impl();
	}

#ifdef __cplusplus
}
#endif
