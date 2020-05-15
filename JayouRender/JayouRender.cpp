//
// JayouRender.cpp
//

#define _WINDOWS

#include "Core/JayouEngine.h"
#include "Core/Common/Interface/IScene.h"
#include "Core/Common/Interface/IGeoImporter.h"
#include "Core/Common/SmartPtr.h"

// IO Test.
#include <iostream>

using namespace Core;
using namespace Utility::GeometryManager;

int main(int argc, char* argv[])
{
	CoreModule coreModule;
	SmartPtr<IJayouEngine> engine = coreModule.GetJayouEngine();

	if (engine != nullptr)
	{
		engine->Init(ES_WithEditor);

		// This will be deprecated in the future engine version.
		engine->Run();

		////////////////////////////////////////////////////////
		/* Future Coding.                                     //
		SmartPtr<IScene> scene = engine->GetScene();

		if (scene == nullptr)
		{
			std::cout << "scene is null..." << std::endl;
		}

		SmartPtr<IGeoImporter> importer = engine->GetAssimpImporter();
		ImportGeoDesc geoDesc;
		geoDesc.Name = "FirstGeo";
		geoDesc.PathName = "../assimp-5.0.1/Cerberus_LP.FBX";
		if (importer->Import(geoDesc))
		{
			Geometry geo = importer->GetGeometry("FirstGeo");
			scene->Add(&geo);
			std::cout << "fbx imported success..." << std::endl;
		}
		else
		{
			std::cout << "fbx imported failed, can't find resource..." << std::endl;
		}
		// Future Coding.                                     */
		////////////////////////////////////////////////////////
	}
	else
	{
		std::cout << "engine is null..." << std::endl;
	}

	std::cout << "Jayou Engine Exit..." << std::endl;
	system("pause");
	return 0;
}

#if 0

interface IDeviceNotify
{
	virtual void OnDeviceLost() = 0;
	virtual void OnDeviceRestored() = 0;

	virtual void UpdateGUI() = 0;
	virtual void RenderGUI() = 0;
	virtual void PostProcessing() = 0;
	virtual void OnOptionsChanged() = 0;

	virtual void ParseCommandLine(std::wstring cmdLine) = 0;

	virtual ~IDeviceNotify() {}
};

class D3DDeviceResources
{
public:

	D3DDeviceResources(
		DXGI_FORMAT backBufferFormat,
		DXGI_FORMAT depthBufferFormat,
		UINT backBufferCount,
		D3D_FEATURE_LEVEL minFeatureLevel,
		unsigned int flags) noexcept(false);
	~D3DDeviceResources();

	void CreateDeviceResources();
	void CreateWindowSizeDependentResources();
	void CreateOffscreenRenderTargets(
		UINT numRenderTargets,
		const DXGI_FORMAT* pRenderTargetFormats,
		const D3D12_RESOURCE_STATES* pDefaultStates,
		CD3DX12_CPU_DESCRIPTOR_HANDLE InHDescriptor);
	void CreateRootSignature(
		D3D12_ROOT_SIGNATURE_DESC* pRootSigDesc, 
		ID3D12RootSignature** ppRootSignature);
	void CreateCommonDescriptorHeap(UINT numDescriptors,
		ID3D12DescriptorHeap** ppDescriptorHeap,
		D3D12_DESCRIPTOR_HEAP_TYPE heapType,
		D3D12_DESCRIPTOR_HEAP_FLAGS heapFlags,
		UINT heapNodeMask = 0);
	void CreateTex2DShaderResourceView(
		ID3D12Resource *pResource,
		D3D12_CPU_DESCRIPTOR_HANDLE destDescriptor,
		UINT mipLevels = 1);
	void CreateGraphicsPipelineState(
		D3D12_GRAPHICS_PIPELINE_STATE_DESC* pDesc, 
		ID3D12PipelineState** ppPipelineState);
	void CreateComputePipelineState(
		D3D12_COMPUTE_PIPELINE_STATE_DESC* pDesc, 
		ID3D12PipelineState** ppPipelineState);
	void CreateDefaultBuffer(
		const void* pData, 
		UINT64 byteSize, 
		ID3D12Resource** ppDefaultBuffer, 
		ID3D12Resource** ppUploadBuffer);
	void CreateTexture2D(Texture* InTexture);
	D3D12_GRAPHICS_PIPELINE_STATE_DESC CreateCommonPSO(
		const std::vector<D3D12_INPUT_ELEMENT_DESC>& InInputLayout, 
		ID3D12RootSignature* InRootSig, 
		ID3DBlob* InShaderVS, ID3DBlob* InShaderPS, 
		ID3D12PipelineState** OutPSO);

	template<typename TVertex, typename TIndex>
	void CreateCommonGeometry(
		RenderItem* InRenderItem, 
		const std::vector<TVertex>& vertices, 
		const std::vector<TIndex>& indices);

	void Prepare(D3D12_RESOURCE_STATES beforeState);
	void Clear(const FLOAT colorRGBA[4],FLOAT depth,UINT8 stencil);
	void Present(D3D12_RESOURCE_STATES beforeState);
	
	template<typename TLambda = PFVOID>
	void DrawRenderItem(RenderItem* InRenderItem, const TLambda& lambda = defalut);

	template<typename TLambda>
	void ExecuteCommandLists(const TLambda& lambda);

	void WaitForGpu() noexcept;

	void SetWindow(HWND window, int width, int height, std::wstring cmdLine);
	bool WindowSizeChanged(int width, int height);
	void SetOptions(unsigned int options);
	void OnOptionsChanged();

	void ParseCommandLine(std::wstring cmdLine);
	void HandleDeviceLost();
	void RegisterDeviceNotify(IDeviceNotify* deviceNotify);

	ID3D12Device*               GetD3DDevice();
	IDXGISwapChain3*            GetSwapChain();
	IDXGIFactory4*              GetDXGIFactory();
	D3D_FEATURE_LEVEL           GetDeviceFeatureLevel();
	ID3D12Resource*             GetRenderTarget();
	ID3D12Resource*             GetMsaaRenderTarget();
	ID3D12Resource*             GetDepthStencil();
	ID3D12CommandQueue*         GetCommandQueue();
	ID3D12CommandAllocator*     GetCommandAllocator();
	ID3D12GraphicsCommandList*  GetCommandList();
	DXGI_FORMAT                 GetBackBufferFormat();
	DXGI_FORMAT                 GetDepthBufferFormat();
	D3D12_VIEWPORT              GetScreenViewport();
	D3D12_RECT                  GetScissorRect();
	UINT                        GetCurrentFrameIndex();
	UINT                        GetBackBufferCount();
	DXGI_COLOR_SPACE_TYPE       GetColorSpace();
	UINT						GetDeviceOptions();
	UINT						GetNum4MSQualityLevels();
	UINT			            GetCbvSrvUavDescriptorSize();
	RECT                        GetOutputSize();

	CD3DX12_CPU_DESCRIPTOR_HANDLE GetRenderTargetView() const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetMsaaRenderTargetView() const;

	CD3DX12_CPU_DESCRIPTOR_HANDLE GetActiveRenderTargetView() const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetActiveDepthStencilView() const;

	CD3DX12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView() const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetMsaaDepthStencilView() const;

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetAllStaticSamplers();
	const CD3DX12_STATIC_SAMPLER_DESC GetStaticSampler(const EStaticSampler& type);

	// ...
}

#endif