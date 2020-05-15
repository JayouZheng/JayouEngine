//
// JayouEngine.cpp
//

#include "JayouEngine.h"
#include "Common/Interface/IDeviceResources.h"
#include "Common/TypeDef.h"
#include "Common/AssimpImporter.h"
#include "Common/Scene.h"
#include "AppEntry.h"

// IO Test.
#include <iostream>

using namespace Core;

class JayouEngine_Impl : public IJayouEngine
{
public:

	virtual void Init() override;
	virtual void Init(const EEngineState&) override;
	virtual void Init(const EngineInitParams&) override;
	virtual IScene* GetScene() override;
	virtual IGeoImporter* GetAssimpImporter() override;

	virtual void Run() override;

	~JayouEngine_Impl()
	{
		std::cout << "Jayou Engine Exit!" << std::endl;
	}

protected:

	// This will be deprecated in the future engine version.
	int WinMain();

	std::unique_ptr<AppEntry> m_app;
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
	return new Scene_Impl();
}

IGeoImporter* JayouEngine_Impl::GetAssimpImporter()
{
	return new AssimpImporter();
}

void JayouEngine_Impl::Run()
{
	std::cout << "Jayou Engine Run Simple..." << std::endl;
	this->WinMain();
}

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int JayouEngine_Impl::WinMain()
{
	if (!XMVerifyCPUSupport())
		return 1;

	// NOTE: CoInitializeEx(...) STA MTA...
	Microsoft::WRL::Wrappers::RoInitializeWrapper initialize(RO_INIT_SINGLETHREADED);
	if (FAILED(initialize))
		return 1;

	// Get App Name & App Path.
	wchar_t exe_path[MAX_PATH];
	GetModuleFileName(NULL, exe_path, MAX_PATH);
	std::wstring app_path = std::wstring(exe_path);
	std::wstring app_name;
	std::wstring::size_type found = app_path.find_last_of(L"\\/");
	if (found != std::wstring::npos)
	{
		app_name = app_path.substr(found + 1);
		app_path = app_path.substr(0, found + 1);
	}

	std::vector<std::wstring> delete_dirs =
	{
		L"x64\\Debug\\",
		L"x64\\Release\\"
	};
	for (auto& dir : delete_dirs)
	{
		found = std::wstring::npos;
		found = app_path.rfind(dir);
		if (found != std::wstring::npos)
			app_path.erase(found, dir.size());
	}

	m_app = std::make_unique<AppEntry>(app_path);

	// Register class and create window
	{
		HINSTANCE hInstance = GetModuleHandle(TEXT("JayouEngine.dll"));

		// Register class
		WNDCLASSEXW wcex = {};
		wcex.cbSize = sizeof(WNDCLASSEXW);
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = WndProc;
		wcex.hInstance = hInstance;
		wcex.hIcon = LoadIconW(hInstance, L"IDI_ICON");
		wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
		wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wcex.lpszClassName = L"JayouEngineWindowClass";
		wcex.hIconSm = LoadIconW(wcex.hInstance, L"IDI_ICON");
		if (!RegisterClassExW(&wcex))
			return 1;

		// Create window
		int w, h;
		m_app->GetDefaultSize(w, h);

		RECT rc = { 0, 0, static_cast<LONG>(w), static_cast<LONG>(h) };

		AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

		HWND hwnd = CreateWindowExW(0, L"JayouEngineWindowClass", L"JayouEngine", WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance,
			nullptr);
		// TODO: Change to CreateWindowExW(WS_EX_TOPMOST, L"JayouEngineWindowClass", L"JayouEngine", WS_POPUP,
		// to default to fullscreen.

		if (!hwnd)
			return 1;

		ShowWindow(hwnd, SW_SHOWNORMAL);
		// TODO: Change nCmdShow to SW_SHOWMAXIMIZED to default to fullscreen.

		SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(m_app.get()));

		GetClientRect(hwnd, &rc);

		m_app->Initialize(hwnd, rc.right - rc.left, rc.bottom - rc.top, std::wstring(TEXT("")));
	}

	// Main message loop
	MSG msg = {};
	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			m_app->Tick();
		}
	}

	m_app.reset();

	return (int)msg.wParam;
}

// Windows procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;

	if (hWnd == NULL)
	{
		return 1;
	}

	static bool s_in_sizemove = false;
	static bool s_in_suspend = false;
	static bool s_minimized = false;
	static bool s_fullscreen = false;
	// TODO: Set s_fullscreen to true if defaulting to fullscreen.

	auto app = reinterpret_cast<AppEntry*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
	if (app && app->AppMessageProcessor(hWnd, message, wParam, lParam))
		return true;

	switch (message)
	{
	case WM_PAINT:
		if (s_in_sizemove && app)
		{
			app->Tick();
		}
		else
		{
			hdc = BeginPaint(hWnd, &ps);
			EndPaint(hWnd, &ps);
		}
		break;

	case WM_MOVE:
		if (app)
		{
			app->OnWindowMoved();
		}
		break;

	case WM_SIZE:
		if (wParam == SIZE_MINIMIZED)
		{
			if (!s_minimized)
			{
				s_minimized = true;
				if (!s_in_suspend && app)
					app->OnSuspending();
				s_in_suspend = true;
			}
		}
		else if (s_minimized)
		{
			s_minimized = false;
			if (s_in_suspend && app)
				app->OnResuming();
			s_in_suspend = false;
		}
		else if (!s_in_sizemove && app)
		{
			app->OnWindowSizeChanged(LOWORD(lParam), HIWORD(lParam));
		}
		break;

	case WM_ENTERSIZEMOVE:
		s_in_sizemove = true;
		break;

	case WM_EXITSIZEMOVE:
		s_in_sizemove = false;
		if (app)
		{
			RECT rc;
			GetClientRect(hWnd, &rc);

			app->OnWindowSizeChanged(rc.right - rc.left, rc.bottom - rc.top);
		}
		break;

	case WM_GETMINMAXINFO:
	{
		auto info = reinterpret_cast<MINMAXINFO*>(lParam);
		info->ptMinTrackSize.x = 320;
		info->ptMinTrackSize.y = 200;
	}
	break;

	case WM_ACTIVATEAPP:
		if (app)
		{
			if (wParam)
			{
				app->OnActivated();
			}
			else
			{
				app->OnDeactivated();
			}
		}
		break;

	case WM_POWERBROADCAST:
		switch (wParam)
		{
		case PBT_APMQUERYSUSPEND:
			if (!s_in_suspend && app)
				app->OnSuspending();
			s_in_suspend = true;
			return TRUE;

		case PBT_APMRESUMESUSPEND:
			if (!s_minimized)
			{
				if (s_in_suspend && app)
					app->OnResuming();
				s_in_suspend = false;
			}
			return TRUE;
		}
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_SYSKEYDOWN:
		if (wParam == VK_RETURN && (lParam & 0x60000000) == 0x20000000)
		{
			// Implements the classic ALT+ENTER fullscreen toggle
			if (s_fullscreen)
			{
				SetWindowLongPtr(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
				SetWindowLongPtr(hWnd, GWL_EXSTYLE, 0);

				int width = 800;
				int height = 600;
				if (app)
					app->GetDefaultSize(width, height);

				ShowWindow(hWnd, SW_SHOWNORMAL);

				SetWindowPos(hWnd, HWND_TOP, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
			}
			else
			{
				SetWindowLongPtr(hWnd, GWL_STYLE, 0);
				SetWindowLongPtr(hWnd, GWL_EXSTYLE, WS_EX_TOPMOST);

				SetWindowPos(hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

				ShowWindow(hWnd, SW_SHOWMAXIMIZED);
			}

			s_fullscreen = !s_fullscreen;
		}
		break;

	case WM_MENUCHAR:
		// A menu is active and the user presses a key that does not correspond
		// to any mnemonic or accelerator key. Ignore so we don't produce an error beep.
		return MAKELRESULT(0, MNC_CLOSE);

	case WM_KEYDOWN:
		app->OnKeyDown(wParam);
		switch (wParam)
		{
		case VK_ESCAPE:
			PostQuitMessage(0);
			break;
		default:
			break;
		}
		break;
	case WM_KEYUP:
		app->OnKeyUp(wParam);
		break;

	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		app->OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;

	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		app->OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;

	case WM_MOUSEMOVE:
		app->OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;

	case WM_MOUSEWHEEL:
		// x, y is not the coordinate of the cursor but pointer.
		app->OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam), GET_KEYSTATE_WPARAM(wParam), GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;

	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

// Exit helper
void ExitGame()
{
	PostQuitMessage(0);
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
