//
// AppGUI.h
//

#pragma once

#include "AppData.h"
#include "Common/Utility.h"
#include "Common/FileManager.h"

using namespace Core;
using namespace WinUtility::FileManager;

class GWorld;

class AppGUI
{
public:

	AppGUI(GWorld* InGWorld, Window InWindow, ID3D12Device* InDevice, ID3D12GraphicsCommandList* InCmdList, UINT InNumFrameResources);
	~AppGUI();

	bool InitGUI(std::wstring cmdLine);	
	void UpdateGUI();
	void RenderGUI();
	void DestroyGUI();

	//////////////////////////////////
	// Call when resizing.
	void GUIInvalidateDeviceObjects();
	bool GUICreateDeviceObjects();
	//////////////////////////////////

	void SetSrvDescHeap(ID3D12DescriptorHeap* InSrvDescHeap, INT InOffset = 0);

	LRESULT GUIMessageProcessor(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	// Properties
	UINT	 GetDescriptorCount() const { return 1; }
	AppData* GetAppData()		  const { return m_appData.get(); }

private:

	void NewFrame();

	void MenuFile();
	void MenuNew();
	void MenuOption();
	void MainPanel();
	void WorldOutliner();

	void AboutMe(bool* p_open);

	void PopupModal_ProcessingOpenedFiles();

	void SetBlockAreas(bool bFullScreen = false);

	void EditTransform(const float *cameraView, const float *cameraProjection, float* matrix, int id = 0, bool bNoScale = false);

	void ParseCommandLine(std::wstring cmdLine);

	// Note: Put struct member first, structure was padded due to alignment specifier...

	std::unique_ptr<AppData>   m_appData = nullptr;
	GWorld*                    m_gWorld = nullptr;

	Window m_window;
	ID3D12Device*			   m_d3dDevice = nullptr;
	ID3D12GraphicsCommandList* m_d3dCommandList = nullptr;
	ID3D12DescriptorHeap*	   m_d3dSrvDescHeap = nullptr;
	INT                        m_offsetInDescs = 0; // From HeapStart.
	UINT                       m_descIncrementSize; // Runtime Data... m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	
	UINT                       m_numFrameResources; // Must be the same with app DR.

	// Others.
	bool                       m_bShowMenuOption = false;
	bool                       m_bShowMenuNew = false;

	uint32                     m_blockAreaId = 0;
	static const uint32        MAX_BLOCK_AREAS = 100;

	// Cached Open Files.
	std::unordered_map<std::wstring, ESupportFileType> m_importPathMapTypes;
}; 