//
// AppData.h
//

#pragma once

#include "Common/TypeDef.h"
#include "Common/Camera.h"
#include "Math/Math.h"

using namespace DirectX;
using namespace Math;
using namespace Utility;

namespace Core
{
	struct BlockArea
	{
		XMINT2 StartPos;
		XMINT2 BlockSize;
	};

	struct AppData
	{
		// Scene.
		float SceneRadius = 250.0f;
		int32 CubeMapIndex = -1;
		float WalkSpeed = 50.0f;

		// Grid.
		bool bShowGrid = true;
		bool bGridDirdy = false;
		float GridWidth = 60.0f;
		int32 GridUnit = 60;
		XMFLOAT4 GridColorX = XMFLOAT4(Colors::Red);
		XMFLOAT4 GridColorZ = XMFLOAT4(Colors::Lime);
		XMFLOAT4 GridColorCell = XMFLOAT4(Colors::Gray);
		XMFLOAT4 GridColorBlock = XMFLOAT4(Colors::Blue);

		XMFLOAT4 AmbientFactor = { 0.05f, 0.05f, 0.05f, 1.0f };
		
		// Camera.
		bool bCameraFarZDirty = false;
		float CameraFarZ = 2000.0f;
		ECameraViewType _ECameraViewType = CV_FocusPointView;
		ECameraProjType _ECameraProjType = CP_PerspectiveProj;		
			
		// App Data.
		bool bEnable4xMsaa = false;
		bool bShowBackGround = false;
		bool bShowWireframe = false;
		bool bShowSkySphere = false;
		bool bOptionsChanged = false;
		std::wstring AppPath;
		Vector4 ClearColor = { 0.608f, 0.689f, 0.730f, 1.0f };
		std::vector<std::unique_ptr<BlockArea>> BlockAreas;
					
		void ResetGrid()
		{
			GridWidth = 60.0f;
			GridUnit = 60;
			GridColorX = XMFLOAT4(Colors::Lime);
			GridColorZ = XMFLOAT4(Colors::Red);
			GridColorCell = XMFLOAT4(Colors::Gray);
			GridColorBlock = XMFLOAT4(Colors::Blue);

			bGridDirdy = true;
		}
	};	
}
