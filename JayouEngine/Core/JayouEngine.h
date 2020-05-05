//
// JayouEngine.h
//

#pragma once

#ifdef ENGINE_EXPORTS
#define ENGINE_API __declspec(dllexport)
#else
#define ENGINE_API __declspec(dllimport)
#endif

namespace Core
{
	class IScene;
	class IGeoImporter;
}
using namespace Core;

struct EngineInitParams;
enum   EEngineState;

// Engine Main Entry.
class IJayouEngine 
{
public:

	virtual void Init() = 0;
	virtual void Init(const EEngineState&) = 0;
	virtual void Init(const EngineInitParams&) = 0;
	virtual IScene* GetScene() = 0;
	virtual IGeoImporter* GetAssimpImporter() = 0;
};


#ifdef __cplusplus
extern "C" 
{  // only need to export C interface if
			  // used by C++ source code
#endif

	ENGINE_API IJayouEngine* __cdecl GetJayouEngine(void);

#ifdef __cplusplus
}
#endif


//////////////////////////////////////////////////
// CoreModule
//////////////////////////////////////////////////
typedef IJayouEngine*(__cdecl *PFJayouEngine)(void);

#ifdef _WINDOWS

#include <windows.h>

// Enable run-time memory check for debug builds.
#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include <queue>
#include <string>

class CoreModule
{
public:

	CoreModule()
	{
		// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
		_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

		m_hinstLib = LoadLibrary(TEXT("JayouEngine.dll"));
	}

	~CoreModule()
	{
		m_bFreeResult = FreeLibrary(m_hinstLib);
	}

	IJayouEngine* GetJayouEngine()
	{
		if (m_hinstLib != NULL)
		{
			PFJayouEngine getEngine = (PFJayouEngine)GetProcAddress(m_hinstLib, "GetJayouEngine");
			if (getEngine != NULL)
			{
				m_bRunTimeLinkSuccess = TRUE;
				return getEngine();
			}
		}
		else
		{
			m_errorString.push("error: JayouEngine.dll is not loaded correctly!");
		}

		return NULL;
	}

protected:

	std::queue<std::string> m_errorString;

	HINSTANCE m_hinstLib;
	BOOL m_bFreeResult = FALSE;
	BOOL m_bRunTimeLinkSuccess = FALSE;
};

#endif // _WINDOWS

