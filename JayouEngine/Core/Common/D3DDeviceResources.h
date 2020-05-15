//
// D3DDeviceResources.h - A wrapper for the Direct3D 12 device and swapchain
// Note: ComPtr operator& == .ReleaseAndGetAddressOf()

#pragma once

#include "Utility.h"
#include "TextureImporter.h"
#include "GeometryManager.h"

using namespace Math;
using namespace Utility;
using namespace WinUtility;
using namespace WinUtility::GeometryManager;

namespace D3DCore
{
    // Provides an interface for an application that owns D3DDeviceResources to do something on DR side.
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

	namespace DefaultClearValue
	{
		const DirectX::XMVECTORF32 ColorRGBA = DirectX::Colors::Black; // ClearRenderTargetView
		const FLOAT Depth = 1.0f;
		const UINT8 Stencil = 0;
	}

	enum EStaticSampler
	{
		SS_PointWrap,
		SS_PointClamp,
		SS_LinearWrap,
		SS_LinearClamp,
		SS_AnisotropicWrap,
		SS_AnisotropicClamp,
		SS_Shadow
	};

    // Controls all the DirectX device resources.
    class D3DDeviceResources
    {
    public:

        static const unsigned int c_AllowTearing			= 0x1;
        static const unsigned int c_EnableHDR				= 0x2;
		static const unsigned int c_Enable4xMsaa			= 0x4;
		static const unsigned int c_EnableOffscreenRender	= 0x8;
		
        D3DDeviceResources(DXGI_FORMAT backBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM,
                        DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT,
                        UINT backBufferCount = 2,
                        D3D_FEATURE_LEVEL minFeatureLevel = D3D_FEATURE_LEVEL_11_0,
                        unsigned int flags = 0) noexcept(false);
        ~D3DDeviceResources();

        void CreateDeviceResources();
        void CreateWindowSizeDependentResources();
		void CreateOffscreenRenderTargets(
			UINT numRenderTargets, 
			const DXGI_FORMAT* pRenderTargetFormats, 
			const D3D12_RESOURCE_STATES* pDefaultStates,
			CD3DX12_CPU_DESCRIPTOR_HANDLE InHDescriptor);
        void SetWindow(HWND window, int width, int height, std::wstring cmdLine);
        bool WindowSizeChanged(int width, int height);
        void HandleDeviceLost();
        void RegisterDeviceNotify(IDeviceNotify* deviceNotify) { m_deviceNotify = deviceNotify; }
        void Prepare(D3D12_RESOURCE_STATES beforeState = D3D12_RESOURCE_STATE_PRESENT);
		void Clear(const FLOAT colorRGBA[4] = DefaultClearValue::ColorRGBA, 
			FLOAT depth = DefaultClearValue::Depth, 
			UINT8 stencil = DefaultClearValue::Stencil);
        void Present(D3D12_RESOURCE_STATES beforeState = D3D12_RESOURCE_STATE_RENDER_TARGET);
        void WaitForGpu() noexcept;

		void UpdateGUI();
		void RenderGUI();
		void PostProcessing();
		void OnOptionsChanged();
		void ParseCommandLine(std::wstring cmdLine);

		template<typename TLambda>
		void ExecuteCommandLists(const TLambda& lambda);

		// Set D3DDeviceResources options.
		void SetOptions(unsigned int options) { m_options = options; OnOptionsChanged(); }
		void SetActiveOffscreenRTIndex(UINT index) { m_activeOffscreenRTIndex = index; }

		////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Some Convenient Helper Creator Functions.
		void CreateRootSignature(D3D12_ROOT_SIGNATURE_DESC* pRootSigDesc, ID3D12RootSignature** ppRootSignature);
		void CreateCommonDescriptorHeap(UINT numDescriptors, 
			ID3D12DescriptorHeap** ppDescriptorHeap,
			D3D12_DESCRIPTOR_HEAP_TYPE heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			D3D12_DESCRIPTOR_HEAP_FLAGS heapFlags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
			UINT heapNodeMask = 0);
		void CreateTex2DShaderResourceView(
			ID3D12Resource *pResource,
			D3D12_CPU_DESCRIPTOR_HANDLE destDescriptor,
			DXGI_FORMAT InFormat = DXGI_FORMAT_UNKNOWN,
			UINT mipLevels = 1);
		void CreateGraphicsPipelineState(D3D12_GRAPHICS_PIPELINE_STATE_DESC* pDesc, ID3D12PipelineState** ppPipelineState);
		void CreateComputePipelineState(D3D12_COMPUTE_PIPELINE_STATE_DESC* pDesc, ID3D12PipelineState** ppPipelineState);
		void CreateDefaultBuffer(const void* pData, UINT64 byteSize, ID3D12Resource** ppDefaultBuffer, ID3D12Resource** ppUploadBuffer);
		void CreateTexture2D(Texture* InTexture);
		D3D12_GRAPHICS_PIPELINE_STATE_DESC CreateCommonPSO(const std::vector<D3D12_INPUT_ELEMENT_DESC>& InInputLayout, ID3D12RootSignature* InRootSig, ID3DBlob* InShaderVS, ID3DBlob* InShaderPS, ID3D12PipelineState** OutPSO);
		template<typename TVertex, typename TIndex>
		// NOTE: NO Section.
		void CreateCommonGeometry(RenderItem* InRenderItem, const std::vector<TVertex>& vertices, const std::vector<TIndex>& indices);
		template<typename TLambda = PFVOID>
		void DrawRenderItem(RenderItem* InRenderItem, const TLambda& lambda = defalut);

		void CreateRtvDescriptorHeaps_AutoUpdate(uint32 InNumRtvs = 0, PFVOID UpdateCallBack = nullptr);
		void CreateDsvDescriptorHeaps_AutoUpdate(uint32 InNumDsvs = 0, PFVOID UpdateCallBack = nullptr);
		////////////////////////////////////////////////////////////////////////////////////////////////////////

        // Device Accessors.
        RECT GetOutputSize() const { return m_outputSize; }

        // Direct3D Accessors.
        ID3D12Device*               GetD3DDevice() const            { return m_d3dDevice.Get(); }
        IDXGISwapChain3*            GetSwapChain() const            { return m_swapChain.Get(); }
        IDXGIFactory4*              GetDXGIFactory() const          { return m_dxgiFactory.Get(); }
        D3D_FEATURE_LEVEL           GetDeviceFeatureLevel() const   { return m_d3dFeatureLevel; }
        ID3D12Resource*             GetRenderTarget() const         { return m_renderTargets[m_backBufferIndex].Get(); }
		ID3D12Resource*             GetMsaaRenderTarget() const		{ return m_msaaRenderTarget.Get(); }
        ID3D12Resource*             GetDepthStencil() const         { return m_depthStencil.Get(); }
        ID3D12CommandQueue*         GetCommandQueue() const         { return m_commandQueue.Get(); }
        ID3D12CommandAllocator*     GetCommandAllocator() const     { return m_commandAllocators[m_backBufferIndex].Get(); }
        ID3D12GraphicsCommandList*  GetCommandList() const          { return m_commandList.Get(); }
        DXGI_FORMAT                 GetBackBufferFormat() const     { return m_backBufferFormat; }
        DXGI_FORMAT                 GetDepthBufferFormat() const    { return m_depthBufferFormat; }
        D3D12_VIEWPORT              GetScreenViewport() const       { return m_screenViewport; }
        D3D12_RECT                  GetScissorRect() const          { return m_scissorRect; }
        UINT                        GetCurrentFrameIndex() const    { return m_backBufferIndex; }
        UINT                        GetBackBufferCount() const      { return m_backBufferCount; }
        DXGI_COLOR_SPACE_TYPE       GetColorSpace() const           { return m_colorSpace; }
        UINT						GetDeviceOptions() const        { return m_options; }
		UINT						GetNum4MSQualityLevels() const  { return m_num4MSQualityLevels; }

		UINT			GetCbvSrvUavDescriptorSize()		 const	{ return m_cbvSrvUavDescriptorSize; }
		ID3D12Resource* GetOffscreenRenderTarget(UINT index) const	{ return m_offscreenRenderTargets[Math::Clamp(index, 0u, MAX_OFFSCREEN_RTV_COUNT - 1u)].Get(); }

		// Texture Samplers.
		const CD3DX12_STATIC_SAMPLER_DESC GetStaticSampler(const EStaticSampler& type);
		std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetAllStaticSamplers();

        CD3DX12_CPU_DESCRIPTOR_HANDLE GetRenderTargetView() const
        {
            return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), 
				m_backBufferIndex, m_rtvDescriptorSize);
        }
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetMsaaRenderTargetView() const
		{
			return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), 
				m_backBufferCount, m_rtvDescriptorSize);
		}
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetOffscreenRenderTargetView(UINT index) const
		{
			return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
				m_backBufferCount + Math::Clamp(index, 0u, MAX_OFFSCREEN_RTV_COUNT - 1u) + 1, m_rtvDescriptorSize); // +1 Msaa RTV.
		}
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetCubeMapRenderTargetView() const
		{
			return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
				m_backBufferCount + MAX_OFFSCREEN_RTV_COUNT + 1, m_rtvDescriptorSize);
		}
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetActiveRenderTargetView() const
		{
			return (m_options & c_Enable4xMsaa) ? GetMsaaRenderTargetView() :
				(m_options & c_EnableOffscreenRender) ? GetOffscreenRenderTargetView(m_activeOffscreenRTIndex) :
				GetRenderTargetView();
		}
        CD3DX12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView() const
        {
            return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
        }
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetMsaaDepthStencilView() const
		{
			return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), 
				1, m_dsvDescriptorSize);
		}
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetNewDepthStencilView() const
		{
			return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
				2, m_dsvDescriptorSize);
		}
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetDepthStencilViewOffset(uint32 InOffset = 0) const
		{
			return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
				3 + InOffset, m_dsvDescriptorSize);
		}
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetActiveDepthStencilView() const
		{
			return (m_options & c_Enable4xMsaa) ? GetMsaaDepthStencilView() : GetDepthStencilView();
		}

    private:

		void CreateRenderTargetView();
		void CreateDepthStencilView();

        void MoveToNextFrame();
        void GetAdapter(IDXGIAdapter1** ppAdapter);
        void UpdateColorSpace();

        static const size_t			MAX_BACK_BUFFER_COUNT = 3;
		static const unsigned int	MAX_OFFSCREEN_RTV_COUNT = 8;

        UINT                                                m_backBufferIndex;

        // Direct3D objects.
        Microsoft::WRL::ComPtr<ID3D12Device>                m_d3dDevice;
        Microsoft::WRL::ComPtr<ID3D12CommandQueue>          m_commandQueue;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>   m_commandList;
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator>      m_commandAllocators[MAX_BACK_BUFFER_COUNT];

        // Swap chain objects.
        Microsoft::WRL::ComPtr<IDXGIFactory4>               m_dxgiFactory;
        Microsoft::WRL::ComPtr<IDXGISwapChain3>             m_swapChain;
        Microsoft::WRL::ComPtr<ID3D12Resource>              m_renderTargets[MAX_BACK_BUFFER_COUNT];
        Microsoft::WRL::ComPtr<ID3D12Resource>              m_depthStencil;
		Microsoft::WRL::ComPtr<ID3D12Resource>              m_newDepthStencil;

		// MSAA resources.
		Microsoft::WRL::ComPtr<ID3D12Resource>				m_msaaRenderTarget;
		Microsoft::WRL::ComPtr<ID3D12Resource>				m_msaaDepthStencil;

		// Offscreen Render Targets.
		Microsoft::WRL::ComPtr<ID3D12Resource>				m_offscreenRenderTargets[MAX_OFFSCREEN_RTV_COUNT];

        // Presentation fence objects.
        Microsoft::WRL::ComPtr<ID3D12Fence>                 m_fence;
        UINT64                                              m_fenceValues[MAX_BACK_BUFFER_COUNT];
        Microsoft::WRL::Wrappers::Event                     m_fenceEvent;

        // Direct3D rendering objects.
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>        m_rtvDescriptorHeap;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>        m_dsvDescriptorHeap;
        UINT                                                m_rtvDescriptorSize;
		UINT                                                m_dsvDescriptorSize;
		UINT                                                m_cbvSrvUavDescriptorSize;
        D3D12_VIEWPORT                                      m_screenViewport;
        D3D12_RECT                                          m_scissorRect;

        // Direct3D properties.
        DXGI_FORMAT                                         m_backBufferFormat;
        DXGI_FORMAT                                         m_depthBufferFormat;
        UINT                                                m_backBufferCount;
        D3D_FEATURE_LEVEL                                   m_d3dMinFeatureLevel;

        // Cached device properties.
        HWND                                                m_window;
        D3D_FEATURE_LEVEL                                   m_d3dFeatureLevel;
        DWORD                                               m_dxgiFactoryFlags;
        RECT                                                m_outputSize;
		UINT												m_num4MSQualityLevels = 0;      // quality level of 4X MSAA

        // HDR Support.
        DXGI_COLOR_SPACE_TYPE                               m_colorSpace;

        // D3DDeviceResources options (see flags above).
        unsigned int                                        m_options;
		UINT												m_numRTVDescriptors;
		UINT												m_numDSVDescriptors;

        // The IDeviceNotify can be held directly as it owns the D3DDeviceResources.
        IDeviceNotify*                                      m_deviceNotify;

		// Others.
		unsigned int										m_sampleCount;
		unsigned int										m_targetSampleCount = 4;
		UINT												m_activeOffscreenRTIndex = 0;
		bool												m_useWarpDevice = false;

	private:

		static void defalut() {}
    };

	template<typename TLambda>
	void D3DDeviceResources::ExecuteCommandLists(const TLambda& lambda)
	{
		// Reset the command list to prep for initialization commands.
		ThrowIfFailedV1(m_commandList->Reset(m_commandAllocators[m_backBufferIndex].Get(), nullptr));

		// ExecuteCommandLists.
		lambda();

		// Execute the initialization commands.
		ThrowIfFailedV1(m_commandList->Close());
		ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
		m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

		// Wait until all previous GPU work is complete.
		WaitForGpu();
	}

	// NOTE: NO Section.
	template<typename TVertex, typename TIndex>
	void D3DDeviceResources::CreateCommonGeometry(RenderItem* InRenderItem, const std::vector<TVertex>& vertices, const std::vector<TIndex>& indices)
	{
		const UINT vbByteSize = (UINT)vertices.size() * sizeof(TVertex);
		const UINT ibByteSize = (UINT)indices.size() * sizeof(TIndex);

		InRenderItem->RenderData = std::make_unique<D3DRenderData>();

		ThrowIfFailedV1(D3DCreateBlob(vbByteSize, &InRenderItem->RenderData->VertexBufferCPU));
		CopyMemory(InRenderItem->RenderData->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

		ThrowIfFailedV1(D3DCreateBlob(ibByteSize, &InRenderItem->RenderData->IndexBufferCPU));
		CopyMemory(InRenderItem->RenderData->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

		CreateDefaultBuffer(vertices.data(), vbByteSize,
			&InRenderItem->RenderData->VertexBufferGPU, &InRenderItem->RenderData->VertexBufferUploader);
		CreateDefaultBuffer(indices.data(), ibByteSize,
			&InRenderItem->RenderData->IndexBufferGPU, &InRenderItem->RenderData->IndexBufferUploader);

		// Vertex Buffer View Data.
		InRenderItem->RenderData->VertexByteStride = sizeof(TVertex);
		InRenderItem->RenderData->VertexBufferByteSize = vbByteSize;

		if (std::is_same<TVertex, Vertex>::value)
			InRenderItem->RenderData->VertexFormat = VF_Vertex;
		else if ((std::is_same<TVertex, ColorVertex>::value))
			InRenderItem->RenderData->VertexFormat = VF_ColorVertex;

		// Index Buffer View Data.
		if (std::is_same<TIndex, uint16>::value)
			InRenderItem->RenderData->IndexFormat = DXGI_FORMAT_R16_UINT;
		else if ((std::is_same<TIndex, uint32>::value))
			InRenderItem->RenderData->IndexFormat = DXGI_FORMAT_R32_UINT;
		InRenderItem->RenderData->IndexBufferByteSize = ibByteSize;

		Section section;
		section.IndexCountPerInstance = (UINT)indices.size();
		section.InstanceCount = 1;
		section.StartIndexLocation = 0;
		section.BaseVertexLocation = 0;
		section.StartInstanceLocation = 0;
		section.CalcBounds(vertices, indices);

		InRenderItem->RenderData->Sections[InRenderItem->Name] = section;
	}

	template<typename TLambda /*= PFVOID*/>
	void D3DDeviceResources::DrawRenderItem(RenderItem* InRenderItem, const TLambda& lambda /*= defalut*/)
	{
		auto commandList = GetCommandList();

		commandList->IASetVertexBuffers(0, 1, &InRenderItem->RenderData->VertexBufferView());
		commandList->IASetIndexBuffer(&InRenderItem->RenderData->IndexBufferView());
		commandList->IASetPrimitiveTopology(InRenderItem->PrimitiveType);

		// Set/Bind Per Object Data.
		lambda();

		for (auto& e : InRenderItem->RenderData->Sections)
		{
			Section& section = e.second;
			commandList->DrawIndexedInstanced(section.IndexCountPerInstance,
				section.InstanceCount,
				section.StartIndexLocation,
				section.BaseVertexLocation,
				section.StartInstanceLocation);
		}
	}

}
