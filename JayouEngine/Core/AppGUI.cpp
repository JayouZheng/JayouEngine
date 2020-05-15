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
	ImGui::MenuItem(u8"�½�", nullptr, &m_bShowMenuNew);

	if (ImGui::MenuItem(u8"��", "Ctrl+O")) 
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
	if (ImGui::BeginMenu(u8"�������..."))
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
	if (ImGui::MenuItem(u8"����", "Ctrl+S")) {}
	if (ImGui::MenuItem(u8"����Ϊ...")) {}

	ImGui::Separator();

	ImGui::MenuItem(u8"ѡ��", nullptr, &m_bShowMenuOption);

	if (ImGui::MenuItem(u8"�˳�", "ESC")) { ExitGame(); }
}

void AppGUI::MenuNew()
{
	if (m_bShowMenuNew)
	{
		if (!ImGui::Begin(u8"�½�", &m_bShowMenuNew, ImGuiWindowFlags_AlwaysAutoResize))
			ImGui::End();
		else
		{
			SetBlockAreas();

			if (ImGui::CollapsingHeader(u8"�½�ģ��", nullptr, ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Text(u8"ѡ��һ���ڽ�����");

				static EBuiltInGeoType builtInModelType = BG_Box;
				const char* builtInModelTypeStrs[] =
				{
					u8"������",
					u8"����_1",
					u8"����_2",
					u8"Բ����",
					u8"ƽ��"
				};
				ImGui::Combo(u8"����##1", (int*)&builtInModelType, builtInModelTypeStrs, IM_ARRAYSIZE(builtInModelTypeStrs));

				switch (builtInModelType)
				{
				case BG_Box:
				{
					static float width = 10.0f;
					static float depth = 10.0f;
					static float height = 10.0f;
					static int32 numSubdivisions = 1;
					static XMFLOAT4 color = XMFLOAT4(Colors::Gray);

					ImGui::DragFloat(u8"����", &width, 0.1f, 0.0f, 1000.0f);
					ImGui::DragFloat(u8"���", &depth, 0.1f, 0.0f, 1000.0f);
					ImGui::DragFloat(u8"�߶�", &height, 0.1f, 0.0f, 1000.0f);
					ImGui::DragInt(u8"ϸ��", &numSubdivisions, 1.0f, 0u, 1000u);
					ImGui::ColorEdit3(u8"��ɫ", (float*)&color);

					ImGui::Text(u8"����ģ�͵ĵ���ѡ��");

					static Vector3  trans = { 0.0f };
					static Vector3  rotat = { 0.0f, 0.0f, 0.0f };
					static Vector3  scale = { 1.0f };
					static bool     lockScale = true;
					static char     userNamed[256] = "Box";

					ImGui::InputText(u8"����", userNamed, IM_ARRAYSIZE(userNamed));
					ImGui::Checkbox(u8"��������", &lockScale);
					ImGui::DragFloat3(u8"λ��", (float*)&trans, 0.1f, -100.0f, 100.0f);
					ImGui::DragFloat3(u8"��ת", (float*)&rotat, 0.1f, -360.0f, 360.0f);
					if (lockScale)
					{
						ImGui::DragFloat(u8"����", (float*)&scale, 0.1f, 0.0f, 100.0f);
						scale.SetY(scale.GetX());
						scale.SetZ(scale.GetX());
					}
					else
					{
						ImGui::DragFloat(u8"����", (float*)&scale, 0.1f, 0.0f, 100.0f);
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

					if (ImGui::Button(u8"�½�"))
						m_gWorld->AddRenderItem(geoDesc);
				}
				break;
				case BG_Sphere_1:
				{
					static float radius = 5.0f;
					static int32 sliceCount = 8;
					static int32 stackCount = 8;
					static XMFLOAT4 color = XMFLOAT4(Colors::Gray);

					ImGui::DragFloat(u8"�뾶", &radius, 0.1f, 0.0f, 1000.0f);
					ImGui::DragInt(u8"Ƭϸ��", &sliceCount, 1.0f, 0u, 1000u);
					ImGui::DragInt(u8"��ϸ��", &stackCount, 1.0f, 0u, 1000u);
					ImGui::ColorEdit3(u8"��ɫ", (float*)&color);

					ImGui::Text(u8"����ģ�͵ĵ���ѡ��");

					static Vector3  trans = { 0.0f };
					static Vector3  rotat = { 0.0f, 0.0f, 0.0f };
					static Vector3  scale = { 1.0f };
					static bool     lockScale = true;
					static char     userNamed[256] = "Sphere_1";

					ImGui::InputText(u8"����", userNamed, IM_ARRAYSIZE(userNamed));
					ImGui::Checkbox(u8"��������", &lockScale);
					ImGui::DragFloat3(u8"λ��", (float*)&trans, 0.1f, -100.0f, 100.0f);
					ImGui::DragFloat3(u8"��ת", (float*)&rotat, 0.1f, -360.0f, 360.0f);
					if (lockScale)
					{
						ImGui::DragFloat(u8"����", (float*)&scale, 0.1f, 0.0f, 100.0f);
						scale.SetY(scale.GetX());
						scale.SetZ(scale.GetX());
					}
					else
					{
						ImGui::DragFloat(u8"����", (float*)&scale, 0.1f, 0.0f, 100.0f);
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

					if (ImGui::Button(u8"�½�"))
						m_gWorld->AddRenderItem(geoDesc);
				}
				break;
				case BG_Sphere_2:
				{
					static float radius = 5.0f;
					static int32 numSubdivisions = 3;
					static XMFLOAT4 color = XMFLOAT4(Colors::Gray);

					ImGui::DragFloat(u8"�뾶", &radius, 0.1f, 0.0f, 1000.0f);
					ImGui::DragInt(u8"ϸ��", &numSubdivisions, 1.0f, 0u, 1000u);
					ImGui::ColorEdit3(u8"��ɫ", (float*)&color);

					ImGui::Text(u8"����ģ�͵ĵ���ѡ��");

					static Vector3  trans = { 0.0f };
					static Vector3  rotat = { 0.0f, 0.0f, 0.0f };
					static Vector3  scale = { 1.0f };
					static bool     lockScale = true;
					static char     userNamed[256] = "Sphere_2";

					ImGui::InputText(u8"����", userNamed, IM_ARRAYSIZE(userNamed));
					ImGui::Checkbox(u8"��������", &lockScale);
					ImGui::DragFloat3(u8"λ��", (float*)&trans, 0.1f, -100.0f, 100.0f);
					ImGui::DragFloat3(u8"��ת", (float*)&rotat, 0.1f, -360.0f, 360.0f);
					if (lockScale)
					{
						ImGui::DragFloat(u8"����", (float*)&scale, 0.1f, 0.0f, 100.0f);
						scale.SetY(scale.GetX());
						scale.SetZ(scale.GetX());
					}
					else
					{
						ImGui::DragFloat(u8"����", (float*)&scale, 0.1f, 0.0f, 100.0f);
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

					if (ImGui::Button(u8"�½�"))
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

					ImGui::DragFloat(u8"���뾶", &topRadius, 0.1f, 0.0f, 1000.0f);
					ImGui::DragFloat(u8"�װ뾶", &bottomRadius, 0.1f, 0.0f, 1000.0f);
					ImGui::DragFloat(u8"�߶�", &height, 0.1f, 0.0f, 1000.0f);
					ImGui::DragInt(u8"Ƭϸ��", &sliceCount, 1.0f, 0u, 1000u);
					ImGui::DragInt(u8"��ϸ��", &stackCount, 1.0f, 0u, 1000u);
					ImGui::ColorEdit3(u8"��ɫ", (float*)&color);

					ImGui::Text(u8"����ģ�͵ĵ���ѡ��");

					static Vector3  trans = { 0.0f };
					static Vector3  rotat = { 0.0f, 0.0f, 0.0f };
					static Vector3  scale = { 1.0f };
					static bool     lockScale = true;
					static char     userNamed[256] = "Cylinder";

					ImGui::InputText(u8"����", userNamed, IM_ARRAYSIZE(userNamed));
					ImGui::Checkbox(u8"��������", &lockScale);
					ImGui::DragFloat3(u8"λ��", (float*)&trans, 0.1f, -100.0f, 100.0f);
					ImGui::DragFloat3(u8"��ת", (float*)&rotat, 0.1f, -360.0f, 360.0f);
					if (lockScale)
					{
						ImGui::DragFloat(u8"����", (float*)&scale, 0.1f, 0.0f, 100.0f);
						scale.SetY(scale.GetX());
						scale.SetZ(scale.GetX());
					}
					else
					{
						ImGui::DragFloat(u8"����", (float*)&scale, 0.1f, 0.0f, 100.0f);
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

					if (ImGui::Button(u8"�½�"))
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

					ImGui::DragFloat(u8"����", &width, 0.1f, 0.0f, 1000.0f);
					ImGui::DragFloat(u8"���", &depth, 0.1f, 0.0f, 1000.0f);
					ImGui::DragInt(u8"Ƭϸ��", &sliceCount, 1.0f, 0u, 1000u);
					ImGui::DragInt(u8"��ϸ��", &stackCount, 1.0f, 0u, 1000u);
					ImGui::ColorEdit3(u8"��ɫ", (float*)&color);

					ImGui::Text(u8"����ģ�͵ĵ���ѡ��");

					static Vector3  trans = { 0.0f };
					static Vector3  rotat = { 0.0f, 0.0f, 0.0f };
					static Vector3  scale = { 1.0f };
					static bool     lockScale = true;
					static char     userNamed[256] = "Plane";

					ImGui::InputText(u8"����", userNamed, IM_ARRAYSIZE(userNamed));
					ImGui::Checkbox(u8"��������", &lockScale);
					ImGui::DragFloat3(u8"λ��", (float*)&trans, 0.1f, -100.0f, 100.0f);
					ImGui::DragFloat3(u8"��ת", (float*)&rotat, 0.1f, -360.0f, 360.0f);
					if (lockScale)
					{
						ImGui::DragFloat(u8"����", (float*)&scale, 0.1f, 0.0f, 100.0f);
						scale.SetY(scale.GetX());
						scale.SetZ(scale.GetX());
					}
					else
					{
						ImGui::DragFloat(u8"����", (float*)&scale, 0.1f, 0.0f, 100.0f);
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

					if (ImGui::Button(u8"�½�"))
						m_gWorld->AddRenderItem(geoDesc);
				}
				break;
				default:
					break;
				}

			}

			if (ImGui::CollapsingHeader(u8"�½��ƹ�", nullptr, ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Text(u8"ѡ��һ���ڽ�����");

				static ELightType builtInLightType = LT_Directional;
				const char* builtInLightTypeStrs[] =
				{
					u8"ƽ�й�",
					u8"���Դ",
					u8"�۹��"
				};
				ImGui::Combo(u8"����##2", (int*)&builtInLightType, builtInLightTypeStrs, IM_ARRAYSIZE(builtInLightTypeStrs));

				static LightDesc litDesc;
				switch (builtInLightType)
				{
				case LT_Directional:
				{				
					litDesc.LightType = LT_Directional;

					static char     userNamed[256] = "DirLight";
					ImGui::InputText(u8"����##2", userNamed, IM_ARRAYSIZE(userNamed));
					litDesc.Name = userNamed;
				}
				break;
				case LT_Point:
				{
					litDesc.LightType = LT_Point;

					static char     userNamed[256] = "PointLight";
					ImGui::InputText(u8"����##2", userNamed, IM_ARRAYSIZE(userNamed));
					litDesc.Name = userNamed;
				}
				break;
				case LT_Spot:
				{
					litDesc.LightType = LT_Spot;

					static char     userNamed[256] = "SpotLight";
					ImGui::InputText(u8"����##2", userNamed, IM_ARRAYSIZE(userNamed));
					litDesc.Name = userNamed;
				}
				break;
				default:
					break;
				}

				ImGui::DragFloat3(u8"λ��##2", (float*)&litDesc.Translation, 0.1f, -100.0f, 100.0f);
				ImGui::DragFloat3(u8"��ת##2", (float*)&litDesc.Rotation, 0.1f, -360.0f, 360.0f);

				ImGui::ColorEdit3(u8"��Դǿ��", (float*)&litDesc.Strength, ImGuiColorEditFlags_HDR);
				ImGui::DragFloat(u8"˥����ʼ", &litDesc.FalloffStart, 0.1f, 0.0f, 100.0f);
				ImGui::DragFloat(u8"˥����ֹ", &litDesc.FalloffEnd, 0.1f, 0.0f, 100.0f);
				ImGui::DragFloat(u8"˥��ָ��", &litDesc.SpotPower, 0.1f, 0.0f, 100.0f);

				if (ImGui::Button(u8"�½�##2"))
					m_gWorld->AddLight(litDesc);
			}

			if (ImGui::CollapsingHeader(u8"�½�����", nullptr, ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Text(u8"�����µĲ���");

				static MaterialDesc matDesc;

				static char     userNamed[256] = "NewMaterial";
				ImGui::InputText(u8"����##3", userNamed, IM_ARRAYSIZE(userNamed));
				matDesc.Name = userNamed;

				ImGui::Text(u8"��������");
				ImGui::ColorEdit4(u8"Diffuse", (float*)&matDesc.DiffuseAlbedo);
				ImGui::DragFloat(u8"Roughness", &matDesc.Roughness, 0.01f, 0.0f, 1.0f);
				ImGui::DragFloat(u8"Metallicity", &matDesc.Metallicity, 0.01f, 0.0f, 1.0f);

				// Set Textures.
				static bool bUsedTexture = false;
				ImGui::Checkbox(u8"ʹ����ͼ", &bUsedTexture);
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

				ImGui::Text(u8"UV�任");
				static bool     lockScale = true;

				ImGui::Checkbox(u8"��������##99", &lockScale);
				ImGui::DragFloat3(u8"λ��##99", (float*)&matDesc.Translation, 0.1f, -100.0f, 100.0f);
				ImGui::DragFloat3(u8"��ת##99", (float*)&matDesc.Rotation, 0.1f, -360.0f, 360.0f);
				if (lockScale)
				{
					ImGui::DragFloat(u8"����##99", (float*)&matDesc.Scale, 0.1f, 0.0f, 100.0f);
					matDesc.Scale.SetY(matDesc.Scale.GetX());
					matDesc.Scale.SetZ(matDesc.Scale.GetX());
				}
				else
				{
					ImGui::DragFloat(u8"����##99", (float*)&matDesc.Scale, 0.1f, 0.0f, 100.0f);
				}

				if (ImGui::Button(u8"�½�##3"))
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
		if (!ImGui::Begin(u8"����", &m_bShowMenuOption, ImGuiWindowFlags_AlwaysAutoResize))
			ImGui::End();
		else
		{
			SetBlockAreas();

			ImGui::Checkbox(u8"��ʾ�߿�", &m_appData->bShowWireframe);
			ImGui::Checkbox(u8"��ʾ����", &m_appData->bShowBackGround);			
			ImGui::Checkbox(u8"��ʾ����", &m_appData->bShowGrid);
			ImGui::SameLine();
			if (ImGui::Button(u8"����"))
				m_appData->ResetGrid();
			if (m_appData->bShowGrid)
			{
				ImGui::SetNextItemWidth(70);
				if (ImGui::DragFloat(u8"����", &m_appData->GridWidth, 1.0f, 20.0f, std::numeric_limits<float>::max()))
					m_appData->bGridDirdy = true;
				ImGui::SameLine();
				ImGui::SetNextItemWidth(70);
				if (ImGui::DragInt(u8"��Ԫ", &m_appData->GridUnit, 20.0f, 20, 2000))
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

				ImGui::LabelText(u8"����", u8"������:  %d", m_gWorld->m_allRItems["grid"]->NumVertices);
				ImGui::LabelText(u8"����", u8"������:  %d", m_gWorld->m_allRItems["grid"]->NumIndices);

				ImGui::Text(u8"����任");
				ImGui::Checkbox(u8"��������", &lockScale);
				ImGui::DragFloat3(u8"λ��", (float*)&trans, 0.01f, -10.0f, 10.0f);
				ImGui::DragFloat3(u8"��ת", (float*)&rotat, 0.01f, -360.0f, 360.0f);
				if (lockScale)
				{
					ImGui::DragFloat(u8"����", (float*)&scale, 0.01f, 0.0f, 10.0f);
					scale.SetY(scale.GetX());
					scale.SetZ(scale.GetX());
				}
				else
				{
					ImGui::DragFloat3(u8"����", (float*)&scale, 0.01f, 0.0f, 10.0f);
				}

				m_gWorld->m_allRItems["grid"]->SetTransFormMatrix(trans, rotat, scale);
				m_gWorld->m_allRItems["grid"]->MarkAsDirty();
			}

			ImGui::Separator();
			ImGui::ColorEdit3(u8"����ɫ", (float*)&m_appData->ClearColor); // Edit 3 floats representing a color	
			ImGui::ColorEdit3(u8"������ǿϵ��", (float*)&m_appData->AmbientFactor);
			ImGui::DragFloat(u8"������Ӱ�뾶", &m_appData->SceneRadius, 1.0f, 10.0f, 1000.0f);
			ImGui::Checkbox(u8"��ʾ�����", &m_appData->bShowSkySphere);
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
					ImGui::Combo(u8"ѡ�񻷾���ͼ", &m_appData->CubeMapIndex, allTexNames.c_str(), (int)allTexNames.size());
				}
			}
			ImGui::DragFloat(u8"��һ�˳��ƶ��ٶ�", &m_appData->WalkSpeed, 1.0f, 10.0f, 1000.0f);

			ImGui::Separator();
			if (ImGui::Checkbox(u8"4 ���������������", &m_appData->bEnable4xMsaa))
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
	if (show_app_style_editor) { ImGui::Begin(u8"�Զ���", &show_app_style_editor); SetBlockAreas(); ImGui::ShowStyleEditor(); ImGui::End(); }
	if (show_app_about) AboutMe(&show_app_about);

	if (!ImGui::Begin(u8"��Ҫ���", nullptr, ImGuiWindowFlags_MenuBar))
		ImGui::End();
	else
	{
		SetBlockAreas();

		MenuNew();
		MenuOption();

		// Menu Bar
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu(u8"�ļ�"))
			{
				MenuFile();
				ImGui::EndMenu();
			}
			
			if (ImGui::BeginMenu(u8"����"))
			{
				ImGui::MenuItem(u8"�Զ���", NULL, &show_app_style_editor);
				ImGui::MenuItem(u8"ʾ������", NULL, &show_app_demo);
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu(u8"����"))
			{
				ImGui::MenuItem(u8"������", NULL, &show_app_about);
				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}

		ImGui::Separator();

		ImGui::Text(u8"���������");

		const char* cameraViewTypes[] =
		{
			u8"��һ�˳�",
			u8"�۽�ԭ��",
			u8"����ͼ",
			u8"����ͼ",
			u8"����ͼ",
			u8"����ͼ",
			u8"ǰ��ͼ",
			u8"����ͼ"
		};
		ImGui::Combo(u8"��ͼ����", (int*)&m_appData->_ECameraViewType, cameraViewTypes, IM_ARRAYSIZE(cameraViewTypes));

		const char* cameraProjTypes[] =
		{
			u8"͸��ͶӰ",
			u8"����ͶӰ"
		};
		ImGui::Combo(u8"ͶӰ����", (int*)&m_appData->_ECameraProjType, cameraProjTypes, IM_ARRAYSIZE(cameraProjTypes));	
		
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
		ImGui::Begin(u8"���˹���", &show_filter_help);
		SetBlockAreas();
		ImGui::Text("Filter usage:\n"
			"  \"\"         display all lines\n"
			"  \"xxx\"      display lines containing \"xxx\"\n"
			"  \"xxx,yyy\"  display lines containing \"xxx\" or \"yyy\"\n"
			"  \"-xxx\"     hide lines containing \"xxx\"");
		ImGui::End();
	}

	if (!ImGui::Begin(u8"������", nullptr, ImGuiWindowFlags_MenuBar))
		ImGui::End();
	else
	{
		SetBlockAreas();

		// Menu Bar
		if (ImGui::BeginMenuBar())
		{			
			if (ImGui::BeginMenu(u8"����"))
			{
				ImGui::MenuItem(u8"���˹���", NULL, &show_filter_help);
				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}

		ImGui::Separator();

		filter.Draw(u8"����");		ImGui::Separator();
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

		if (ImGui::CollapsingHeader(u8"ģ�����ϸ�����", nullptr, ImGuiTreeNodeFlags_DefaultOpen))
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

					ImGui::InputText(u8"����##1", buffer, IM_ARRAYSIZE(buffer));
					ImGui::InputText(u8"·��##1", (char*)ri->PathName.c_str(), ri->PathName.size(), ImGuiInputTextFlags_ReadOnly);

					ImGui::LabelText(u8"����##1", u8"������:  %d", ri->NumVertices);
					ImGui::LabelText(u8"����##2", u8"������:  %d", ri->NumIndices);
							
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
						ImGui::Combo(u8"ѡ�����", &ri->MaterialIndex, allMatNames.c_str(), (int)allMatNames.size());
					}

					if (ri->bIsBuiltIn)
					{
						BuiltInGeoDesc geoDesc = ri->CachedBuiltInGeoDesc;

						ImGui::DragFloat(u8"����", &geoDesc.Width, 0.1f, 0.0f, 1000.0f);
						ImGui::DragFloat(u8"���", &geoDesc.Depth, 0.1f, 0.0f, 1000.0f);
						ImGui::DragFloat(u8"�߶�", &geoDesc.Height, 0.1f, 0.0f, 1000.0f);

						ImGui::DragFloat(u8"�뾶", &geoDesc.Radius, 0.1f, 0.0f, 1000.0f);
						ImGui::DragFloat(u8"���뾶", &geoDesc.TopRadius, 0.1f, 0.0f, 1000.0f);
						ImGui::DragFloat(u8"�װ뾶", &geoDesc.BottomRadius, 0.1f, 0.0f, 1000.0f);

						ImGui::DragInt(u8"ϸ��", &geoDesc.NumSubdivisions, 1.0f, 0u, 1000u);
						ImGui::DragInt(u8"Ƭϸ��", &geoDesc.SliceCount, 1.0f, 0u, 1000u);
						ImGui::DragInt(u8"��ϸ��", &geoDesc.StackCount, 1.0f, 0u, 1000u);

						ImGui::ColorEdit3(u8"��ɫ", (float*)&geoDesc.Color);

						ri->CachedBuiltInGeoDesc = geoDesc;
						ri->VertexColor = geoDesc.Color;						
					}
					else
					{
						ImGui::ColorEdit3(u8"������ɫ", (float*)&color);
						ri->VertexColor = color;
					}
					
					bool hide = !ri->bIsVisible;
					bool lock = !ri->bCanBeSelected;
					ImGui::Checkbox(u8"����##77", &hide);
					ImGui::SameLine();

					ImGui::Checkbox(u8"����##77", &lock);
					ImGui::SameLine();
						
					if (ImGui::Button(u8"�ؽ�##77"))
						ri->bNeedRebuild = true;
					ImGui::SameLine();

					if (ri->bCanBeDeleted == true)
					{
						if (ImGui::Button(u8"ɾ��##77"))
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

		if (ImGui::CollapsingHeader(u8"�ƹ���Դϸ�����", nullptr, ImGuiTreeNodeFlags_DefaultOpen))
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

					ImGui::InputText(u8"����##2", buffer, IM_ARRAYSIZE(buffer));
					ImGui::InputText(u8"·��##2", (char*)lit->PathName.c_str(), lit->PathName.size(), ImGuiInputTextFlags_ReadOnly);

					const char* litTypeName[] =
					{
						u8"ƽ�й�",
						u8"���Դ",
						u8"�۹��"
					};
					ImGui::LabelText(u8"����##88", u8"��Դ����:  %s", litTypeName[lit->LightType]);

					ImGui::ColorEdit3(u8"��Դǿ��", (float*)&lit->Strength, ImGuiColorEditFlags_HDR);
					ImGui::DragFloat(u8"˥����ʼ", &lit->FalloffStart, 0.1f, 0.0f, 100.0f);
					ImGui::DragFloat(u8"˥����ֹ", &lit->FalloffEnd, 0.1f, 0.0f, 100.0f);
					ImGui::DragFloat(u8"˥��ָ��", &lit->SpotPower, 0.1f, 0.0f, 100.0f);

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
					ImGui::Checkbox(u8"����##88", &hide);
					ImGui::SameLine();

					ImGui::Checkbox(u8"����##88", &lock);
					ImGui::SameLine();			

					if (lit->bCanBeDeleted == true)
					{
						if (ImGui::Button(u8"ɾ��##88"))
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

		if (ImGui::CollapsingHeader(u8"������Դϸ�����", nullptr, ImGuiTreeNodeFlags_DefaultOpen))
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

					ImGui::InputText(u8"����##3", buffer, IM_ARRAYSIZE(buffer));
					ImGui::InputText(u8"·��##3", (char*)mat->PathName.c_str(), mat->PathName.size(), ImGuiInputTextFlags_ReadOnly);
					
					ImGui::Text(u8"��������");
					ImGui::ColorEdit4(u8"Diffuse", (float*)&mat->DiffuseAlbedo);
					ImGui::DragFloat(u8"Roughness", &mat->Roughness, 0.01f, 0.0f, 1.0f);
					ImGui::DragFloat(u8"Metallicity", &mat->Metallicity, 0.01f, 0.0f, 1.0f);

					// Set Textures.
					static bool bUsedTexture = false;
					ImGui::Checkbox(u8"ʹ����ͼ", &bUsedTexture);
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

					ImGui::Text(u8"UV�任");
					static bool     lockScale = true;

					ImGui::Checkbox(u8"��������##99", &lockScale);
					ImGui::DragFloat3(u8"λ��##99", (float*)&mat->Translation, 0.1f, -100.0f, 100.0f);
					ImGui::DragFloat3(u8"��ת##99", (float*)&mat->Rotation, 0.1f, -360.0f, 360.0f);
					if (lockScale)
					{
						ImGui::DragFloat(u8"����##99", (float*)&mat->Scale, 0.1f, 0.0f, 100.0f);
						mat->Scale.SetY(mat->Scale.GetX());
						mat->Scale.SetZ(mat->Scale.GetX());
					}
					else
					{
						ImGui::DragFloat(u8"����##99", (float*)&mat->Scale, 0.1f, 0.0f, 100.0f);
					}

					if (mat->bCanBeDeleted == true)
					{
						if (ImGui::Button(u8"ɾ��##99"))
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

		if (ImGui::CollapsingHeader(u8"��ͼ��Դϸ�����", nullptr, ImGuiTreeNodeFlags_DefaultOpen))
		{
			for (auto tex : m_gWorld->m_allTextureRefs)
			{
				if (tex->bSelected)
				{
					ImGui::LabelText("ID##4", "ID:  %d", tex->Index);

					ImGui::InputText(u8"����##4", (char*)tex->Name.c_str(), tex->Name.size(), ImGuiInputTextFlags_ReadOnly);
					ImGui::InputText(u8"·��##4", (char*)tex->PathName.c_str(), tex->PathName.size(), ImGuiInputTextFlags_ReadOnly);

					ImGui::Text(u8"��ͼԤ��");
					// Preview Texture.
					{
						ImGuiIO& io = ImGui::GetIO();
						ImTextureID my_tex_id = (void*)tex->HGPUDescriptor.ptr;

						static float zoom = 4.0f;
						static float region_sz = 32.0f;
						float view_tex_w = ((float)tex->Width / (float)tex->Height) * 128.0f;
						float view_tex_h = 128.0f;

						ImGui::SliderFloat(u8"����##66", &zoom, 1.0f, 16.0f);
						ImGui::SliderFloat(u8"����", &region_sz, 4.0f, Math::Max(view_tex_w, view_tex_h));
					
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

							ImGui::Text(u8"������С����: (%.2f, %.2f)", region_x, region_y);
							ImGui::Text(u8"�����������: (%.2f, %.2f)", region_x + region_sz, region_y + region_sz);

							ImVec2 uv0 = ImVec2((region_x) / view_tex_w, (region_y) / view_tex_h);
							ImVec2 uv1 = ImVec2((region_x + region_sz) / view_tex_w, (region_y + region_sz) / view_tex_h);

							ImGui::Image(my_tex_id, ImVec2(32.0f * zoom, 32.0f * zoom), uv0, uv1);
							ImGui::EndTooltip();
						}
					}

					ImGui::Text(u8"�ֱ���:  %dx%d", tex->Width, tex->Height);

					if (tex->bCanBeDeleted == true)
					{
						if (ImGui::Button(u8"ɾ��##66"))
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
	if (!ImGui::Begin(u8"������", p_open, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::End();
		return;
	}

	SetBlockAreas();

	ImGui::Text("Dear ImGui %s", ImGui::GetVersion());
	ImGui::Text("Jayou Engine Dev.");
	ImGui::Separator();
	ImGui::Text(u8"@ ������: Jayou Zheng");
	ImGui::Text(u8"@ ����: 2020-05-08");
	ImGui::Text(u8"@ �ʼ�: ansys.zheng@foxmail.com");

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
		ImGui::OpenPopup(u8"ģ�͵���ѡ��");
	}

	if (bHasAnyTextures)
	{
		ImGui::OpenPopup(u8"��ͼ����ѡ��");
	}

	if (ImGui::BeginPopupModal(u8"ģ�͵���ѡ��", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		SetBlockAreas(true);

		ImGui::Text(u8"����ģ�͵ĵ���ѡ��");

		ImportGeoDesc   geoDesc;
		
		static Vector3  trans = { 0.0f };
		static Vector3  rotat = { 90.0f, 0.0f, 0.0f };
		static Vector3  scale = { 0.1f };
		static bool     lockScale = true;
		static bool     defaultName = true;
		static char     userNamed[256] = "Unnamed";
		static XMFLOAT4 color = geoDesc.Color;

		ImGui::Checkbox(u8"ʹ��Ĭ���ļ���", &defaultName);
		ImGui::InputText(u8"����", userNamed, IM_ARRAYSIZE(userNamed), defaultName ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_None);
		ImGui::Checkbox(u8"��������", &lockScale);
		ImGui::DragFloat3(u8"λ��", (float*)&trans, 0.1f, -100.0f, 100.0f);
		ImGui::DragFloat3(u8"��ת", (float*)&rotat, 0.1f, -360.0f, 360.0f);		
		if (lockScale)
		{
			ImGui::DragFloat(u8"����", (float*)&scale, 0.1f, 0.0f, 100.0f);
			scale.SetY(scale.GetX());
			scale.SetZ(scale.GetX());
		}
		else
		{
			ImGui::DragFloat(u8"����", (float*)&scale, 0.1f, 0.0f, 100.0f);
		}
		ImGui::ColorEdit3(u8"Ĭ����ɫ", (float*)&color);

		geoDesc.Color = color;
		geoDesc.Translation = trans;
		geoDesc.Rotation = rotat;
		geoDesc.Scale = scale;

		ImGui::Separator();

		if (ImGui::Button(u8"����", ImVec2(60, 0)))
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
		if (ImGui::Button(u8"ȡ��", ImVec2(60, 0)))
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

	if (ImGui::BeginPopupModal(u8"��ͼ����ѡ��", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		SetBlockAreas(true);

		ImGui::Text(u8"������ͼ�ĵ���ѡ��");

		static bool    defaultName = true;
		static char    userNamed[256] = "Unnamed";
		static bool    bIsHDR = false;

		ImGui::Checkbox(u8"ʹ��Ĭ���ļ���", &defaultName);
		ImGui::InputText(u8"����", userNamed, IM_ARRAYSIZE(userNamed), defaultName ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_None);

		ImGui::Checkbox("HDR", &bIsHDR);

		ImGui::Separator();

		if (ImGui::Button(u8"����", ImVec2(60, 0)))
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
		if (ImGui::Button(u8"ȡ��", ImVec2(60, 0)))
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

	ImGui::Text(u8"����任");

	if (IsKeyDown('Q'))
		mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
	if (IsKeyDown('R'))
		mCurrentGizmoOperation = ImGuizmo::ROTATE;
	if (IsKeyDown('E'))
		mCurrentGizmoOperation = ImGuizmo::SCALE;

	if (ImGui::RadioButton((std::string(u8"λ��##") + std::to_string(id)).c_str(), mCurrentGizmoOperation == ImGuizmo::TRANSLATE))
		mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
	ImGui::SameLine();
	if (ImGui::RadioButton((std::string(u8"��ת##") + std::to_string(id)).c_str(), mCurrentGizmoOperation == ImGuizmo::ROTATE))
		mCurrentGizmoOperation = ImGuizmo::ROTATE;
	ImGui::SameLine();
	if (ImGui::RadioButton((std::string(u8"����##") + std::to_string(id)).c_str(), mCurrentGizmoOperation == ImGuizmo::SCALE))
		mCurrentGizmoOperation = ImGuizmo::SCALE;
	
	float matrixTranslation[3], matrixRotation[3], matrixScale[3];
	ImGuizmo::DecomposeMatrixToComponents(matrix, matrixTranslation, matrixRotation, matrixScale);

	ImGui::Checkbox((std::string(u8"��������##") + std::to_string(id)).c_str(), &lockScale);
	ImGui::DragFloat3((std::string(u8"λ��##1") + std::to_string(id)).c_str(), matrixTranslation, 0.1f, -100.0f, 100.0f);
	ImGui::DragFloat3((std::string(u8"��ת##1") + std::to_string(id)).c_str(), matrixRotation, 1.0f, -360.0f, 360.0f);
	if (bNoScale)
	{
		matrixScale[0] = 1.0f; matrixScale[1] = 1.0f; matrixScale[2] = 1.0f;
	}
	else
	{
		if (lockScale)
		{
			ImGui::DragFloat((std::string(u8"����##1") + std::to_string(id)).c_str(), &matrixScale[0], 0.01f, 0.0f, 10.0f);
			matrixScale[1] = matrixScale[0];
			matrixScale[2] = matrixScale[0];
		}
		else
		{
			ImGui::DragFloat3((std::string(u8"����##2") + std::to_string(id)).c_str(), matrixScale, 0.01f, 0.0f, 10.0f);
		}
	}	
	ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, matrix);

	if (mCurrentGizmoOperation != ImGuizmo::SCALE)
	{
		if (ImGui::RadioButton((std::string(u8"����##") + std::to_string(id)).c_str(), mCurrentGizmoMode == ImGuizmo::LOCAL))
			mCurrentGizmoMode = ImGuizmo::LOCAL;
		ImGui::SameLine();
		if (ImGui::RadioButton((std::string(u8"����##") + std::to_string(id)).c_str(), mCurrentGizmoMode == ImGuizmo::WORLD))
			mCurrentGizmoMode = ImGuizmo::WORLD;
	}

	if (IsKeyDown('T'))
		useSnap = !useSnap;

	ImGui::Checkbox((std::string(u8"�任�޶���λ /λ��/��ת/����##") + std::to_string(id)).c_str(), &useSnap);
	
	if (useSnap)
	{
		ImGui::DragFloat3((std::string(u8"##1") + std::to_string(id)).c_str(), snap, 0.1f, 0.0f, 100.0f);
	}

	ImGui::Checkbox((std::string(u8"ʹ�ð����任##") + std::to_string(id)).c_str(), &boundSizing);

	if (boundSizing)
	{
		ImGui::PushID(3);
		ImGui::Checkbox((std::string(u8"�任�޶���λ##") + std::to_string(id)).c_str(), &boundSizingSnap);
		
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
