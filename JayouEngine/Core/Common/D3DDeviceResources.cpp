//
// D3DDeviceResources.cpp - A wrapper for the Direct3D 12 device and swapchain
//

#include "D3DDeviceResources.h"

using namespace DirectX;
using namespace D3DCore;

using Microsoft::WRL::ComPtr;

namespace
{
    inline DXGI_FORMAT NoSRGB(DXGI_FORMAT fmt)
    {
        switch (fmt)
        {
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:   return DXGI_FORMAT_R8G8B8A8_UNORM;
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:   return DXGI_FORMAT_B8G8R8A8_UNORM;
        case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:   return DXGI_FORMAT_B8G8R8X8_UNORM;
        default:                                return fmt;
        }
    }
};

// Constructor for D3DDeviceResources.
D3DDeviceResources::D3DDeviceResources(
    DXGI_FORMAT backBufferFormat,
    DXGI_FORMAT depthBufferFormat,
    UINT backBufferCount,
    D3D_FEATURE_LEVEL minFeatureLevel,
    unsigned int flags) noexcept(false) :
        m_backBufferIndex(0),
        m_fenceValues{},
        m_rtvDescriptorSize(0),
        m_screenViewport{},
        m_scissorRect{},
        m_backBufferFormat(backBufferFormat),
        m_depthBufferFormat(depthBufferFormat),
        m_backBufferCount(backBufferCount),
        m_d3dMinFeatureLevel(minFeatureLevel),
        m_window(nullptr),
        m_d3dFeatureLevel(D3D_FEATURE_LEVEL_11_0),
        m_dxgiFactoryFlags(0),
        m_outputSize{0, 0, 1, 1},
        m_colorSpace(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709),
        m_options(flags),
        m_deviceNotify(nullptr)
{
    if (backBufferCount > MAX_BACK_BUFFER_COUNT)
    {
        throw std::out_of_range("backBufferCount too large");
    }
	
    if (minFeatureLevel < D3D_FEATURE_LEVEL_11_0)
    {
        throw std::out_of_range("minFeatureLevel too low");
    }

	m_numRTVDescriptors = backBufferCount + 1 + MAX_OFFSCREEN_RTV_COUNT; // +1 Msaa RTV, +8 Offscreen RTV (Current support 8). +6 CubeMap.
	m_numDSVDescriptors = 1 + 1 + 1; // +1 Default, +1 Msaa DSV, +1 Temp New.
}

// Destructor for D3DDeviceResources.
D3DDeviceResources::~D3DDeviceResources()
{
    // Ensure that the GPU is no longer referencing resources that are about to be destroyed.
    WaitForGpu();
}

// Private.
void D3DDeviceResources::CreateRenderTargetView()
{
	// Obtain the back buffers for this window which will be the final render targets
	// and create render target views for each of them.
	for (UINT n = 0; n < m_backBufferCount; n++)
	{
		ThrowIfFailedV1(m_swapChain->GetBuffer(n, IID_PPV_ARGS(m_renderTargets[n].GetAddressOf())));

		wchar_t name[25] = {};
		swprintf_s(name, L"Render target %u", n);
		m_renderTargets[n]->SetName(name);

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = m_backBufferFormat;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescCPUHandle(m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), n, m_rtvDescriptorSize);
		m_d3dDevice->CreateRenderTargetView(m_renderTargets[n].Get(), &rtvDesc, rtvDescCPUHandle);
	}

	// Msaa.
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = m_backBufferFormat;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;

	m_d3dDevice->CreateRenderTargetView(
		m_msaaRenderTarget.Get(), &rtvDesc,
		GetMsaaRenderTargetView());
}

void D3DDeviceResources::CreateDepthStencilView()
{
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvMSDesc = {};
	dsvMSDesc.Format = m_depthBufferFormat;
	dsvMSDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;

	m_d3dDevice->CreateDepthStencilView(
		m_msaaDepthStencil.Get(), &dsvMSDesc,
		GetMsaaDepthStencilView());

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = m_depthBufferFormat;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

	m_d3dDevice->CreateDepthStencilView(m_depthStencil.Get(), &dsvDesc, GetDepthStencilView());
	m_d3dDevice->CreateDepthStencilView(m_newDepthStencil.Get(), &dsvDesc, GetNewDepthStencilView());
}

void D3DDeviceResources::CreateRtvDescriptorHeaps_AutoUpdate(uint32 InNumRtvs /*= 0*/, PFVOID UpdateCallBack /*= nullptr*/)
{
	// RtvHeaps.
	D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc = {};
	rtvDescriptorHeapDesc.NumDescriptors = m_numRTVDescriptors + InNumRtvs;
	rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

	ThrowIfFailedV1(m_d3dDevice->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(m_rtvDescriptorHeap.ReleaseAndGetAddressOf())));

	m_rtvDescriptorHeap->SetName(L"D3DDeviceResources");

	if (UpdateCallBack != nullptr)
	{
		UpdateCallBack();
	}

	static bool flag = false;
	if (flag)
	{
		CreateRenderTargetView();		
	}
	flag = true;
}

void D3DDeviceResources::CreateDsvDescriptorHeaps_AutoUpdate(uint32 InNumDsvs /*= 0*/, PFVOID UpdateCallBack /*= nullptr*/)
{
	// DsvHeaps.
	D3D12_DESCRIPTOR_HEAP_DESC dsvDescriptorHeapDesc = {};
	dsvDescriptorHeapDesc.NumDescriptors = m_numDSVDescriptors + InNumDsvs;
	dsvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

	ThrowIfFailedV1(m_d3dDevice->CreateDescriptorHeap(&dsvDescriptorHeapDesc, IID_PPV_ARGS(m_dsvDescriptorHeap.ReleaseAndGetAddressOf())));

	m_dsvDescriptorHeap->SetName(L"D3DDeviceResources");

	if (UpdateCallBack != nullptr)
	{
		UpdateCallBack();
	}

	static bool flag = false;
	if (flag)
	{
		CreateDepthStencilView();
	}
	flag = true;
}
// Configures the Direct3D device, and stores handles to it and the device context.
void D3DDeviceResources::CreateDeviceResources() 
{
#if defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    //
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf()))))
        {
            debugController->EnableDebugLayer();
        }
        else
        {
            OutputDebugStringA("WARNING: Direct3D Debug Device is not available\n");
        }

        ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(dxgiInfoQueue.GetAddressOf()))))
        {
            m_dxgiFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;

            dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
            dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);

            DXGI_INFO_QUEUE_MESSAGE_ID hide[] =
            {
                80 /* IDXGISwapChain::GetContainingOutput: The swapchain's adapter does not control the output on which the swapchain's window resides. */,
            };
            DXGI_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumIDs = _countof(hide);
            filter.DenyList.pIDList = hide;
            dxgiInfoQueue->AddStorageFilterEntries(DXGI_DEBUG_DXGI, &filter);
        }
    }
#endif

    ThrowIfFailedV1(CreateDXGIFactory2(m_dxgiFactoryFlags, IID_PPV_ARGS(m_dxgiFactory.ReleaseAndGetAddressOf())));

    // Determines whether tearing support is available for fullscreen borderless windows.
    if (m_options & c_AllowTearing)
    {
        BOOL allowTearing = FALSE;

        ComPtr<IDXGIFactory5> factory5;
        HRESULT hr = m_dxgiFactory.As(&factory5);
        if (SUCCEEDED(hr))
        {
            hr = factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
        }

        if (FAILED(hr) || !allowTearing)
        {
            m_options &= ~c_AllowTearing;
#ifdef _DEBUG
            OutputDebugStringA("WARNING: Variable refresh rate displays not supported");
#endif
        }
    }

	if (m_useWarpDevice)
	{
		ComPtr<IDXGIAdapter> warpAdapter;
		ThrowIfFailedV1(m_dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

		ThrowIfFailedV1(D3D12CreateDevice(
			warpAdapter.Get(),
			m_d3dMinFeatureLevel,
			IID_PPV_ARGS(&m_d3dDevice)
		));
	}
	else
	{
		ComPtr<IDXGIAdapter1> adapter;
		GetAdapter(adapter.GetAddressOf());

		// Create the DX12 API device object.
		ThrowIfFailedV1(D3D12CreateDevice(
			adapter.Get(),
			m_d3dMinFeatureLevel,
			IID_PPV_ARGS(m_d3dDevice.ReleaseAndGetAddressOf())
		));
	}

    m_d3dDevice->SetName(L"D3DDeviceResources");

#ifndef NDEBUG
    // Configure debug device (if active).
    ComPtr<ID3D12InfoQueue> d3dInfoQueue;
    if (SUCCEEDED(m_d3dDevice.As(&d3dInfoQueue)))
    {
#ifdef _DEBUG
        d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
#endif
        D3D12_MESSAGE_ID hide[] =
        {
            D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
            D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE
        };
        D3D12_INFO_QUEUE_FILTER filter = {};
        filter.DenyList.NumIDs = _countof(hide);
        filter.DenyList.pIDList = hide;
        d3dInfoQueue->AddStorageFilterEntries(&filter);
    }
#endif

    // Determine maximum supported feature level for this device
    static const D3D_FEATURE_LEVEL s_featureLevels[] =
    {
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    D3D12_FEATURE_DATA_FEATURE_LEVELS featLevels =
    {
        _countof(s_featureLevels), s_featureLevels, D3D_FEATURE_LEVEL_11_0
    };

    HRESULT hr = m_d3dDevice->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &featLevels, sizeof(featLevels));
    if (SUCCEEDED(hr))
    {
        m_d3dFeatureLevel = featLevels.MaxSupportedFeatureLevel;
    }
    else
    {
        m_d3dFeatureLevel = m_d3dMinFeatureLevel;
    }

	{
		//////////////////////////////////////////////////////////////////
		// Check 4X MSAA quality support for our back buffer format.
		// All Direct3D 11 capable devices support 4X MSAA for all render 
		// target formats, so we only need to check quality support.
		D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
		msQualityLevels.Format = m_backBufferFormat;
		msQualityLevels.SampleCount = 4;
		msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
		msQualityLevels.NumQualityLevels = 0;
		ThrowIfFailedV1(m_d3dDevice->CheckFeatureSupport(
			D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
			&msQualityLevels,
			sizeof(msQualityLevels)));

		m_num4MSQualityLevels = msQualityLevels.NumQualityLevels;
		assert(m_num4MSQualityLevels > 0 && "Unexpected MSAA quality level.");
		//////////////////////////////////////////////////////////////////
		// The same as above.
		// Check for MSAA support.
		//
		// Note that 4x MSAA and 8x MSAA is required for Direct3D Feature Level 11.0 or better
		//
		
		for (m_sampleCount = m_targetSampleCount; m_sampleCount > 1; m_sampleCount--)
		{
			D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS levels = { m_backBufferFormat, m_sampleCount };
			if (FAILED(m_d3dDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &levels, sizeof(levels))))
				continue;

			if (levels.NumQualityLevels > 0)
				break;
		}

		if (m_sampleCount < 2)
		{
			throw std::exception("MSAA not supported");
		}
		//////////////////////////////////////////////////////////////////
	}

    // Create the command queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ThrowIfFailedV1(m_d3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(m_commandQueue.ReleaseAndGetAddressOf())));

    m_commandQueue->SetName(L"D3DDeviceResources");

    m_rtvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	m_dsvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	m_cbvSrvUavDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Create a command allocator for each back buffer that will be rendered to.
    for (UINT n = 0; n < m_backBufferCount; n++)
    {
        ThrowIfFailedV1(m_d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(m_commandAllocators[n].ReleaseAndGetAddressOf())));

        wchar_t name[25] = {};
        swprintf_s(name, L"Render target %u", n);
        m_commandAllocators[n]->SetName(name);
    }

    // Create a command list for recording graphics commands.
    ThrowIfFailedV1(m_d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(m_commandList.ReleaseAndGetAddressOf())));
    ThrowIfFailedV1(m_commandList->Close());

    m_commandList->SetName(L"D3DDeviceResources");

    // Create a fence for tracking GPU execution progress.
    ThrowIfFailedV1(m_d3dDevice->CreateFence(m_fenceValues[m_backBufferIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_fence.ReleaseAndGetAddressOf())));
    m_fenceValues[m_backBufferIndex]++;

    m_fence->SetName(L"D3DDeviceResources");

    m_fenceEvent.Attach(CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE));
    if (!m_fenceEvent.IsValid())
    {
        throw std::exception("CreateEvent");
    }

	CreateRtvDescriptorHeaps_AutoUpdate();
	CreateDsvDescriptorHeaps_AutoUpdate();
}

// These resources need to be recreated every time the window size is changed.
void D3DDeviceResources::CreateWindowSizeDependentResources() 
{
    if (!m_window)
    {
        throw std::exception("Call SetWindow with a valid Win32 window handle");
    }

    // Wait until all previous GPU work is complete.
    WaitForGpu();

    // Release resources that are tied to the swap chain and update fence values.
    for (UINT n = 0; n < m_backBufferCount; n++)
    {
        m_renderTargets[n].Reset();
        m_fenceValues[n] = m_fenceValues[m_backBufferIndex];
    }

    // Determine the render target size in pixels.
    UINT backBufferWidth = std::max<UINT>(m_outputSize.right - m_outputSize.left, 1);
    UINT backBufferHeight = std::max<UINT>(m_outputSize.bottom - m_outputSize.top, 1);
    DXGI_FORMAT backBufferFormat = NoSRGB(m_backBufferFormat);

    // If the swap chain already exists, resize it, otherwise create one.
    if (m_swapChain)
    {
        // If the swap chain already exists, resize it.
        HRESULT hr = m_swapChain->ResizeBuffers(
            m_backBufferCount,
            backBufferWidth,
            backBufferHeight,
            backBufferFormat,
            (m_options & c_AllowTearing) ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0
            );

        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
        {
#ifdef _DEBUG
            char buff[64] = {};
            sprintf_s(buff, "Device Lost on ResizeBuffers: Reason code 0x%08X\n", (hr == DXGI_ERROR_DEVICE_REMOVED) ? m_d3dDevice->GetDeviceRemovedReason() : hr);
            OutputDebugStringA(buff);
#endif
            // If the device was removed for any reason, a new device and swap chain will need to be created.
            HandleDeviceLost();

            // Everything is set up now. Do not continue execution of this method. HandleDeviceLost will reenter this method 
            // and correctly set up the new device.
            return;
        }
        else
        {
            ThrowIfFailedV1(hr);
        }
    }
    else
    {
        // Create a descriptor for the swap chain.
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.Width = backBufferWidth;
        swapChainDesc.Height = backBufferHeight;
        swapChainDesc.Format = backBufferFormat;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = m_backBufferCount;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        swapChainDesc.Flags = (m_options & c_AllowTearing) ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

        DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
        fsSwapChainDesc.Windowed = TRUE;

        // Create a swap chain for the window.
        ComPtr<IDXGISwapChain1> swapChain;
        ThrowIfFailedV1(m_dxgiFactory->CreateSwapChainForHwnd(
            m_commandQueue.Get(),
            m_window,
            &swapChainDesc,
            &fsSwapChainDesc,
            nullptr,
            swapChain.GetAddressOf()
            ));

        ThrowIfFailedV1(swapChain.As(&m_swapChain));

        // This class does not support exclusive full-screen mode and prevents DXGI from responding to the ALT+ENTER shortcut
        ThrowIfFailedV1(m_dxgiFactory->MakeWindowAssociation(m_window, DXGI_MWA_NO_ALT_ENTER));
    }

    // Handle color space settings for HDR
    UpdateColorSpace();

    // Reset the index to the current back buffer.
    m_backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

	// Create RTV & DSV Resources.
	CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
	{	
		//////////////////////////////////////////////////////////////
		// Create 4x MSAA RT.
		D3D12_RESOURCE_DESC msaaRTDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			m_backBufferFormat,
			backBufferWidth,
			backBufferHeight,
			1, // This render target view has only one texture.
			1, // Use a single mipmap level
			m_sampleCount,  // <--- Use 4x MSAA
			m_num4MSQualityLevels - 1
		);
		msaaRTDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		D3D12_CLEAR_VALUE msaaOptimizedClearValue = {};
		msaaOptimizedClearValue.Format = m_backBufferFormat;
		memcpy(msaaOptimizedClearValue.Color, DefaultClearValue::ColorRGBA, sizeof(float) * 4);

		ThrowIfFailedV1(m_d3dDevice->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&msaaRTDesc,
			D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
			&msaaOptimizedClearValue,
			IID_PPV_ARGS(m_msaaRenderTarget.ReleaseAndGetAddressOf())
		));
		// Create 4x MSAA RT.
		//////////////////////////////////////////////////////////////	

		if (m_depthBufferFormat != DXGI_FORMAT_UNKNOWN)
		{
			//////////////////////////////////////////////////////////////
			// Create an MSAA Depth Stencil.
			D3D12_RESOURCE_DESC depthStencilDesc0 = CD3DX12_RESOURCE_DESC::Tex2D(
				m_depthBufferFormat,
				backBufferWidth,
				backBufferHeight,
				1, // This depth stencil view has only one texture.
				1, // Use a single mipmap level.
				m_sampleCount,
				m_num4MSQualityLevels - 1
			);
			depthStencilDesc0.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

			D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
			depthOptimizedClearValue.Format = m_depthBufferFormat;
			depthOptimizedClearValue.DepthStencil.Depth = DefaultClearValue::Depth;
			depthOptimizedClearValue.DepthStencil.Stencil = DefaultClearValue::Stencil;

			ThrowIfFailedV1(m_d3dDevice->CreateCommittedResource(
				&heapProperties,
				D3D12_HEAP_FLAG_NONE,
				&depthStencilDesc0,
				D3D12_RESOURCE_STATE_DEPTH_WRITE,
				&depthOptimizedClearValue,
				IID_PPV_ARGS(m_msaaDepthStencil.ReleaseAndGetAddressOf())
			));
			// Create an MSAA depth stencil view.
			//////////////////////////////////////////////////////////////

			// Create 2 DVS. One Default, One Maintained.
			D3D12_RESOURCE_DESC depthStencilDesc1 = depthStencilDesc0;
			depthStencilDesc1.SampleDesc.Count = 1;
			depthStencilDesc1.SampleDesc.Quality = 0;

			ThrowIfFailedV1(m_d3dDevice->CreateCommittedResource(
				&heapProperties,
				D3D12_HEAP_FLAG_NONE,
				&depthStencilDesc1,
				D3D12_RESOURCE_STATE_DEPTH_WRITE,
				&depthOptimizedClearValue,
				IID_PPV_ARGS(m_depthStencil.ReleaseAndGetAddressOf())
			));

			ThrowIfFailedV1(m_d3dDevice->CreateCommittedResource(
				&heapProperties,
				D3D12_HEAP_FLAG_NONE,
				&depthStencilDesc1,
				D3D12_RESOURCE_STATE_DEPTH_WRITE,
				&depthOptimizedClearValue,
				IID_PPV_ARGS(m_newDepthStencil.ReleaseAndGetAddressOf())
			));

			m_depthStencil->SetName(L"Depth stencil");

			m_newDepthStencil->SetName(L"Depth stencil");
		}
	}

    // Set the 3D rendering viewport and scissor rectangle to target the entire window.
    m_screenViewport.TopLeftX = m_screenViewport.TopLeftY = 0.f;
    m_screenViewport.Width = static_cast<float>(backBufferWidth);
    m_screenViewport.Height = static_cast<float>(backBufferHeight);
    m_screenViewport.MinDepth = D3D12_MIN_DEPTH;
    m_screenViewport.MaxDepth = D3D12_MAX_DEPTH;

    m_scissorRect.left = m_scissorRect.top = 0;
    m_scissorRect.right = backBufferWidth;
    m_scissorRect.bottom = backBufferHeight;

	CreateRenderTargetView();
	CreateDepthStencilView();
}

// This method is called when the Win32 window is created (or re-created).
void D3DDeviceResources::SetWindow(HWND window, int width, int height, std::wstring cmdLine)
{
    m_window = window;

    m_outputSize.left = m_outputSize.top = 0;
    m_outputSize.right = width;
    m_outputSize.bottom = height;

	ParseCommandLine(cmdLine);
}

// This method is called when the Win32 window changes size.
bool D3DDeviceResources::WindowSizeChanged(int width, int height)
{
    RECT newRc;
    newRc.left = newRc.top = 0;
    newRc.right = width;
    newRc.bottom = height;
    if (newRc.left == m_outputSize.left
        && newRc.top == m_outputSize.top
        && newRc.right == m_outputSize.right
        && newRc.bottom == m_outputSize.bottom)
    {
        // Handle color space settings for HDR
        UpdateColorSpace();

        return false;
    }

    m_outputSize = newRc;
    CreateWindowSizeDependentResources();
    return true;
}

// Recreate all device resources and set them back to the current state.
void D3DDeviceResources::HandleDeviceLost()
{
    if (m_deviceNotify)
    {
        m_deviceNotify->OnDeviceLost();
    }

    for (UINT n = 0; n < m_backBufferCount; n++)
    {
        m_commandAllocators[n].Reset();
        m_renderTargets[n].Reset();
    }

	m_msaaRenderTarget.Reset();
    m_depthStencil.Reset();
	m_newDepthStencil.Reset();
    m_commandQueue.Reset();
    m_commandList.Reset();
    m_fence.Reset();
    m_rtvDescriptorHeap.Reset();
    m_dsvDescriptorHeap.Reset();
    m_swapChain.Reset();
    m_d3dDevice.Reset();
    m_dxgiFactory.Reset();

#ifdef _DEBUG
    {
        ComPtr<IDXGIDebug1> dxgiDebug;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
        {
            dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
        }
    }
#endif

    CreateDeviceResources();
    CreateWindowSizeDependentResources();

    if (m_deviceNotify)
    {
        m_deviceNotify->OnDeviceRestored();
    }
}

// Prepare the command list and render target for rendering.
void D3DDeviceResources::Prepare(D3D12_RESOURCE_STATES beforeState)
{
	UpdateGUI();

    // Reset command list and allocator.
    ThrowIfFailedV1(m_commandAllocators[m_backBufferIndex]->Reset());
    ThrowIfFailedV1(m_commandList->Reset(m_commandAllocators[m_backBufferIndex].Get(), nullptr));

	D3D12_RESOURCE_STATES dirtyState = beforeState;

	if (m_options & c_Enable4xMsaa)
		dirtyState = D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
	else if (m_options & c_EnableOffscreenRender)
		dirtyState = D3D12_RESOURCE_STATE_COPY_SOURCE;

    if (dirtyState != D3D12_RESOURCE_STATE_RENDER_TARGET)
    {
        // Transition the render target into the correct state to allow for drawing into it.
        D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			(m_options & c_Enable4xMsaa) ? m_msaaRenderTarget.Get() : 
			(m_options & c_EnableOffscreenRender) ? m_offscreenRenderTargets[m_activeOffscreenRTIndex].Get() : 
			m_renderTargets[m_backBufferIndex].Get(),
			dirtyState, 
			D3D12_RESOURCE_STATE_RENDER_TARGET);

        m_commandList->ResourceBarrier(1, &barrier);
    }
}

void D3DDeviceResources::Clear(const FLOAT colorRGBA[4], FLOAT depth, UINT8 stencil)
{
	// Clear the views.
	auto renderTargetView = GetActiveRenderTargetView();
	auto depthStencilView = GetActiveDepthStencilView();
	m_commandList->OMSetRenderTargets(1, &renderTargetView, FALSE, &depthStencilView);
	m_commandList->ClearRenderTargetView(renderTargetView, colorRGBA, 0, nullptr);
	m_commandList->ClearDepthStencilView(depthStencilView, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, depth, stencil, 0, nullptr);

	// Set the viewport and scissor rect.
	m_commandList->RSSetViewports(1, &m_screenViewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);
}

// Present the contents of the swap chain to the screen.
void D3DDeviceResources::Present(D3D12_RESOURCE_STATES beforeState)
{
	if (m_options & c_Enable4xMsaa)
	{
		// Resolve the MSAA render target.
		{
			D3D12_RESOURCE_BARRIER barriers[2] =
			{
				CD3DX12_RESOURCE_BARRIER::Transition(
					m_msaaRenderTarget.Get(),
					D3D12_RESOURCE_STATE_RENDER_TARGET,
					D3D12_RESOURCE_STATE_RESOLVE_SOURCE),
				CD3DX12_RESOURCE_BARRIER::Transition(
					(m_options & c_EnableOffscreenRender) ? m_offscreenRenderTargets[m_activeOffscreenRTIndex].Get() :
					m_renderTargets[m_backBufferIndex].Get(),
					(m_options & c_EnableOffscreenRender) ? D3D12_RESOURCE_STATE_RESOLVE_SOURCE : 
					D3D12_RESOURCE_STATE_PRESENT,
					D3D12_RESOURCE_STATE_RESOLVE_DEST)
			};

			m_commandList->ResourceBarrier(2, barriers);
		}

		m_commandList->ResolveSubresource(
			(m_options & c_EnableOffscreenRender) ? m_offscreenRenderTargets[m_activeOffscreenRTIndex].Get() :
			m_renderTargets[m_backBufferIndex].Get(), 0, m_msaaRenderTarget.Get(), 0, m_backBufferFormat);

		// Set render target (Back Buffer) for GUI which is typically rendered without MSAA or Offscreen.
		{
			D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				(m_options & c_EnableOffscreenRender) ? m_offscreenRenderTargets[m_activeOffscreenRTIndex].Get() :
				m_renderTargets[m_backBufferIndex].Get(),
				D3D12_RESOURCE_STATE_RESOLVE_DEST,
				D3D12_RESOURCE_STATE_RENDER_TARGET);

			m_commandList->ResourceBarrier(1, &barrier);
		}

		m_commandList->OMSetRenderTargets(1, &GetRenderTargetView(), FALSE, nullptr);
	}

	// PostProcessing after item draw, before render GUI, Offscreen RT required.
	if (m_options & c_EnableOffscreenRender)
	{
		// Change offscreen texture to be used as an input.
		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			m_offscreenRenderTargets[m_activeOffscreenRTIndex].Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, 
			D3D12_RESOURCE_STATE_GENERIC_READ));

		PostProcessing();

		D3D12_RESOURCE_BARRIER barriers[2] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(
				m_offscreenRenderTargets[m_activeOffscreenRTIndex].Get(),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				D3D12_RESOURCE_STATE_COPY_SOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(
				m_renderTargets[m_backBufferIndex].Get(),
				D3D12_RESOURCE_STATE_PRESENT,
				D3D12_RESOURCE_STATE_COPY_DEST)
		};

		m_commandList->ResourceBarrier(2, barriers);

		m_commandList->CopyResource(m_renderTargets[m_backBufferIndex].Get(), 
			m_offscreenRenderTargets[m_activeOffscreenRTIndex].Get());

		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			m_renderTargets[m_backBufferIndex].Get(),
			D3D12_RESOURCE_STATE_COPY_DEST, 
			D3D12_RESOURCE_STATE_RENDER_TARGET));

		m_commandList->OMSetRenderTargets(1, &GetRenderTargetView(), FALSE, nullptr);
		m_commandList->ClearRenderTargetView(GetRenderTargetView(), DefaultClearValue::ColorRGBA, 0, nullptr);
	}

	// Render GUI.
	RenderGUI();

    if (beforeState != D3D12_RESOURCE_STATE_PRESENT)
    {
        // Transition the render target to the state that allows it to be presented to the display.
        D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			GetRenderTarget(), 
			beforeState,
			D3D12_RESOURCE_STATE_PRESENT);

        m_commandList->ResourceBarrier(1, &barrier);
    }

    // Send the command list off to the GPU for processing.
    ThrowIfFailedV1(m_commandList->Close());
    m_commandQueue->ExecuteCommandLists(1, CommandListCast(m_commandList.GetAddressOf()));

    HRESULT hr;
    if (m_options & c_AllowTearing)
    {
        // Recommended to always use tearing if supported when using a sync interval of 0.
        // Note this will fail if in true 'fullscreen' mode.
        hr = m_swapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
    }
    else
    {
        // The first argument instructs DXGI to block until VSync, putting the application
        // to sleep until the next VSync. This ensures we don't waste any cycles rendering
        // frames that will never be displayed to the screen.
        hr = m_swapChain->Present(1, 0);
    }

    // If the device was reset we must completely reinitialize the renderer.
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
#ifdef _DEBUG
        char buff[64] = {};
        sprintf_s(buff, "Device Lost on Present: Reason code 0x%08X\n", (hr == DXGI_ERROR_DEVICE_REMOVED) ? m_d3dDevice->GetDeviceRemovedReason() : hr);
        OutputDebugStringA(buff);
#endif
        HandleDeviceLost();
    }
    else
    {
        ThrowIfFailedV1(hr);

        MoveToNextFrame();

        if (!m_dxgiFactory->IsCurrent())
        {
            // Output information is cached on the DXGI Factory. If it is stale we need to create a new factory.
            ThrowIfFailedV1(CreateDXGIFactory2(m_dxgiFactoryFlags, IID_PPV_ARGS(m_dxgiFactory.ReleaseAndGetAddressOf())));
        }
    }
}

// Wait for pending GPU work to complete.
void D3DDeviceResources::WaitForGpu() noexcept
{
    if (m_commandQueue && m_fence && m_fenceEvent.IsValid())
    {
        // Schedule a Signal command in the GPU queue.
        UINT64 fenceValue = m_fenceValues[m_backBufferIndex];
        if (SUCCEEDED(m_commandQueue->Signal(m_fence.Get(), fenceValue)))
        {
            // Wait until the Signal has been processed.
            if (SUCCEEDED(m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent.Get())))
            {
                WaitForSingleObjectEx(m_fenceEvent.Get(), INFINITE, FALSE);

                // Increment the fence value for the current frame.
                m_fenceValues[m_backBufferIndex]++;
            }
        }
    }
}

// Prepare to render the next frame.
void D3DDeviceResources::MoveToNextFrame()
{
    // Schedule a Signal command in the queue.
    const UINT64 currentFenceValue = m_fenceValues[m_backBufferIndex];
    ThrowIfFailedV1(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));

    // Update the back buffer index.
    m_backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

    // If the next frame is not ready to be rendered yet, wait until it is ready.
    if (m_fence->GetCompletedValue() < m_fenceValues[m_backBufferIndex])
    {
        ThrowIfFailedV1(m_fence->SetEventOnCompletion(m_fenceValues[m_backBufferIndex], m_fenceEvent.Get()));
        WaitForSingleObjectEx(m_fenceEvent.Get(), INFINITE, FALSE);
    }

    // Set the fence value for the next frame.
    m_fenceValues[m_backBufferIndex] = currentFenceValue + 1;
}

// This method acquires the first available hardware adapter that supports Direct3D 12.
// If no such adapter can be found, try WARP. Otherwise throw an exception.
void D3DDeviceResources::GetAdapter(IDXGIAdapter1** ppAdapter)
{
    *ppAdapter = nullptr;

    ComPtr<IDXGIAdapter1> adapter;

#if defined(__dxgi1_6_h__) && defined(NTDDI_WIN10_RS4)
    ComPtr<IDXGIFactory6> factory6;
    HRESULT hr = m_dxgiFactory.As(&factory6);
    if (SUCCEEDED(hr))
    {
        for (UINT adapterIndex = 0;
            DXGI_ERROR_NOT_FOUND != factory6->EnumAdapterByGpuPreference(
                adapterIndex,
                DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf()));
            adapterIndex++)
        {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                // Don't select the Basic Render Driver adapter.
                continue;
            }

            // Check to see if the adapter supports Direct3D 12, but don't create the actual device yet.
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), m_d3dMinFeatureLevel, _uuidof(ID3D12Device), nullptr)))
            {
            #ifdef _DEBUG
                wchar_t buff[256] = {};
                swprintf_s(buff, L"Direct3D Adapter (%u): VID:%04X, PID:%04X - %ls\n", adapterIndex, desc.VendorId, desc.DeviceId, desc.Description);
                OutputDebugStringW(buff);
            #endif
                break;
            }
        }
    }
    else
#endif
    for (UINT adapterIndex = 0;
        DXGI_ERROR_NOT_FOUND != m_dxgiFactory->EnumAdapters1(
            adapterIndex,
            adapter.ReleaseAndGetAddressOf());
        ++adapterIndex)
    {
        DXGI_ADAPTER_DESC1 desc;
        ThrowIfFailedV1(adapter->GetDesc1(&desc));

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            // Don't select the Basic Render Driver adapter.
            continue;
        }

        // Check to see if the adapter supports Direct3D 12, but don't create the actual device yet.
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), m_d3dMinFeatureLevel, _uuidof(ID3D12Device), nullptr)))
        {
        #ifdef _DEBUG
            wchar_t buff[256] = {};
            swprintf_s(buff, L"Direct3D Adapter (%u): VID:%04X, PID:%04X - %ls\n", adapterIndex, desc.VendorId, desc.DeviceId, desc.Description);
            OutputDebugStringW(buff);
        #endif
            break;
        }
    }

#if !defined(NDEBUG)
    if (!adapter)
    {
        // Try WARP12 instead
        if (FAILED(m_dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf()))))
        {
            throw std::exception("WARP12 not available. Enable the 'Graphics Tools' optional feature");
        }

        OutputDebugStringA("Direct3D Adapter - WARP12\n");
    }
#endif

    if (!adapter)
    {
        throw std::exception("No Direct3D 12 device found");
    }

    *ppAdapter = adapter.Detach();
}

// Sets the color space for the swap chain in order to handle HDR output.
void D3DDeviceResources::UpdateColorSpace()
{
    DXGI_COLOR_SPACE_TYPE colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;

    bool isDisplayHDR10 = false;

#if defined(NTDDI_WIN10_RS2)
    if (m_swapChain)
    {
        ComPtr<IDXGIOutput> output;
        if (SUCCEEDED(m_swapChain->GetContainingOutput(output.GetAddressOf())))
        {
            ComPtr<IDXGIOutput6> output6;
            if (SUCCEEDED(output.As(&output6)))
            {
                DXGI_OUTPUT_DESC1 desc;
                ThrowIfFailedV1(output6->GetDesc1(&desc));

                if (desc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)
                {
                    // Display output is HDR10.
                    isDisplayHDR10 = true;
                }
            }
        }
    }
#endif

    if ((m_options & c_EnableHDR) && isDisplayHDR10)
    {
        switch (m_backBufferFormat)
        {
        case DXGI_FORMAT_R10G10B10A2_UNORM:
            // The application creates the HDR10 signal.
            colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
            break;

        case DXGI_FORMAT_R16G16B16A16_FLOAT:
            // The system creates the HDR10 signal; application uses linear values.
            colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;
            break;

        default:
            break;
        }
    }

    m_colorSpace = colorSpace;

    UINT colorSpaceSupport = 0;
    if (SUCCEEDED(m_swapChain->CheckColorSpaceSupport(colorSpace, &colorSpaceSupport))
        && (colorSpaceSupport & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT))
    {
        ThrowIfFailedV1(m_swapChain->SetColorSpace1(colorSpace));
    }
}

void D3DDeviceResources::UpdateGUI()
{
	if (m_deviceNotify)
	{
		m_deviceNotify->UpdateGUI();
	}
}

void D3DDeviceResources::RenderGUI()
{
	if (m_deviceNotify)
	{
		m_deviceNotify->RenderGUI();
	}
}

void D3DDeviceResources::PostProcessing()
{
	if (m_deviceNotify)
	{
		m_deviceNotify->PostProcessing();
	}
}

void D3DDeviceResources::OnOptionsChanged()
{
	WaitForGpu();
	if (m_deviceNotify)
	{
		m_deviceNotify->OnOptionsChanged();
	}
}

void D3DDeviceResources::ParseCommandLine(std::wstring cmdLine)
{
	std::wstring::size_type found = cmdLine.find(L"-warp");
	if (found != std::wstring::npos)
	{
		m_useWarpDevice = true;
	}

	if (m_deviceNotify)
	{
		m_deviceNotify->ParseCommandLine(cmdLine);
	}
}

void D3DDeviceResources::CreateRootSignature(D3D12_ROOT_SIGNATURE_DESC* pRootSigDesc, ID3D12RootSignature** ppRootSignature)
{
	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(pRootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailedV1(hr);

	ThrowIfFailedV1(m_d3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(ppRootSignature)));
}

void D3DDeviceResources::CreateCommonDescriptorHeap(UINT numDescriptors, ID3D12DescriptorHeap** ppDescriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE heapType /*= D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV*/, D3D12_DESCRIPTOR_HEAP_FLAGS heapFlags /*= D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE*/, UINT heapNodeMask /*= 0*/)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc;
	desc.NumDescriptors = numDescriptors;
	desc.Type = heapType;
	desc.Flags = heapFlags;
	desc.NodeMask = heapNodeMask;

	ThrowIfFailedV1(m_d3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(ppDescriptorHeap)));
}

void D3DDeviceResources::CreateTex2DShaderResourceView(ID3D12Resource *pResource, D3D12_CPU_DESCRIPTOR_HANDLE destDescriptor, DXGI_FORMAT InFormat /*= DXGI_FORMAT_UNKNOWN*/, UINT mipLevels /*= 1*/)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	if (InFormat != DXGI_FORMAT_UNKNOWN)
	{
		srvDesc.Format = InFormat;
	}
	else
	{
		srvDesc.Format = pResource->GetDesc().Format;
	}
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = mipLevels;

	m_d3dDevice->CreateShaderResourceView(pResource, &srvDesc, destDescriptor);
}

void D3DDeviceResources::CreateGraphicsPipelineState(D3D12_GRAPHICS_PIPELINE_STATE_DESC* pDesc, ID3D12PipelineState** ppPipelineState)
{
	ThrowIfFailedV1(m_d3dDevice->CreateGraphicsPipelineState(pDesc, IID_PPV_ARGS(ppPipelineState)));
}

void D3DDeviceResources::CreateComputePipelineState(D3D12_COMPUTE_PIPELINE_STATE_DESC* pDesc, ID3D12PipelineState** ppPipelineState)
{
	ThrowIfFailedV1(m_d3dDevice->CreateComputePipelineState(pDesc, IID_PPV_ARGS(ppPipelineState)));
}

void D3DDeviceResources::CreateDefaultBuffer(const void* pData, UINT64 byteSize, ID3D12Resource** ppDefaultBuffer, ID3D12Resource** ppUploadBuffer)
{
	// Create the actual default buffer resource.
	ThrowIfFailedV1(m_d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(byteSize),
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(ppDefaultBuffer)));

	// In order to copy CPU memory data into our default buffer, we need to create
	// an intermediate upload heap. 
	ThrowIfFailedV1(m_d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(byteSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(ppUploadBuffer)));


	// Describe the data we want to copy into the default buffer.
	D3D12_SUBRESOURCE_DATA subResourceData = {};
	subResourceData.pData = pData;
	subResourceData.RowPitch = byteSize;
	subResourceData.SlicePitch = subResourceData.RowPitch;

	// Schedule to copy the data to the default buffer resource.  At a high level, the helper function UpdateSubresources
	// will copy the CPU memory into the intermediate upload heap.  Then, using ID3D12CommandList::CopySubresourceRegion,
	// the intermediate upload heap data will be copied to mBuffer.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(*ppDefaultBuffer,
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
	UpdateSubresources<1>(m_commandList.Get(), *ppDefaultBuffer, *ppUploadBuffer, 0, 0, 1, &subResourceData);
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(*ppDefaultBuffer,
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));

	// Note: uploadBuffer has to be kept alive after the above function calls because
	// the command list has not been executed yet that performs the actual copy.
	// The caller can Release the uploadBuffer after it knows the copy has been executed.
}

void D3DDeviceResources::CreateTexture2D(Texture* InTexture)
{
	// Create the actual default buffer resource.
	ThrowIfFailedV1(m_d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(InTexture->Format, InTexture->Width, InTexture->Height),
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&InTexture->Resource)));

	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(InTexture->Resource.Get(), 0, 1);

	// In order to copy CPU memory data into our default buffer, we need to create
	// an intermediate upload heap. 
	ThrowIfFailedV1(m_d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&InTexture->UploadHeap)));


	// Describe the data we want to copy into the default buffer.
	D3D12_SUBRESOURCE_DATA subResourceData = {};
	subResourceData.pData = InTexture->Data;
	subResourceData.RowPitch = InTexture->RowBytes;
	subResourceData.SlicePitch = InTexture->NumBytes;

	// Schedule to copy the data to the default buffer resource.  At a high level, the helper function UpdateSubresources
	// will copy the CPU memory into the intermediate upload heap.  Then, using ID3D12CommandList::CopySubresourceRegion,
	// the intermediate upload heap data will be copied to mBuffer.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(InTexture->Resource.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
	UpdateSubresources(m_commandList.Get(), InTexture->Resource.Get(), InTexture->UploadHeap.Get(), 0, 0, 1, &subResourceData);
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(InTexture->Resource.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));

	// Note: uploadBuffer has to be kept alive after the above function calls because
	// the command list has not been executed yet that performs the actual copy.
	// The caller can Release the uploadBuffer after it knows the copy has been executed.

	CreateTex2DShaderResourceView(InTexture->Resource.Get(), InTexture->HCPUDescriptor);
}

D3D12_GRAPHICS_PIPELINE_STATE_DESC D3DDeviceResources::CreateCommonPSO(const std::vector<D3D12_INPUT_ELEMENT_DESC>& InInputLayout, ID3D12RootSignature* InRootSig, ID3DBlob* InShaderVS, ID3DBlob* InShaderPS, ID3D12PipelineState** OutPSO)
{
	bool enable4xMsaa = GetDeviceOptions() & D3DDeviceResources::c_Enable4xMsaa;

	//////////////////////////////////////////////////////////////////////////
	// PSO Default.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC commonPSO;
	ZeroMemory(&commonPSO, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	/// Common.
	commonPSO.InputLayout = CD3DX12_INPUT_LAYOUT_DESC(InInputLayout);
	commonPSO.pRootSignature = InRootSig;
	/// Shader.
	commonPSO.VS = CD3DX12_SHADER_BYTECODE(InShaderVS);
	commonPSO.PS = CD3DX12_SHADER_BYTECODE(InShaderPS);
	/// Default.
	commonPSO.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	commonPSO.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	commonPSO.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	commonPSO.SampleMask = UINT_MAX;
	commonPSO.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	commonPSO.NumRenderTargets = 1;
	commonPSO.RTVFormats[0] = m_backBufferFormat;
	commonPSO.SampleDesc.Count = enable4xMsaa ? 4 : 1;
	commonPSO.SampleDesc.Quality = enable4xMsaa ? (GetNum4MSQualityLevels() - 1) : 0;
	commonPSO.DSVFormat = m_depthBufferFormat;
	CreateGraphicsPipelineState(&commonPSO, OutPSO);
	//////////////////////////////////////////////////////////////////////////
	return commonPSO;
}

void D3DDeviceResources::CreateOffscreenRenderTargets(UINT numRenderTargets, const DXGI_FORMAT* pRenderTargetFormats, const D3D12_RESOURCE_STATES* pDefaultStates, CD3DX12_CPU_DESCRIPTOR_HANDLE InHDescriptor)
{
	// Determine the render target size in pixels.
	UINT width = std::max<UINT>(m_outputSize.right - m_outputSize.left, 1);
	UINT height = std::max<UINT>(m_outputSize.bottom - m_outputSize.top, 1);

	CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);

	//////////////////////////////////////////////////////////////
	// Create Offscreen RT.
	for (UINT i = 0; i < numRenderTargets; ++i)
	{
		DXGI_FORMAT format = NoSRGB(pRenderTargetFormats[i]);
		D3D12_RESOURCE_DESC offscreenRTDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height);
		offscreenRTDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		D3D12_CLEAR_VALUE offscreenOptimizedClearValue = {};
		offscreenOptimizedClearValue.Format = format;
		memcpy(offscreenOptimizedClearValue.Color, DefaultClearValue::ColorRGBA, sizeof(float) * 4);

		ThrowIfFailedV1(m_d3dDevice->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&offscreenRTDesc,
			pDefaultStates[i],
			&offscreenOptimizedClearValue,
			IID_PPV_ARGS(m_offscreenRenderTargets[i].ReleaseAndGetAddressOf())
		));

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = format;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

		m_d3dDevice->CreateRenderTargetView(
			m_offscreenRenderTargets[i].Get(), &rtvDesc,
			GetOffscreenRenderTargetView(i));

		CreateTex2DShaderResourceView(m_offscreenRenderTargets[i].Get(), InHDescriptor);

		InHDescriptor.Offset(1, GetCbvSrvUavDescriptorSize());
	}
	// Create Offscreen RT.
	//////////////////////////////////////////////////////////////
}

const CD3DX12_STATIC_SAMPLER_DESC D3DDeviceResources::GetStaticSampler(const EStaticSampler& type)
{
	// Applications usually only need a handful of samplers. So just define them all up front
	// and keep them available as part of the root signature.  
	// In addition, you can only define 2032 number of static samplers, 
	// which is more than enough for most applications. If you do need more, 
	// however, you can just use non-static samplers and go through a sampler heap.
	// D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, device->CreateSampler().

	const CD3DX12_STATIC_SAMPLER_DESC PointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC PointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC LinearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC LinearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC AnisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC AnisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC Shadow(
		6, // shaderRegister
		D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressW
		0.0f,                               // mipLODBias
		1, //16,                                 // maxAnisotropy
		D3D12_COMPARISON_FUNC_LESS_EQUAL,
		D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK);

#define Ret(x) case SS_##x: return x;
	switch (type)
	{
		Ret(PointWrap);
		Ret(PointClamp);
		Ret(LinearWrap);
		Ret(LinearClamp);
		Ret(AnisotropicWrap);
		Ret(AnisotropicClamp);
		Ret(Shadow);
	default:
		break;
	}
	return PointWrap;
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> D3DDeviceResources::GetAllStaticSamplers()
{
	return
	{
		GetStaticSampler(SS_PointWrap),
		GetStaticSampler(SS_PointClamp),
		GetStaticSampler(SS_LinearWrap),
		GetStaticSampler(SS_LinearClamp),
		GetStaticSampler(SS_AnisotropicWrap),
		GetStaticSampler(SS_AnisotropicClamp),
		GetStaticSampler(SS_Shadow)
	};
}
