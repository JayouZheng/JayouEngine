//
// AppEntry.h
//

#pragma once

#include "Common/D3DDeviceResources.h"
#include "AppGUI.h"
#include "Common/GeometryManager.h"
#include "Common/FrameResource.h"
#include "Common/Camera.h"
#include "Common/TimerManager.h"
#include "Common/AssimpImporter.h"
#include "Common/TextureImporter.h"
#include "Common/ShadowMap.h"
#include "Common/CubeMap.h"

using namespace Core;
using namespace D3DCore;
using namespace Utility;
using namespace Utility::GeometryManager;
using namespace WinUtility::GeometryManager;
using namespace Utility::TimerManager;

class GWorld
{
public:

	// Global.
	std::unique_ptr<D3DDeviceResources>			                           m_deviceResources = nullptr;
	std::vector<std::unique_ptr<FrameResource>>                            m_frameResources;
	FrameResource*                                                         m_currFrameResource = nullptr;
	int                                                                    m_currFrameResourceIndex = 0;

	std::unique_ptr<AppGUI>		                                           m_appGui = nullptr;
	std::unique_ptr<BaseTimer>				                               m_timer = nullptr;
	std::unique_ptr<Camera>                                                m_camera = nullptr;
	std::unique_ptr<Camera>                                                m_dirLightCamera = nullptr;

	std::unique_ptr<AssimpImporter>                                        m_assimpImporter = nullptr;
	std::unique_ptr<TextureImporter>                                       m_textureImporter = nullptr;
	
	// Scene Resources (e.g. Geo, Mat, Tex).
	std::unordered_map<std::string, std::unique_ptr<Texture>>              m_allTextures;
	std::vector<Texture*>                                                  m_allTextureRefs;

	std::unordered_map<std::string, std::unique_ptr<Material>>             m_allMaterials;
	std::vector<Material*>                                                 m_allMaterialRefs;

	std::unordered_map<std::string, std::unique_ptr<Light>>                m_allLights;
	std::vector<Light*>                                                    m_allLightRefs;

	// Render Data.
	std::unordered_map<std::string, std::unique_ptr<RenderItem>>           m_allRItems;
	std::vector<RenderItem*>                                               m_renderItemLayer[RenderLayer::Count];

	// Constant Buffer & Structure Buffer Count.
	UINT                                                                   m_passCount = 1;
	UINT                                                                   m_objectCount = 0;
	UINT                                                                   m_objectArrayCount = 0;
	UINT                                                                   m_materialCount = 0;
	UINT                                                                   m_lightCount = 0;
	UINT                                                                   m_numDirLights = 0;
	UINT                                                                   m_numPointLights = 0;
	UINT                                                                   m_numSpotLights = 0;

	// Properties.
	std::wstring                                                           m_appPath;
	static const uint32                                                    m_maxPreGBuffers = 2; // Depth View. ShowMap.
	static const uint32                                                    m_maxGBuffers = 4;
	static const uint32                                                    m_maxCubeMapSRVs = 64;
	uint32                                                                 m_currentActiveSRVs;
	uint32                                                                 m_maxSupportTex2Ds = 1024;
	int                                                                    m_width = 1280;
	int                                                                    m_height = 720;

	void GWorldCached(std::unique_ptr<Texture>& InTexture);
	void GWorldCached(std::unique_ptr<Material>& InMaterial);
	void GWorldCached(std::unique_ptr<Light>& InLight);
	void GWorldCached(std::unique_ptr<RenderItem>& InRenderItem, const RenderLayer& InRenderLayer, bool bIsSelectable = true);
	void AddRenderItem(const BuiltInGeoDesc& InGeoDesc);
	void AddRenderItem(const ImportGeoDesc& InGeoDesc);
	void AddTexture2D(const ImportTexDesc& InTexDesc);
	void AddMaterial(const MaterialDesc& InMaterialDesc);
	void AddLight(const LightDesc& InLightDesc);

	void GarbageCollection();
	void HandleRebuildRenderItem();
	void HandleRenderItemStateChanged();
	virtual ~GWorld() {}

protected:

	ComPtr<ID3D12DescriptorHeap>                                           m_srvCbvDescHeap = nullptr;

	CD3DX12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHeapStartOffset(uint32 InOffset = 0);
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHeapStartOffset(uint32 InOffset = 0);
};

// A basic game implementation that creates a D3D12 device and
// provides a game loop.
class AppEntry : public IDeviceNotify, public GWorld
{
public:

	/////////////////////////////////
	// C4316: object allocated on the 
	// heap may not be aligned 64...
	void* operator new(size_t InSize)
	{
		return _mm_malloc(InSize, 64);
	}

	void operator delete(void* InPtr)
	{
		_mm_free(InPtr);
	}
	/////////////////////////////////

    AppEntry(std::wstring appPath) noexcept(false);
    ~AppEntry();

    // Initialization and management
	void PreInitialize(HWND window, int width, int height, std::wstring cmdLine);
    void Initialize(HWND window, int width, int height, std::wstring cmdLine);

    // Basic game loop
    void Tick();

    // IDeviceNotify
    virtual void OnDeviceLost() override;
    virtual void OnDeviceRestored() override;

	virtual void UpdateGUI() override;
	virtual void RenderGUI() override;
	virtual void PostProcessing() override;
	virtual void OnOptionsChanged() override;

	virtual void ParseCommandLine(std::wstring cmdLine) override;

    // Messages
    void OnActivated();
    void OnDeactivated();
    void OnSuspending();
    void OnResuming();
    void OnWindowMoved();
    void OnWindowSizeChanged(int width, int height);

	void OnMouseDown(WPARAM btnState, int x, int y);
	void OnMouseUp(WPARAM btnState, int x, int y);
	void OnMouseMove(WPARAM btnState, int x, int y);
	// x, y is not the coordinate of the cursor but pointer.
	void OnMouseWheel(int d, WPARAM btnState, int x, int y);

	void OnKeyDown(WPARAM keyState);
	void OnKeyUp(WPARAM keyState);

	void OnKeyboardInput(const BaseTimer& timer);

	LRESULT AppMessageProcessor(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Properties
    void  GetDefaultSize( int& width, int& height ) const;
	void  GetCurrentSize( int& width, int& height ) const;

	float AspectRatio() const;

private:

    void Update(const BaseTimer& timer);
    
	void ResponseToGUIOptionChanged();

	void UpdateMainPassCB();
	void UpdateShadowPassCB();
	void UpdateMaterialSB();
	void UpdateLightSB();
	void UpdateCamera();
	void UpdatePerObjectCB();

	void Render();
	void DrawRenderItem(std::vector<RenderItem*>& renderItemLayer);
	void DrawFullscreenQuad(ID3D12GraphicsCommandList* commandList);

	void DrawSceneToShadowMap();
	void DrawSceneToCubeMap();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

	void CreateApplicationDependentResources();

	// Build Resources.
	void BuildDescriptorHeaps();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildRenderItems();

	void BuildPSO();

	// GUI Messages
	bool CheckInBlockAreas(int x, int y);
	void Pick(int InPosX, int InPosY);

	//////////////////////////////////////////////////////////////////////////////////////////////////
    // Global. 	
	std::array<DXGI_FORMAT, m_maxGBuffers>                                 m_offscreenRTFormats;

	// PSO.
	std::unordered_map<std::string, ComPtr<ID3D12RootSignature>>           m_rootSIGs;
	std::unordered_map<std::string, ComPtr<ID3DBlob>>                      m_shaderByteCode;
	std::unordered_map<std::string, std::vector<D3D12_INPUT_ELEMENT_DESC>> m_inputLayout;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>>           m_PSOs;

	std::unique_ptr<ShadowMap>                                             m_shadowMap = nullptr;
	// std::unique_ptr<CubeMap>                                               m_cubeMap = nullptr;
	//////////////////////////////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////////////////////
	// Application Options.
	DXGI_FORMAT m_backBufferFormat   = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT m_depthBufferFormat  = DXGI_FORMAT_D24_UNORM_S8_UINT; /// DXGI_FORMAT_D32_FLOAT

	HWND        m_window = nullptr;	
	XMINT2      m_lastMousePos = XMINT2(0, 0);
	bool        m_bMouseDownInBlockArea = false;
	bool	    m_bMousePosInBlockArea = false;

	// About 2 or 3 should be Enough, 
	// Also Named BackBufferCount Or NUM_FRAMES_IN_FLIGHT.
	const UINT		m_numFrameResources  = 3;
	const UINT      m_cubeMapSize = 1024;
	///////////////////////////////////////////////////////////////////////////////////////////
};