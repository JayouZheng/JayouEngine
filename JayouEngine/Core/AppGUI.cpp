//
// AppGUI.h
//

#include "AppGUI.h"
#include "AppEntry.h"
#include "ImGui/imgui.h"
#include "ImGui/impl/imgui_impl_win32.h"
#include "ImGui/impl/imgui_impl_dx12.h"
#include "Common/StringManager.h"
#include "Common/Camera.h"
#include "Common/InputManager.h"
#include "ImGui/ImGuizmo.h"

using namespace Utility;
using namespace Utility::StringManager;
using namespace WinUtility::InputManager;

extern void ExitGame();

AppGUI::AppGUI(GWorld* InGWorld, Window InWindow, ID3D12Device* InDevice, ID3D12GraphicsCommandList* InCmdList, UINT InNumFrameResources)
	: m_gWorld(InGWorld), m_window(InWindow), m_d3dDevice(InDevice), m_d3dCommandList(InCmdList), m_numFrameResources(InNumFrameResources)
{
	// ...
	m_descIncrementSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	m_appData = std::make_unique<AppData>();
}

AppGUI::~AppGUI()
{
	// ...
	m_d3dDevice = nullptr;
	m_d3dCommandList = nullptr;
	m_d3dSrvDescHeap = nullptr;
}

bool AppGUI::InitGUI(std::wstring cmdLine)
{
	ParseCommandLine(cmdLine);

	// Setup Dear ImGui context.
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	
	// Save imgui.ini manually.
	io.IniFilename = NULL;
	ImGui::LoadIniSettingsFromDisk(std::string(StringUtil::WStringToString(m_appData->AppPath) + "AppGui.ini").c_str());

	// Setup Dear ImGui style.
	ImGui::StyleColorsClassic();

	// Setup Platform/Renderer bindings.
	if (!ImGui_ImplWin32_Init(m_window.Handle))
	{
		return false;
	}

	CD3DX12_CPU_DESCRIPTOR_HANDLE srvCpuDescHandle(m_d3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart());	
	CD3DX12_GPU_DESCRIPTOR_HANDLE srvGpuDescHandle(m_d3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart());

	if (m_offsetInDescs != 0)
	{
		srvCpuDescHandle.Offset(m_offsetInDescs, m_descIncrementSize);
		srvGpuDescHandle.Offset(m_offsetInDescs, m_descIncrementSize);
	}	

	if (!ImGui_ImplDX12_Init(m_d3dDevice, m_numFrameResources,
		DXGI_FORMAT_R8G8B8A8_UNORM, m_d3dSrvDescHeap,
		srvCpuDescHandle,
		srvGpuDescHandle))
	{
		return false;
	}

	// Load Fonts.
	ImFont* font = io.Fonts->AddFontFromFileTTF(
		"C:/Windows/Fonts/msyh.ttc",
		20.0f, NULL, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
	IM_ASSERT(font != NULL);

	ImGui::SetColorEditOptions(ImGuiColorEditFlags_Float | ImGuiColorEditFlags_PickerHueWheel);

	return true;
}

void AppGUI::RenderGUI()
{	
	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_d3dCommandList);
}

void AppGUI::DestroyGUI()
{
	// After WaitForGpu.
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::SaveIniSettingsToDisk(std::string(StringUtil::WStringToString(m_appData->AppPath) + "AppGui.ini").c_str());
	ImGui::DestroyContext();
}

void AppGUI::GUIInvalidateDeviceObjects()
{
	return ImGui_ImplDX12_InvalidateDeviceObjects(); // void
}

bool AppGUI::GUICreateDeviceObjects()
{
	return ImGui_ImplDX12_CreateDeviceObjects();
}

void AppGUI::SetSrvDescHeap(ID3D12DescriptorHeap* InSrvDescHeap, INT InOffset)
{
	m_d3dSrvDescHeap = InSrvDescHeap;
	m_offsetInDescs = InOffset;
}

// Win32 message handler
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT AppGUI::GUIMessageProcessor(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
}

void AppGUI::NewFrame()
{
	// Start the Dear ImGui frame
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGuizmo::BeginFrame();
}

void AppGUI::UpdateGUI()
{
	NewFrame();

	m_appData->BlockAreas.resize(MAX_BLOCK_AREAS);
	m_blockAreaId = 0;	
	
	PopupModal_ProcessingOpenedFiles();

	MainPanel();
	WorldOutliner();	
}

void AppGUI::MenuFile()
{
	ImGui::MenuItem(u8"新建", nullptr, &m_bShowMenuNew);

	if (ImGui::MenuItem(u8"打开", "Ctrl+O")) 
	{
		std::vector<std::wstring> paths;
		std::vector<std::wstring> names;
		std::vector<std::wstring> extens;
		
		if (FileUtil::OpenDialogBox(m_window.Handle, paths, FOS_ALLOWMULTISELECT))
		{
			for (auto path : paths)
			{
				std::wstring name;
				std::wstring exten;
				StringUtil::SplitFileNameAndExtFromPathW(path, name, exten);

				names.push_back(name);
				extens.push_back(exten);
			}			
		}

		FileUtil::GetPathMapFileTypeFromPathW(paths, m_importPathMapTypes);
	}
	if (ImGui::BeginMenu(u8"打开最近的..."))
	{
		ImGui::MenuItem("001.scene");
		ImGui::MenuItem("002.scene");
		ImGui::MenuItem("003.scene");
		if (ImGui::BeginMenu("More.."))
		{
			ImGui::MenuItem("2020-05-06.scene");
			ImGui::MenuItem("2020-05-08.scene");		
			ImGui::EndMenu();
		}
		ImGui::EndMenu();
	}
	if (ImGui::MenuItem(u8"保存", "Ctrl+S")) {}
	if (ImGui::MenuItem(u8"保存为...")) {}

	ImGui::Separator();

	ImGui::MenuItem(u8"选项", nullptr, &m_bShowMenuOption);

	if (ImGui::MenuItem(u8"退出", "ESC")) { ExitGame(); }
}

void AppGUI::MenuNew()
{
	if (m_bShowMenuNew)
	{
		if (!ImGui::Begin(u8"新建", &m_bShowMenuNew, ImGuiWindowFlags_AlwaysAutoResize))
			ImGui::End();
		else
		{
			SetBlockAreas();

			if (ImGui::CollapsingHeader(u8"新建模型", nullptr, ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Text(u8"选择一个内建类型");

				static EBuiltInGeoType builtInModelType = BG_Box;
				const char* builtInModelTypeStrs[] =
				{
					u8"正方体",
					u8"球体_1",
					u8"球体_2",
					u8"圆柱体",
					u8"平面"
				};
				ImGui::Combo(u8"类型##1", (int*)&builtInModelType, builtInModelTypeStrs, IM_ARRAYSIZE(builtInModelTypeStrs));

				switch (builtInModelType)
				{
				case BG_Box:
				{
					static float width = 10.0f;
					static float depth = 10.0f;
					static float height = 10.0f;
					static int32 numSubdivisions = 1;
					static XMFLOAT4 color = XMFLOAT4(Colors::Gray);

					ImGui::DragFloat(u8"长度", &width, 0.1f, 0.0f, 1000.0f);
					ImGui::DragFloat(u8"宽度", &depth, 0.1f, 0.0f, 1000.0f);
					ImGui::DragFloat(u8"高度", &height, 0.1f, 0.0f, 1000.0f);
					ImGui::DragInt(u8"细分", &numSubdivisions, 1.0f, 0u, 1000u);
					ImGui::ColorEdit3(u8"颜色", (float*)&color);

					ImGui::Text(u8"设置模型的导入选项");

					static Vector3  trans = { 0.0f };
					static Vector3  rotat = { 0.0f, 0.0f, 0.0f };
					static Vector3  scale = { 1.0f };
					static bool     lockScale = true;
					static char     userNamed[256] = "Box";

					ImGui::InputText(u8"名称", userNamed, IM_ARRAYSIZE(userNamed));
					ImGui::Checkbox(u8"锁定缩放", &lockScale);
					ImGui::DragFloat3(u8"位置", (float*)&trans, 0.1f, -100.0f, 100.0f);
					ImGui::DragFloat3(u8"旋转", (float*)&rotat, 0.1f, -360.0f, 360.0f);
					if (lockScale)
					{
						ImGui::DragFloat(u8"缩放", (float*)&scale, 0.1f, 0.0f, 100.0f);
						scale.SetY(scale.GetX());
						scale.SetZ(scale.GetX());
					}
					else
					{
						ImGui::DragFloat(u8"缩放", (float*)&scale, 0.1f, 0.0f, 100.0f);
					}

					BuiltInGeoDesc geoDesc;
					geoDesc.Name = userNamed;
					geoDesc.Width = width;
					geoDesc.Depth = depth;
					geoDesc.Height = height;
					geoDesc.NumSubdivisions = numSubdivisions;
					geoDesc.Color = color;
					geoDesc.GeoType = builtInModelType;
					geoDesc.Translation = trans;
					geoDesc.Rotation = rotat;
					geoDesc.Scale = scale;

					if (ImGui::Button(u8"新建"))
						m_gWorld->AddRenderItem(geoDesc);
				}
				break;
				case BG_Sphere_1:
				{
					static float radius = 5.0f;
					static int32 sliceCount = 8;
					static int32 stackCount = 8;
					static XMFLOAT4 color = XMFLOAT4(Colors::Gray);

					ImGui::DragFloat(u8"半径", &radius, 0.1f, 0.0f, 1000.0f);
					ImGui::DragInt(u8"片细分", &sliceCount, 1.0f, 0u, 1000u);
					ImGui::DragInt(u8"层细分", &stackCount, 1.0f, 0u, 1000u);
					ImGui::ColorEdit3(u8"颜色", (float*)&color);

					ImGui::Text(u8"设置模型的导入选项");

					static Vector3  trans = { 0.0f };
					static Vector3  rotat = { 0.0f, 0.0f, 0.0f };
					static Vector3  scale = { 1.0f };
					static bool     lockScale = true;
					static char     userNamed[256] = "Sphere_1";

					ImGui::InputText(u8"名称", userNamed, IM_ARRAYSIZE(userNamed));
					ImGui::Checkbox(u8"锁定缩放", &lockScale);
					ImGui::DragFloat3(u8"位置", (float*)&trans, 0.1f, -100.0f, 100.0f);
					ImGui::DragFloat3(u8"旋转", (float*)&rotat, 0.1f, -360.0f, 360.0f);
					if (lockScale)
					{
						ImGui::DragFloat(u8"缩放", (float*)&scale, 0.1f, 0.0f, 100.0f);
						scale.SetY(scale.GetX());
						scale.SetZ(scale.GetX());
					}
					else
					{
						ImGui::DragFloat(u8"缩放", (float*)&scale, 0.1f, 0.0f, 100.0f);
					}

					BuiltInGeoDesc geoDesc;
					geoDesc.Name = userNamed;
					geoDesc.Radius = radius;
					geoDesc.SliceCount = sliceCount;
					geoDesc.StackCount = stackCount;
					geoDesc.Color = color;
					geoDesc.GeoType = builtInModelType;
					geoDesc.Translation = trans;
					geoDesc.Rotation = rotat;
					geoDesc.Scale = scale;

					if (ImGui::Button(u8"新建"))
						m_gWorld->AddRenderItem(geoDesc);
				}
				break;
				case BG_Sphere_2:
				{
					static float radius = 5.0f;
					static int32 numSubdivisions = 3;
					static XMFLOAT4 color = XMFLOAT4(Colors::Gray);

					ImGui::DragFloat(u8"半径", &radius, 0.1f, 0.0f, 1000.0f);
					ImGui::DragInt(u8"细分", &numSubdivisions, 1.0f, 0u, 1000u);
					ImGui::ColorEdit3(u8"颜色", (float*)&color);

					ImGui::Text(u8"设置模型的导入选项");

					static Vector3  trans = { 0.0f };
					static Vector3  rotat = { 0.0f, 0.0f, 0.0f };
					static Vector3  scale = { 1.0f };
					static bool     lockScale = true;
					static char     userNamed[256] = "Sphere_2";

					ImGui::InputText(u8"名称", userNamed, IM_ARRAYSIZE(userNamed));
					ImGui::Checkbox(u8"锁定缩放", &lockScale);
					ImGui::DragFloat3(u8"位置", (float*)&trans, 0.1f, -100.0f, 100.0f);
					ImGui::DragFloat3(u8"旋转", (float*)&rotat, 0.1f, -360.0f, 360.0f);
					if (lockScale)
					{
						ImGui::DragFloat(u8"缩放", (float*)&scale, 0.1f, 0.0f, 100.0f);
						scale.SetY(scale.GetX());
						scale.SetZ(scale.GetX());
					}
					else
					{
						ImGui::DragFloat(u8"缩放", (float*)&scale, 0.1f, 0.0f, 100.0f);
					}

					BuiltInGeoDesc geoDesc;
					geoDesc.Name = userNamed;
					geoDesc.Radius = radius;
					geoDesc.NumSubdivisions = numSubdivisions;
					geoDesc.Color = color;
					geoDesc.GeoType = builtInModelType;
					geoDesc.Translation = trans;
					geoDesc.Rotation = rotat;
					geoDesc.Scale = scale;

					if (ImGui::Button(u8"新建"))
						m_gWorld->AddRenderItem(geoDesc);
				}
				break;
				case BG_Cylinder:
				{
					static float topRadius = 2.5f;
					static float bottomRadius = 2.5f;
					static float height = 10.0f;
					static int32 sliceCount = 10;
					static int32 stackCount = 10;
					static XMFLOAT4 color = XMFLOAT4(Colors::Gray);

					ImGui::DragFloat(u8"顶半径", &topRadius, 0.1f, 0.0f, 1000.0f);
					ImGui::DragFloat(u8"底半径", &bottomRadius, 0.1f, 0.0f, 1000.0f);
					ImGui::DragFloat(u8"高度", &height, 0.1f, 0.0f, 1000.0f);
					ImGui::DragInt(u8"片细分", &sliceCount, 1.0f, 0u, 1000u);
					ImGui::DragInt(u8"层细分", &stackCount, 1.0f, 0u, 1000u);
					ImGui::ColorEdit3(u8"颜色", (float*)&color);

					ImGui::Text(u8"设置模型的导入选项");

					static Vector3  trans = { 0.0f };
					static Vector3  rotat = { 0.0f, 0.0f, 0.0f };
					static Vector3  scale = { 1.0f };
					static bool     lockScale = true;
					static char     userNamed[256] = "Cylinder";

					ImGui::InputText(u8"名称", userNamed, IM_ARRAYSIZE(userNamed));
					ImGui::Checkbox(u8"锁定缩放", &lockScale);
					ImGui::DragFloat3(u8"位置", (float*)&trans, 0.1f, -100.0f, 100.0f);
					ImGui::DragFloat3(u8"旋转", (float*)&rotat, 0.1f, -360.0f, 360.0f);
					if (lockScale)
					{
						ImGui::DragFloat(u8"缩放", (float*)&scale, 0.1f, 0.0f, 100.0f);
						scale.SetY(scale.GetX());
						scale.SetZ(scale.GetX());
					}
					else
					{
						ImGui::DragFloat(u8"缩放", (float*)&scale, 0.1f, 0.0f, 100.0f);
					}

					BuiltInGeoDesc geoDesc;
					geoDesc.Name = userNamed;
					geoDesc.TopRadius = topRadius;
					geoDesc.BottomRadius = bottomRadius;
					geoDesc.Height = height;
					geoDesc.SliceCount = sliceCount;
					geoDesc.StackCount = stackCount;
					geoDesc.Color = color;
					geoDesc.GeoType = builtInModelType;
					geoDesc.Translation = trans;
					geoDesc.Rotation = rotat;
					geoDesc.Scale = scale;

					if (ImGui::Button(u8"新建"))
						m_gWorld->AddRenderItem(geoDesc);
				}
				break;
				case BG_Plane:
				{
					static float width = 10.0f;
					static float depth = 10.0f;
					static int32 sliceCount = 5;
					static int32 stackCount = 5;
					static XMFLOAT4 color = XMFLOAT4(Colors::Gray);

					ImGui::DragFloat(u8"长度", &width, 0.1f, 0.0f, 1000.0f);
					ImGui::DragFloat(u8"宽度", &depth, 0.1f, 0.0f, 1000.0f);
					ImGui::DragInt(u8"片细分", &sliceCount, 1.0f, 0u, 1000u);
					ImGui::DragInt(u8"层细分", &stackCount, 1.0f, 0u, 1000u);
					ImGui::ColorEdit3(u8"颜色", (float*)&color);

					ImGui::Text(u8"设置模型的导入选项");

					static Vector3  trans = { 0.0f };
					static Vector3  rotat = { 0.0f, 0.0f, 0.0f };
					static Vector3  scale = { 1.0f };
					static bool     lockScale = true;
					static char     userNamed[256] = "Plane";

					ImGui::InputText(u8"名称", userNamed, IM_ARRAYSIZE(userNamed));
					ImGui::Checkbox(u8"锁定缩放", &lockScale);
					ImGui::DragFloat3(u8"位置", (float*)&trans, 0.1f, -100.0f, 100.0f);
					ImGui::DragFloat3(u8"旋转", (float*)&rotat, 0.1f, -360.0f, 360.0f);
					if (lockScale)
					{
						ImGui::DragFloat(u8"缩放", (float*)&scale, 0.1f, 0.0f, 100.0f);
						scale.SetY(scale.GetX());
						scale.SetZ(scale.GetX());
					}
					else
					{
						ImGui::DragFloat(u8"缩放", (float*)&scale, 0.1f, 0.0f, 100.0f);
					}

					BuiltInGeoDesc geoDesc;
					geoDesc.Name = userNamed;
					geoDesc.Width = width;
					geoDesc.Depth = depth;
					geoDesc.SliceCount = sliceCount;
					geoDesc.StackCount = stackCount;
					geoDesc.Color = color;
					geoDesc.GeoType = builtInModelType;
					geoDesc.Translation = trans;
					geoDesc.Rotation = rotat;
					geoDesc.Scale = scale;

					if (ImGui::Button(u8"新建"))
						m_gWorld->AddRenderItem(geoDesc);
				}
				break;
				default:
					break;
				}

			}

			if (ImGui::CollapsingHeader(u8"新建灯光", nullptr, ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Text(u8"选择一个内建类型");

				static ELightType builtInLightType = LT_Directional;
				const char* builtInLightTypeStrs[] =
				{
					u8"平行光",
					u8"点光源",
					u8"聚光灯"
				};
				ImGui::Combo(u8"类型##2", (int*)&builtInLightType, builtInLightTypeStrs, IM_ARRAYSIZE(builtInLightTypeStrs));

				static LightDesc litDesc;
				switch (builtInLightType)
				{
				case LT_Directional:
				{				
					litDesc.LightType = LT_Directional;

					static char     userNamed[256] = "DirLight";
					ImGui::InputText(u8"名称##2", userNamed, IM_ARRAYSIZE(userNamed));
					litDesc.Name = userNamed;
				}
				break;
				case LT_Point:
				{
					litDesc.LightType = LT_Point;

					static char     userNamed[256] = "PointLight";
					ImGui::InputText(u8"名称##2", userNamed, IM_ARRAYSIZE(userNamed));
					litDesc.Name = userNamed;
				}
				break;
				case LT_Spot:
				{
					litDesc.LightType = LT_Spot;

					static char     userNamed[256] = "SpotLight";
					ImGui::InputText(u8"名称##2", userNamed, IM_ARRAYSIZE(userNamed));
					litDesc.Name = userNamed;
				}
				break;
				default:
					break;
				}

				ImGui::DragFloat3(u8"位置##2", (float*)&litDesc.Translation, 0.1f, -100.0f, 100.0f);
				ImGui::DragFloat3(u8"旋转##2", (float*)&litDesc.Rotation, 0.1f, -360.0f, 360.0f);

				ImGui::ColorEdit3(u8"光源强度", (float*)&litDesc.Strength, ImGuiColorEditFlags_HDR);
				ImGui::DragFloat(u8"衰减起始", &litDesc.FalloffStart, 0.1f, 0.0f, 100.0f);
				ImGui::DragFloat(u8"衰减终止", &litDesc.FalloffEnd, 0.1f, 0.0f, 100.0f);
				ImGui::DragFloat(u8"衰减指数", &litDesc.SpotPower, 0.1f, 0.0f, 100.0f);

				if (ImGui::Button(u8"新建##2"))
					m_gWorld->AddLight(litDesc);
			}

			if (ImGui::CollapsingHeader(u8"新建材质", nullptr, ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Text(u8"建立新的材质");

				static MaterialDesc matDesc;

				static char     userNamed[256] = "NewMaterial";
				ImGui::InputText(u8"名称##3", userNamed, IM_ARRAYSIZE(userNamed));
				matDesc.Name = userNamed;

				ImGui::Text(u8"材质属性");
				ImGui::ColorEdit4(u8"Diffuse", (float*)&matDesc.DiffuseAlbedo);
				ImGui::DragFloat(u8"Roughness", &matDesc.Roughness, 0.01f, 0.0f, 1.0f);
				ImGui::DragFloat(u8"Metallicity", &matDesc.Metallicity, 0.01f, 0.0f, 1.0f);

				// Set Textures.
				static bool bUsedTexture = false;
				ImGui::Checkbox(u8"使用贴图", &bUsedTexture);
				if (bUsedTexture)
				{
					std::string allTexNames;
					for (auto& tex : m_gWorld->m_allTextureRefs)
					{
						if (tex->Index != -1)
						{
							allTexNames += tex->Name + '\0';
						}
					}
					if (!m_gWorld->m_allTextureRefs.empty())
					{
						ImGui::Combo(u8"Diffuse", &matDesc.DiffuseMapIndex, allTexNames.c_str(), (int)allTexNames.size());
						ImGui::Combo(u8"Normal", &matDesc.NormalMapIndex, allTexNames.c_str(), (int)allTexNames.size());
						ImGui::Combo(u8"ORM", &matDesc.ORMMapIndex, allTexNames.c_str(), (int)allTexNames.size());
					}
				}
				else
				{
					matDesc.DiffuseMapIndex = -1;
					matDesc.NormalMapIndex = -1;
					matDesc.ORMMapIndex = -1;
				}

				ImGui::Text(u8"UV变换");
				static bool     lockScale = true;

				ImGui::Checkbox(u8"锁定缩放##99", &lockScale);
				ImGui::DragFloat3(u8"位置##99", (float*)&matDesc.Translation, 0.1f, -100.0f, 100.0f);
				ImGui::DragFloat3(u8"旋转##99", (float*)&matDesc.Rotation, 0.1f, -360.0f, 360.0f);
				if (lockScale)
				{
					ImGui::DragFloat(u8"缩放##99", (float*)&matDesc.Scale, 0.1f, 0.0f, 100.0f);
					matDesc.Scale.SetY(matDesc.Scale.GetX());
					matDesc.Scale.SetZ(matDesc.Scale.GetX());
				}
				else
				{
					ImGui::DragFloat(u8"缩放##99", (float*)&matDesc.Scale, 0.1f, 0.0f, 100.0f);
				}

				if (ImGui::Button(u8"新建##3"))
					m_gWorld->AddMaterial(matDesc);
			}

			ImGui::End();
		}
	}	
}

void AppGUI::MenuOption()
{
	if (m_bShowMenuOption)
	{
		if (!ImGui::Begin(u8"设置", &m_bShowMenuOption, ImGuiWindowFlags_AlwaysAutoResize))
			ImGui::End();
		else
		{
			SetBlockAreas();

			ImGui::Checkbox(u8"显示线框", &m_appData->bShowWireframe);
			ImGui::Checkbox(u8"显示背景", &m_appData->bShowBackGround);			
			ImGui::Checkbox(u8"显示网格", &m_appData->bShowGrid);
			ImGui::SameLine();
			if (ImGui::Button(u8"重置"))
				m_appData->ResetGrid();
			if (m_appData->bShowGrid)
			{
				ImGui::SetNextItemWidth(70);
				if (ImGui::DragFloat(u8"长度", &m_appData->GridWidth, 1.0f, 20.0f, std::numeric_limits<float>::max()))
					m_appData->bGridDirdy = true;
				ImGui::SameLine();
				ImGui::SetNextItemWidth(70);
				if (ImGui::DragInt(u8"单元", &m_appData->GridUnit, 20.0f, 20, 2000))
					m_appData->bGridDirdy = true;

				if (ImGui::ColorEdit3("GridColorX", (float*)&m_appData->GridColorX))
					m_appData->bGridDirdy = true;
				if (ImGui::ColorEdit3("GridColorZ", (float*)&m_appData->GridColorZ))
					m_appData->bGridDirdy = true;
				if (ImGui::ColorEdit3("GridColorCell", (float*)&m_appData->GridColorCell))
					m_appData->bGridDirdy = true;
				if (ImGui::ColorEdit3("GridColorBlock", (float*)&m_appData->GridColorBlock))
					m_appData->bGridDirdy = true;

				static bool lockScale = true;

				Vector3 trans = m_gWorld->m_allRItems["grid"]->Translation;
				Vector3 rotat = m_gWorld->m_allRItems["grid"]->Rotation;
				Vector3 scale = m_gWorld->m_allRItems["grid"]->Scale;

				ImGui::LabelText(u8"属性", u8"顶点数:  %d", m_gWorld->m_allRItems["grid"]->NumVertices);
				ImGui::LabelText(u8"属性", u8"索引数:  %d", m_gWorld->m_allRItems["grid"]->NumIndices);

				ImGui::Text(u8"坐标变换");
				ImGui::Checkbox(u8"锁定缩放", &lockScale);
				ImGui::DragFloat3(u8"位置", (float*)&trans, 0.01f, -10.0f, 10.0f);
				ImGui::DragFloat3(u8"旋转", (float*)&rotat, 0.01f, -360.0f, 360.0f);
				if (lockScale)
				{
					ImGui::DragFloat(u8"缩放", (float*)&scale, 0.01f, 0.0f, 10.0f);
					scale.SetY(scale.GetX());
					scale.SetZ(scale.GetX());
				}
				else
				{
					ImGui::DragFloat3(u8"缩放", (float*)&scale, 0.01f, 0.0f, 10.0f);
				}

				m_gWorld->m_allRItems["grid"]->SetTransFormMatrix(trans, rotat, scale);
				m_gWorld->m_allRItems["grid"]->MarkAsDirty();
			}

			ImGui::Separator();
			ImGui::ColorEdit3(u8"背景色", (float*)&m_appData->ClearColor); // Edit 3 floats representing a color	
			ImGui::ColorEdit3(u8"环境光强系数", (float*)&m_appData->AmbientFactor);
			ImGui::DragFloat(u8"场景阴影半径", &m_appData->SceneRadius, 1.0f, 10.0f, 1000.0f);
			ImGui::Checkbox(u8"显示天空球", &m_appData->bShowSkySphere);
			if (m_appData->bShowSkySphere)
			{
				std::string allTexNames;
				for (auto& tex : m_gWorld->m_allTextureRefs)
				{
					if (tex->Index != -1)
					{
						allTexNames += tex->Name + '\0';
					}
				}
				if (!m_gWorld->m_allTextureRefs.empty())
				{
					ImGui::Combo(u8"选择环境贴图", &m_appData->CubeMapIndex, allTexNames.c_str(), (int)allTexNames.size());
				}
			}
			ImGui::DragFloat(u8"第一人称移动速度", &m_appData->WalkSpeed, 1.0f, 10.0f, 1000.0f);

			ImGui::Separator();
			if (ImGui::Checkbox(u8"4 倍超级采样抗锯齿", &m_appData->bEnable4xMsaa))
				m_appData->bOptionsChanged = true;

			ImGui::Separator();
			ImGui::End();
		}		
	}	
}

void AppGUI::MainPanel()
{
	static bool show_app_demo = false;
	static bool show_app_style_editor = false;
	static bool show_app_about = false;

	if (show_app_demo) ImGui::ShowDemoWindow(&show_app_demo);
	if (show_app_style_editor) { ImGui::Begin(u8"自定义", &show_app_style_editor); SetBlockAreas(); ImGui::ShowStyleEditor(); ImGui::End(); }
	if (show_app_about) AboutMe(&show_app_about);

	if (!ImGui::Begin(u8"主要面板", nullptr, ImGuiWindowFlags_MenuBar))
		ImGui::End();
	else
	{
		SetBlockAreas();

		MenuNew();
		MenuOption();

		// Menu Bar
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu(u8"文件"))
			{
				MenuFile();
				ImGui::EndMenu();
			}
			
			if (ImGui::BeginMenu(u8"工具"))
			{
				ImGui::MenuItem(u8"自定义", NULL, &show_app_style_editor);
				ImGui::MenuItem(u8"示例窗口", NULL, &show_app_demo);
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu(u8"帮助"))
			{
				ImGui::MenuItem(u8"关于我", NULL, &show_app_about);
				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}

		ImGui::Separator();

		ImGui::Text(u8"摄像机属性");

		const char* cameraViewTypes[] =
		{
			u8"第一人称",
			u8"聚焦原点",
			u8"顶视图",
			u8"底视图",
			u8"左视图",
			u8"右视图",
			u8"前视图",
			u8"后视图"
		};
		ImGui::Combo(u8"视图类型", (int*)&m_appData->_ECameraViewType, cameraViewTypes, IM_ARRAYSIZE(cameraViewTypes));

		const char* cameraProjTypes[] =
		{
			u8"透视投影",
			u8"正交投影"
		};
		ImGui::Combo(u8"投影类型", (int*)&m_appData->_ECameraProjType, cameraProjTypes, IM_ARRAYSIZE(cameraProjTypes));	
		
		ImGui::Separator();
		ImGui::Text(u8"Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::End();
	}
}

void AppGUI::WorldOutliner()
{
	static bool show_filter_help = false;
	static ImGuiTextFilter filter;
	if (show_filter_help)
	{
		ImGui::Begin(u8"过滤规则", &show_filter_help);
		SetBlockAreas();
		ImGui::Text("Filter usage:\n"
			"  \"\"         display all lines\n"
			"  \"xxx\"      display lines containing \"xxx\"\n"
			"  \"xxx,yyy\"  display lines containing \"xxx\" or \"yyy\"\n"
			"  \"-xxx\"     hide lines containing \"xxx\"");
		ImGui::End();
	}

	if (!ImGui::Begin(u8"世界大纲", nullptr, ImGuiWindowFlags_MenuBar))
		ImGui::End();
	else
	{
		SetBlockAreas();

		// Menu Bar
		if (ImGui::BeginMenuBar())
		{			
			if (ImGui::BeginMenu(u8"帮助"))
			{
				ImGui::MenuItem(u8"过滤规则", NULL, &show_filter_help);
				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}

		ImGui::Separator();

		filter.Draw(u8"搜索");		ImGui::Separator();
		if (ImGui::TreeNodeEx("GWorld", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::TreeNodeEx("RenderItems", ImGuiTreeNodeFlags_DefaultOpen))
			{
				std::vector<RenderItem*>& items = m_gWorld->m_renderItemLayer[RenderLayer::Selectable];
				for (uint32 i = 0; i < items.size(); ++i)
				{
					RenderItem* item = items[i];
					if (filter.PassFilter(items[i]->Name.c_str()))
					{
						if (ImGui::Selectable(items[i]->Name.c_str(), items[i]->bSelected))
						{
							// Clear selection when CTRL is not held.
							if (!ImGui::GetIO().KeyCtrl)
								for (auto ri : items)
									ri->bSelected = false;
							items[i]->bSelected ^= 1;
						}

						if (ImGui::IsItemActive() && !ImGui::IsItemHovered())
						{
							int i_next = i + (ImGui::GetMouseDragDelta(0).y < 0.f ? -1 : 1);
							if (i_next >= 0 && i_next < items.size())
							{
								items[i] = items[i_next];
								items[i_next] = item;
								ImGui::ResetMouseDragDelta();
							}
						}
					}
				}
				ImGui::TreePop();
			}

			if (ImGui::TreeNodeEx("Lights", ImGuiTreeNodeFlags_DefaultOpen))
			{
				std::vector<Light*>& lits = m_gWorld->m_allLightRefs;
				for (uint32 i = 0; i < lits.size(); ++i)
				{
					Light* lit = lits[i];
					if (filter.PassFilter(lits[i]->Name.c_str()))
					{
						if (ImGui::Selectable(lits[i]->Name.c_str(), lits[i]->bSelected))
						{
							// Clear selection when CTRL is not held.
							if (!ImGui::GetIO().KeyCtrl)
								for (auto _lit : lits)
								{
									_lit->bSelected = false;
									if (_lit->LightRItem != nullptr)
									{
										_lit->LightRItem->bSelected = false;
									}
								}								
							lits[i]->bSelected ^= 1;
							if (lits[i]->LightRItem != nullptr)
								lits[i]->LightRItem->bSelected ^= 1;
						}

						if (ImGui::IsItemActive() && !ImGui::IsItemHovered())
						{
							int i_next = i + (ImGui::GetMouseDragDelta(0).y < 0.f ? -1 : 1);
							if (i_next >= 0 && i_next < lits.size())
							{
								lits[i] = lits[i_next];
								lits[i_next] = lit;
								ImGui::ResetMouseDragDelta();
							}
						}
					}
				}
				ImGui::TreePop();
			}

			if (ImGui::TreeNodeEx("Materials", ImGuiTreeNodeFlags_DefaultOpen))
			{
				std::vector<Material*>& mats = m_gWorld->m_allMaterialRefs;
				for (uint32 i = 0; i < mats.size(); ++i)
				{
					Material* mat = mats[i];
					if (filter.PassFilter(mats[i]->Name.c_str()))
					{
						if (ImGui::Selectable(mats[i]->Name.c_str(), mats[i]->bSelected))
						{
							// Clear selection when CTRL is not held.
							if (!ImGui::GetIO().KeyCtrl)
								for (auto _mat : mats)
									_mat->bSelected = false;
							mats[i]->bSelected ^= 1;
						}

						if (ImGui::IsItemActive() && !ImGui::IsItemHovered())
						{
							int i_next = i + (ImGui::GetMouseDragDelta(0).y < 0.f ? -1 : 1);
							if (i_next >= 0 && i_next < mats.size())
							{
								mats[i] = mats[i_next];
								mats[i_next] = mat;
								ImGui::ResetMouseDragDelta();
							}
						}
					}
				}
				ImGui::TreePop();
			}

			if (ImGui::TreeNodeEx("Textures", ImGuiTreeNodeFlags_DefaultOpen))
			{
				std::vector<Texture*>& texs = m_gWorld->m_allTextureRefs;
				for (uint32 i = 0; i < texs.size(); ++i)
				{
					Texture* tex = texs[i];
					if (filter.PassFilter(texs[i]->Name.c_str()))
					{
						if (ImGui::Selectable(texs[i]->Name.c_str(), texs[i]->bSelected))
						{
							// Clear selection when CTRL is not held.
							if (!ImGui::GetIO().KeyCtrl)
								for (auto _tex : texs)
									_tex->bSelected = false;
							texs[i]->bSelected ^= 1;
						}

						if (ImGui::IsItemActive() && !ImGui::IsItemHovered())
						{
							int i_next = i + (ImGui::GetMouseDragDelta(0).y < 0.f ? -1 : 1);
							if (i_next >= 0 && i_next < texs.size())
							{
								texs[i] = texs[i_next];
								texs[i_next] = tex;
								ImGui::ResetMouseDragDelta();
							}
						}
					}
				}
				ImGui::TreePop();
			}

			ImGui::TreePop();
		}

		ImGui::Separator();

		if (ImGui::CollapsingHeader(u8"模型物件细节面板", nullptr, ImGuiTreeNodeFlags_DefaultOpen))
		{
			for (auto ri : m_gWorld->m_renderItemLayer[RenderLayer::Selectable])
			{
				if (ri->bSelected)
				{
					for (auto& lit : m_gWorld->m_allLightRefs)
						lit->bSelected = false;

					char buffer[256];
					uint32 bufferLen = (uint32)ri->Name.size();
					ri->Name.copy(buffer, bufferLen);
					buffer[bufferLen] = '\0';

					XMFLOAT4 color = ri->VertexColor;				

					ImGui::LabelText("ID##1", "ID:  %d", ri->Index);

					ImGui::InputText(u8"名称##1", buffer, IM_ARRAYSIZE(buffer));
					ImGui::InputText(u8"路径##1", (char*)ri->PathName.c_str(), ri->PathName.size(), ImGuiInputTextFlags_ReadOnly);

					ImGui::LabelText(u8"属性##1", u8"顶点数:  %d", ri->NumVertices);
					ImGui::LabelText(u8"属性##2", u8"索引数:  %d", ri->NumIndices);
							
					if (ri->bCanBeSelected)
					{
						float mView[16];
						float mProj[16];
						float mWorld[16];
						memcpy(mView, m_gWorld->m_camera->GetView4x4f().m, 16 * sizeof(float));
						memcpy(mProj, m_gWorld->m_camera->GetProj4x4f().m, 16 * sizeof(float));
						memcpy(mWorld, (const float*)&ri->TransFormMatrix, 16 * sizeof(float));

						EditTransform(mView, mProj, mWorld);
						memcpy((float*)&ri->TransFormMatrix, mWorld, 16 * sizeof(float));
					}		
				
					// Set Material.
					std::string allMatNames;					
					for (auto& mat : m_gWorld->m_allMaterialRefs)
					{
						allMatNames += mat->Name + '\0';
					}
					if (!m_gWorld->m_allMaterialRefs.empty())
					{
						ImGui::Combo(u8"选择材质", &ri->MaterialIndex, allMatNames.c_str(), (int)allMatNames.size());
					}

					if (ri->bIsBuiltIn)
					{
						BuiltInGeoDesc geoDesc = ri->CachedBuiltInGeoDesc;

						ImGui::DragFloat(u8"长度", &geoDesc.Width, 0.1f, 0.0f, 1000.0f);
						ImGui::DragFloat(u8"宽度", &geoDesc.Depth, 0.1f, 0.0f, 1000.0f);
						ImGui::DragFloat(u8"高度", &geoDesc.Height, 0.1f, 0.0f, 1000.0f);

						ImGui::DragFloat(u8"半径", &geoDesc.Radius, 0.1f, 0.0f, 1000.0f);
						ImGui::DragFloat(u8"顶半径", &geoDesc.TopRadius, 0.1f, 0.0f, 1000.0f);
						ImGui::DragFloat(u8"底半径", &geoDesc.BottomRadius, 0.1f, 0.0f, 1000.0f);

						ImGui::DragInt(u8"细分", &geoDesc.NumSubdivisions, 1.0f, 0u, 1000u);
						ImGui::DragInt(u8"片细分", &geoDesc.SliceCount, 1.0f, 0u, 1000u);
						ImGui::DragInt(u8"层细分", &geoDesc.StackCount, 1.0f, 0u, 1000u);

						ImGui::ColorEdit3(u8"颜色", (float*)&geoDesc.Color);

						ri->CachedBuiltInGeoDesc = geoDesc;
						ri->VertexColor = geoDesc.Color;						
					}
					else
					{
						ImGui::ColorEdit3(u8"顶点颜色", (float*)&color);
						ri->VertexColor = color;
					}
					
					bool hide = !ri->bIsVisible;
					bool lock = !ri->bCanBeSelected;
					ImGui::Checkbox(u8"隐藏##77", &hide);
					ImGui::SameLine();

					ImGui::Checkbox(u8"锁定##77", &lock);
					ImGui::SameLine();
						
					if (ImGui::Button(u8"重建##77"))
						ri->bNeedRebuild = true;
					ImGui::SameLine();

					if (ri->bCanBeDeleted == true)
					{
						if (ImGui::Button(u8"删除##77"))
							ri->MarkAsDeleted();
					}				

					ri->Name = buffer;				
					ri->bIsVisible = !hide;
					ri->bCanBeSelected = !hide ? !lock : false;
					
					ri->MarkAsDirty();

					break;
				}
			}
		}

		ImGui::Separator();

		if (ImGui::CollapsingHeader(u8"灯光资源细节面板", nullptr, ImGuiTreeNodeFlags_DefaultOpen))
		{
			for (auto& lit : m_gWorld->m_allLightRefs)
			{
				if (lit->bSelected)
				{
					char buffer[256];
					uint32 bufferLen = (uint32)lit->Name.size();
					lit->Name.copy(buffer, bufferLen);
					buffer[bufferLen] = '\0';

					ImGui::LabelText("ID##2", "ID:  %d", lit->Index);

					ImGui::InputText(u8"名称##2", buffer, IM_ARRAYSIZE(buffer));
					ImGui::InputText(u8"路径##2", (char*)lit->PathName.c_str(), lit->PathName.size(), ImGuiInputTextFlags_ReadOnly);

					const char* litTypeName[] =
					{
						u8"平行光",
						u8"点光源",
						u8"聚光灯"
					};
					ImGui::LabelText(u8"类型##88", u8"光源类型:  %s", litTypeName[lit->LightType]);

					ImGui::ColorEdit3(u8"光源强度", (float*)&lit->Strength, ImGuiColorEditFlags_HDR);
					ImGui::DragFloat(u8"衰减起始", &lit->FalloffStart, 0.1f, 0.0f, 100.0f);
					ImGui::DragFloat(u8"衰减终止", &lit->FalloffEnd, 0.1f, 0.0f, 100.0f);
					ImGui::DragFloat(u8"衰减指数", &lit->SpotPower, 0.1f, 0.0f, 100.0f);

					if (lit->bCanBeSelected)
					{
						float mView[16];
						float mProj[16];
						float mWorld[16];
						memcpy(mView, m_gWorld->m_camera->GetView4x4f().m, 16 * sizeof(float));
						memcpy(mProj, m_gWorld->m_camera->GetProj4x4f().m, 16 * sizeof(float));
						memcpy(mWorld, (const float*)&lit->World, 16 * sizeof(float));

						EditTransform(mView, mProj, mWorld, 1, true);
						memcpy((float*)&lit->World, mWorld, 16 * sizeof(float));
					}				
						
					bool hide = !lit->bIsVisible;
					bool lock = !lit->bCanBeSelected;
					ImGui::Checkbox(u8"隐藏##88", &hide);
					ImGui::SameLine();

					ImGui::Checkbox(u8"锁定##88", &lock);
					ImGui::SameLine();			

					if (lit->bCanBeDeleted == true)
					{
						if (ImGui::Button(u8"删除##88"))
							lit->MarkAsDeleted();
					}			

					static Vector3 dirCache = lit->Direction;

					lit->Name = buffer;
					lit->bIsVisible = !hide;
					lit->bCanBeSelected = !hide ? !lock : false;
					lit->Position = XMFLOAT3(AffineTransform(lit->World).GetTranslation());
					lit->Direction = XMFLOAT3(Vector3(lit->World.Get3x3()*dirCache));
					lit->MarkAsDirty();

					break;
				}
			}
		}

		ImGui::Separator();

		if (ImGui::CollapsingHeader(u8"材质资源细节面板", nullptr, ImGuiTreeNodeFlags_DefaultOpen))
		{
			for (auto& mat : m_gWorld->m_allMaterialRefs)
			{
				if (mat->bSelected)
				{
					char buffer[256];
					uint32 bufferLen = (uint32)mat->Name.size();
					mat->Name.copy(buffer, bufferLen);
					buffer[bufferLen] = '\0';

					ImGui::LabelText("ID##3", "ID:  %d", mat->Index);

					ImGui::InputText(u8"名称##3", buffer, IM_ARRAYSIZE(buffer));
					ImGui::InputText(u8"路径##3", (char*)mat->PathName.c_str(), mat->PathName.size(), ImGuiInputTextFlags_ReadOnly);
					
					ImGui::Text(u8"材质属性");
					ImGui::ColorEdit4(u8"Diffuse", (float*)&mat->DiffuseAlbedo);
					ImGui::DragFloat(u8"Roughness", &mat->Roughness, 0.01f, 0.0f, 1.0f);
					ImGui::DragFloat(u8"Metallicity", &mat->Metallicity, 0.01f, 0.0f, 1.0f);

					// Set Textures.
					static bool bUsedTexture = false;
					ImGui::Checkbox(u8"使用贴图", &bUsedTexture);
					if (bUsedTexture)
					{
						std::string allTexNames;
						for (auto& tex : m_gWorld->m_allTextureRefs)
						{
							if (tex->Index != -1)
							{
								allTexNames += tex->Name + '\0';
							}
						}
						if (!m_gWorld->m_allTextureRefs.empty())
						{
							ImGui::Combo(u8"Diffuse", &mat->DiffuseMapIndex, allTexNames.c_str(), (int)allTexNames.size());
							ImGui::Combo(u8"Normal", &mat->NormalMapIndex, allTexNames.c_str(), (int)allTexNames.size());
							ImGui::Combo(u8"ORM", &mat->ORMMapIndex, allTexNames.c_str(), (int)allTexNames.size());
						}
					}
					else
					{
						mat->DiffuseMapIndex = -1;
						mat->NormalMapIndex = -1;
						mat->ORMMapIndex = -1;
					}

					ImGui::Text(u8"UV变换");
					static bool     lockScale = true;

					ImGui::Checkbox(u8"锁定缩放##99", &lockScale);
					ImGui::DragFloat3(u8"位置##99", (float*)&mat->Translation, 0.1f, -100.0f, 100.0f);
					ImGui::DragFloat3(u8"旋转##99", (float*)&mat->Rotation, 0.1f, -360.0f, 360.0f);
					if (lockScale)
					{
						ImGui::DragFloat(u8"缩放##99", (float*)&mat->Scale, 0.1f, 0.0f, 100.0f);
						mat->Scale.SetY(mat->Scale.GetX());
						mat->Scale.SetZ(mat->Scale.GetX());
					}
					else
					{
						ImGui::DragFloat(u8"缩放##99", (float*)&mat->Scale, 0.1f, 0.0f, 100.0f);
					}

					if (mat->bCanBeDeleted == true)
					{
						if (ImGui::Button(u8"删除##99"))
							mat->MarkAsDeleted();
					}				

					mat->SetTransFormMatrix(mat->Translation, mat->Rotation, mat->Scale);
					mat->Name = buffer;
					mat->MarkAsDirty();

					break;
				}
			}
		}

		ImGui::Separator();

		if (ImGui::CollapsingHeader(u8"贴图资源细节面板", nullptr, ImGuiTreeNodeFlags_DefaultOpen))
		{
			for (auto tex : m_gWorld->m_allTextureRefs)
			{
				if (tex->bSelected)
				{
					ImGui::LabelText("ID##4", "ID:  %d", tex->Index);

					ImGui::InputText(u8"名称##4", (char*)tex->Name.c_str(), tex->Name.size(), ImGuiInputTextFlags_ReadOnly);
					ImGui::InputText(u8"路径##4", (char*)tex->PathName.c_str(), tex->PathName.size(), ImGuiInputTextFlags_ReadOnly);

					ImGui::Text(u8"贴图预览");
					// Preview Texture.
					{
						ImGuiIO& io = ImGui::GetIO();
						ImTextureID my_tex_id = (void*)tex->HGPUDescriptor.ptr;

						static float zoom = 4.0f;
						static float region_sz = 32.0f;
						float view_tex_w = ((float)tex->Width / (float)tex->Height) * 128.0f;
						float view_tex_h = 128.0f;

						ImGui::SliderFloat(u8"缩放##66", &zoom, 1.0f, 16.0f);
						ImGui::SliderFloat(u8"区域", &region_sz, 4.0f, Math::Max(view_tex_w, view_tex_h));
					
						ImVec2 pos = ImGui::GetCursorScreenPos();

						ImGui::Image(my_tex_id, ImVec2(view_tex_w, view_tex_h), ImVec2(0, 0), ImVec2(1, 1),
							/* color rgba */ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
							/* border color rgb thickness 0.0f - 1.0f */ImVec4(1.0f, 1.0f, 1.0f, 0.5f));
									
						if (ImGui::IsItemHovered())
						{
							ImGui::BeginTooltip();

							float region_x = io.MousePos.x - pos.x - region_sz * 0.5f;
							if (region_x < 0.0f)
								region_x = 0.0f;
							else if (region_x > view_tex_w - region_sz)
								region_x = view_tex_w - region_sz;
							float region_y = io.MousePos.y - pos.y - region_sz * 0.5f;
							if (region_y < 0.0f)
								region_y = 0.0f;
							else if (region_y > view_tex_h - region_sz)
								region_y = view_tex_h - region_sz;

							ImGui::Text(u8"区域最小坐标: (%.2f, %.2f)", region_x, region_y);
							ImGui::Text(u8"区域最大坐标: (%.2f, %.2f)", region_x + region_sz, region_y + region_sz);

							ImVec2 uv0 = ImVec2((region_x) / view_tex_w, (region_y) / view_tex_h);
							ImVec2 uv1 = ImVec2((region_x + region_sz) / view_tex_w, (region_y + region_sz) / view_tex_h);

							ImGui::Image(my_tex_id, ImVec2(32.0f * zoom, 32.0f * zoom), uv0, uv1);
							ImGui::EndTooltip();
						}
					}

					ImGui::Text(u8"分辨率:  %dx%d", tex->Width, tex->Height);

					if (tex->bCanBeDeleted == true)
					{
						if (ImGui::Button(u8"删除##66"))
							tex->MarkAsDeleted();
					}										
								
					break;
				}
			}
		}

		ImGui::Separator();

		ImGui::End();
	}	
}

void AppGUI::AboutMe(bool* p_open)
{
	if (!ImGui::Begin(u8"关于我", p_open, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::End();
		return;
	}

	SetBlockAreas();

	ImGui::Text("Dear ImGui %s", ImGui::GetVersion());
	ImGui::Text("Jayou Engine Dev.");
	ImGui::Separator();
	ImGui::Text(u8"@ 开发者: Jayou Zheng");
	ImGui::Text(u8"@ 日期: 2020-05-08");
	ImGui::Text(u8"@ 邮件: ansys.zheng@foxmail.com");

	ImGui::End();
}

void AppGUI::PopupModal_ProcessingOpenedFiles()
{
	static bool bHasAnyModels = false;
	static bool bHasAnyTextures = false;

	for (auto pair : m_importPathMapTypes)
	{
		ESupportFileType fileType = pair.second;

		switch (fileType)
		{
		case SF_JayouEngine:
			break;
		case SF_AssimpModel:
		{
			bHasAnyModels = !bHasAnyTextures;
		}
		break;
		case SF_StdImage:
		{
			bHasAnyTextures = !bHasAnyModels;			
		}
		break;
		case SF_Unknown:
			break;
		default:
			break;
		}
	}

	if (bHasAnyModels)
	{
		ImGui::OpenPopup(u8"模型导入选项");
	}

	if (bHasAnyTextures)
	{
		ImGui::OpenPopup(u8"贴图导入选项");
	}

	if (ImGui::BeginPopupModal(u8"模型导入选项", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		SetBlockAreas(true);

		ImGui::Text(u8"设置模型的导入选项");

		ImportGeoDesc   geoDesc;
		
		static Vector3  trans = { 0.0f };
		static Vector3  rotat = { 90.0f, 0.0f, 0.0f };
		static Vector3  scale = { 0.1f };
		static bool     lockScale = true;
		static bool     defaultName = true;
		static char     userNamed[256] = "Unnamed";
		static XMFLOAT4 color = geoDesc.Color;

		ImGui::Checkbox(u8"使用默认文件名", &defaultName);
		ImGui::InputText(u8"名称", userNamed, IM_ARRAYSIZE(userNamed), defaultName ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_None);
		ImGui::Checkbox(u8"锁定缩放", &lockScale);
		ImGui::DragFloat3(u8"位置", (float*)&trans, 0.1f, -100.0f, 100.0f);
		ImGui::DragFloat3(u8"旋转", (float*)&rotat, 0.1f, -360.0f, 360.0f);		
		if (lockScale)
		{
			ImGui::DragFloat(u8"缩放", (float*)&scale, 0.1f, 0.0f, 100.0f);
			scale.SetY(scale.GetX());
			scale.SetZ(scale.GetX());
		}
		else
		{
			ImGui::DragFloat(u8"缩放", (float*)&scale, 0.1f, 0.0f, 100.0f);
		}
		ImGui::ColorEdit3(u8"默认颜色", (float*)&color);

		geoDesc.Color = color;
		geoDesc.Translation = trans;
		geoDesc.Rotation = rotat;
		geoDesc.Scale = scale;

		ImGui::Separator();

		if (ImGui::Button(u8"导入", ImVec2(60, 0)))
		{
			for (auto pair : m_importPathMapTypes)
			{
				std::wstring wpath = pair.first;
				ESupportFileType fileType = pair.second;

				std::wstring wname;
				std::wstring wexten;
				StringUtil::SplitFileNameAndExtFromPathW(wpath, wname, wexten);

				std::string name = StringUtil::WStringToString(wname);
				std::string path = StringUtil::WStringToString(wpath);

				if (fileType == SF_AssimpModel)
				{
					geoDesc.Name = defaultName ? name : userNamed;
					geoDesc.PathName = path;
					m_gWorld->AddRenderItem(geoDesc);
					m_importPathMapTypes.erase(wpath);
					break;
				}				
			}

			ImGui::CloseCurrentPopup();
			bHasAnyModels = false;
		}
		ImGui::SameLine();
		if (ImGui::Button(u8"取消", ImVec2(60, 0)))
		{
			for (auto pair : m_importPathMapTypes)
			{
				std::wstring wpath = pair.first;
				m_importPathMapTypes.erase(wpath);
				break;				
			}

			ImGui::CloseCurrentPopup();
			bHasAnyModels = false;
		}
			
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopupModal(u8"贴图导入选项", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		SetBlockAreas(true);

		ImGui::Text(u8"设置贴图的导入选项");

		static bool    defaultName = true;
		static char    userNamed[256] = "Unnamed";
		static bool    bIsHDR = false;

		ImGui::Checkbox(u8"使用默认文件名", &defaultName);
		ImGui::InputText(u8"名称", userNamed, IM_ARRAYSIZE(userNamed), defaultName ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_None);

		ImGui::Checkbox("HDR", &bIsHDR);

		ImGui::Separator();

		if (ImGui::Button(u8"导入", ImVec2(60, 0)))
		{
			for (auto pair : m_importPathMapTypes)
			{
				std::wstring wpath = pair.first;
				ESupportFileType fileType = pair.second;

				std::wstring wname;
				std::wstring wexten;
				StringUtil::SplitFileNameAndExtFromPathW(wpath, wname, wexten);

				std::string name = StringUtil::WStringToString(wname);
				std::string path = StringUtil::WStringToString(wpath);

				if (fileType == SF_StdImage)
				{
					ImportTexDesc texDesc;
					texDesc.Name = defaultName ? name : userNamed;
					texDesc.PathName = path;
					texDesc.bIsHDR = bIsHDR;
					m_gWorld->AddTexture2D(texDesc);
					m_importPathMapTypes.erase(wpath);
					break;
				}
			}

			ImGui::CloseCurrentPopup();
			bHasAnyTextures = false;
		}
		ImGui::SameLine();
		if (ImGui::Button(u8"取消", ImVec2(60, 0)))
		{
			for (auto pair : m_importPathMapTypes)
			{
				std::wstring wpath = pair.first;
				m_importPathMapTypes.erase(wpath);
				break;				
			}

			ImGui::CloseCurrentPopup();
			bHasAnyTextures = false;
		}

		ImGui::EndPopup();
	}
}

void AppGUI::SetBlockAreas(bool bFullScreen)
{
	if (m_blockAreaId >= m_appData->BlockAreas.size())
		return;

	auto blockArea = std::make_unique<BlockArea>();
	if (bFullScreen)
	{
		blockArea->StartPos = XMINT2(0, 0);
		blockArea->BlockSize = m_window.Size;
	}
	else
	{
		ImVec2 current_pos = ImGui::GetWindowPos();
		ImVec2 current_size = ImGui::GetWindowSize();
	
		blockArea->StartPos = XMINT2((int32)current_pos.x, (int32)current_pos.y);
		blockArea->BlockSize = XMINT2((int32)current_size.x, (int32)current_size.y);		
	}

	m_appData->BlockAreas[m_blockAreaId++] = std::move(blockArea);
}

void AppGUI::EditTransform(const float *cameraView, const float *cameraProjection, float* matrix, int id /*= 0*/, bool bNoScale /*= false*/)
{
	static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
	static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::LOCAL);
	static bool useSnap = false;
	static float snap[3] = { 1.0f, 1.0f, 1.0f };
	static float bounds[] = { -10.0f, -10.0f, -10.0f, 10.0f, 10.0f, 10.0f }; // bounding box size.
	static float boundsSnap[] = { 0.1f, 0.1f, 0.1f };
	static bool boundSizing = false;
	static bool boundSizingSnap = false;

	static bool lockScale = false;

	ImGui::Text(u8"坐标变换");

	if (IsKeyDown('Q'))
		mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
	if (IsKeyDown('R'))
		mCurrentGizmoOperation = ImGuizmo::ROTATE;
	if (IsKeyDown('E'))
		mCurrentGizmoOperation = ImGuizmo::SCALE;

	if (ImGui::RadioButton((std::string(u8"位置##") + std::to_string(id)).c_str(), mCurrentGizmoOperation == ImGuizmo::TRANSLATE))
		mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
	ImGui::SameLine();
	if (ImGui::RadioButton((std::string(u8"旋转##") + std::to_string(id)).c_str(), mCurrentGizmoOperation == ImGuizmo::ROTATE))
		mCurrentGizmoOperation = ImGuizmo::ROTATE;
	ImGui::SameLine();
	if (ImGui::RadioButton((std::string(u8"缩放##") + std::to_string(id)).c_str(), mCurrentGizmoOperation == ImGuizmo::SCALE))
		mCurrentGizmoOperation = ImGuizmo::SCALE;
	
	float matrixTranslation[3], matrixRotation[3], matrixScale[3];
	ImGuizmo::DecomposeMatrixToComponents(matrix, matrixTranslation, matrixRotation, matrixScale);

	ImGui::Checkbox((std::string(u8"锁定缩放##") + std::to_string(id)).c_str(), &lockScale);
	ImGui::DragFloat3((std::string(u8"位置##1") + std::to_string(id)).c_str(), matrixTranslation, 0.1f, -100.0f, 100.0f);
	ImGui::DragFloat3((std::string(u8"旋转##1") + std::to_string(id)).c_str(), matrixRotation, 1.0f, -360.0f, 360.0f);
	if (bNoScale)
	{
		matrixScale[0] = 1.0f; matrixScale[1] = 1.0f; matrixScale[2] = 1.0f;
	}
	else
	{
		if (lockScale)
		{
			ImGui::DragFloat((std::string(u8"缩放##1") + std::to_string(id)).c_str(), &matrixScale[0], 0.01f, 0.0f, 10.0f);
			matrixScale[1] = matrixScale[0];
			matrixScale[2] = matrixScale[0];
		}
		else
		{
			ImGui::DragFloat3((std::string(u8"缩放##2") + std::to_string(id)).c_str(), matrixScale, 0.01f, 0.0f, 10.0f);
		}
	}	
	ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, matrix);

	if (mCurrentGizmoOperation != ImGuizmo::SCALE)
	{
		if (ImGui::RadioButton((std::string(u8"本地##") + std::to_string(id)).c_str(), mCurrentGizmoMode == ImGuizmo::LOCAL))
			mCurrentGizmoMode = ImGuizmo::LOCAL;
		ImGui::SameLine();
		if (ImGui::RadioButton((std::string(u8"世界##") + std::to_string(id)).c_str(), mCurrentGizmoMode == ImGuizmo::WORLD))
			mCurrentGizmoMode = ImGuizmo::WORLD;
	}

	if (IsKeyDown('T'))
		useSnap = !useSnap;

	ImGui::Checkbox((std::string(u8"变换限定单位 /位置/旋转/缩放##") + std::to_string(id)).c_str(), &useSnap);
	
	if (useSnap)
	{
		ImGui::DragFloat3((std::string(u8"##1") + std::to_string(id)).c_str(), snap, 0.1f, 0.0f, 100.0f);
	}

	ImGui::Checkbox((std::string(u8"使用包裹变换##") + std::to_string(id)).c_str(), &boundSizing);

	if (boundSizing)
	{
		ImGui::PushID(3);
		ImGui::Checkbox((std::string(u8"变换限定单位##") + std::to_string(id)).c_str(), &boundSizingSnap);
		
		if (boundSizingSnap)
		{
			ImGui::DragFloat3((std::string(u8"##2") + std::to_string(id)).c_str(), boundsSnap, 0.1f, 0.0f, 100.0f);
		}
		
		ImGui::PopID();
	}

	ImGuiIO& io = ImGui::GetIO();
	ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
	ImGuizmo::Manipulate(cameraView, cameraProjection, mCurrentGizmoOperation, mCurrentGizmoMode, matrix, NULL, useSnap ? snap : NULL, boundSizing ? bounds : NULL, boundSizingSnap ? boundsSnap : NULL);
}

void AppGUI::ParseCommandLine(std::wstring cmdLine)
{
	std::vector<std::wstring> cmds = StringUtil::WGetBetween(cmdLine, L"-dir [", L"]");
	if (!cmds.empty())
	{
		
	}
	cmds.clear();
	cmds = StringUtil::WGetBetween(cmdLine, L"-scale [", L"]");
	if (!cmds.empty())
	{
		
	}
}
