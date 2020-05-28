//
// AppEntry.cpp
//

#include "AppEntry.h"
#include "Common/StringManager.h"
#include "Common/InputManager.h"

using namespace Utility;
using namespace WinUtility;
using namespace Utility::StringManager;
using namespace WinUtility::InputManager;

extern void ExitGame();

AppEntry::AppEntry(std::wstring appPath) noexcept(false)
{
    m_deviceResources = std::make_unique<D3DDeviceResources>(
		m_backBufferFormat, 
		m_depthBufferFormat,
		m_numFrameResources);

    m_deviceResources->RegisterDeviceNotify(this);
	m_appPath = appPath;
}

AppEntry::~AppEntry()
{
    if (m_deviceResources)
    {
        m_deviceResources->WaitForGpu();
		if (m_appGui)
		{
			m_appGui->DestroyGUI();
		}		
    }
}

void AppEntry::PreInitialize(HWND window, int width, int height, std::wstring cmdLine)
{
	m_window = window;
	m_width = width;
	m_height = height;

	m_deviceResources->SetWindow(window, width, height, cmdLine);

	m_deviceResources->CreateDeviceResources();
	// Create AppGui Instance & ...
	CreateDeviceDependentResources();

	m_deviceResources->CreateWindowSizeDependentResources();
	CreateWindowSizeDependentResources();

	m_timer = std::make_unique<BaseTimer>();
	m_camera = std::make_unique<Camera>();
	m_dirLightCamera = std::make_unique<Camera>();
	m_shadowMap = std::make_unique<ShadowMap>(m_deviceResources->GetD3DDevice(), 4096, 4096);
	// Now I Do Not Want to Use it!!!
	// m_cubeMap = std::make_unique<CubeMap>(m_deviceResources->GetD3DDevice(), m_cubeMapSize, m_cubeMapSize, DXGI_FORMAT_R8G8B8A8_UNORM, 6, 1);

	CreateApplicationDependentResources();
}

// Initialize the Direct3D resources required to run.
void AppEntry::Initialize(HWND window, int width, int height, std::wstring cmdLine)
{
	PreInitialize(window, width, height, cmdLine);

	m_appGui->GetAppData()->AppPath = m_appPath;
	m_appGui->InitGUI(cmdLine);
	
    // TODO: Change the timer settings if you want something other than the default variable timestep mode.
    // e.g. for 60 FPS fixed timestep update logic, call:
	m_timer->SetFixedTimeStep(true);
	m_timer->SetTargetDeltaSeconds(1.0f / 60);
	
	// Main Camera Init.
	m_camera->SetPosition(0.0f, 8.0f, -25.0f);
	m_camera->SetFrustum(0.25f*XM_PI, m_width, m_height, 1.0f, 2000.0f);
}

#pragma region Frame Update
// Executes the basic game loop.
void AppEntry::Tick()
{
	GarbageCollection();
	HandleRebuildRenderItem();
	HandleRenderItemStateChanged();

    m_timer->Tick([&]()
    {
		OnKeyboardInput(*m_timer);		
        Update(*m_timer);		
    });

	m_currFrameResourceIndex = (m_currFrameResourceIndex + 1) % m_deviceResources->GetBackBufferCount();
	m_currFrameResource = m_frameResources[m_currFrameResourceIndex].get();
	
	UpdateCamera();
	UpdatePerObjectCB();
	UpdateMainPassCB();
	UpdateShadowPassCB();
	UpdateLightSB();
	UpdateMaterialSB();

    Render();	
}

// Updates the Game World!!!
void AppEntry::Update(const BaseTimer& timer)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

	ResponseToGUIOptionChanged();
	
    PIXEndEvent();
}

void AppEntry::ResponseToGUIOptionChanged()
{
	// DO ONCE. GUI Options Changed.
	if (m_appGui->GetAppData()->bOptionsChanged)
	{
		m_appGui->GetAppData()->bOptionsChanged = false;
		if (m_appGui->GetAppData()->bEnable4xMsaa)
		{
			m_deviceResources->SetOptions(m_deviceResources->GetDeviceOptions() | D3DDeviceResources::c_Enable4xMsaa);
		}
		else
		{
			m_deviceResources->SetOptions(m_deviceResources->GetDeviceOptions() & ~D3DDeviceResources::c_Enable4xMsaa);
		}
	}

	// Grid Update.
	if (m_appGui->GetAppData()->bGridDirdy)
	{
		m_appGui->GetAppData()->bGridDirdy = false;
		if (m_appGui->GetAppData()->GridUnit >= 2u || m_appGui->GetAppData()->GridUnit <= 2000u)
		{
			m_deviceResources->ExecuteCommandLists([&]()
			{
				GeometryData<ColorVertex> gridMesh = GeometryCreator::CreateLineGrid(
					m_appGui->GetAppData()->GridWidth,
					m_appGui->GetAppData()->GridWidth,
					m_appGui->GetAppData()->GridUnit,
					m_appGui->GetAppData()->GridUnit,
					m_appGui->GetAppData()->GridColorX,
					m_appGui->GetAppData()->GridColorZ,
					m_appGui->GetAppData()->GridColorCell,
					m_appGui->GetAppData()->GridColorBlock);

				// Dynamic Create Resource Need WaitForGpu.
				m_deviceResources->WaitForGpu();

				m_allRItems["grid"]->NumVertices = (uint32)gridMesh.Vertices.size();
				m_allRItems["grid"]->NumIndices = (uint32)gridMesh.Indices32.size();
				m_deviceResources->CreateCommonGeometry<ColorVertex, uint32>(m_allRItems["grid"].get(), gridMesh.Vertices, gridMesh.Indices32);
			});
		}
	}
}

void AppEntry::UpdateMainPassCB()
{
	PassConstant passConstant;

	XMMATRIX view = m_camera->GetView();
	XMMATRIX proj = m_camera->GetProj();
	XMMATRIX viewProj = m_camera->GetViewProj();

	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	passConstant.View = Matrix4(view);
	passConstant.InvView = Matrix4(invView);
	passConstant.Proj = Matrix4(proj);
	passConstant.InvProj = Matrix4(invProj);
	passConstant.ViewProj = Matrix4(viewProj);
	passConstant.InvViewProj = Matrix4(invViewProj);
	passConstant.ShadowTransform = Matrix4(m_dirLightCamera->GetShadowTransform());

	passConstant.EyePosW = m_camera->GetPosition3f();
	passConstant.RenderTargetSize = XMFLOAT2((float)m_width, (float)m_height);
	passConstant.InvRenderTargetSize = XMFLOAT2(1.0f / m_width, 1.0f / m_height);
	passConstant.NearZ = m_camera->GetNearZ();
	passConstant.FarZ = m_camera->GetFarZ();
	passConstant.TotalTime = (float)m_timer->GetTotalSeconds();
	passConstant.DeltaTime = (float)m_timer->GetDeltaSeconds();

	// Lights.
	passConstant.AmbientFactor = m_appGui->GetAppData()->AmbientFactor;
	passConstant.NumDirLights = m_numDirLights;
	passConstant.NumPointLights = m_numPointLights;
	passConstant.NumSpotLights = m_numSpotLights;

	passConstant.CubeMapIndex = m_appGui->GetAppData()->CubeMapIndex;
	
	m_currFrameResource->CopyData<PassConstant>(0, passConstant);
}

void AppEntry::UpdateShadowPassCB()
{
	PassConstant passConstant;

	XMMATRIX view = m_dirLightCamera->GetView();
	XMMATRIX proj = m_dirLightCamera->GetProj();
	XMMATRIX viewProj = m_dirLightCamera->GetViewProj();

	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	passConstant.View = Matrix4(view);
	passConstant.InvView = Matrix4(invView);
	passConstant.Proj = Matrix4(proj);
	passConstant.InvProj = Matrix4(invProj);
	passConstant.ViewProj = Matrix4(viewProj);
	passConstant.InvViewProj = Matrix4(invViewProj);
	
	passConstant.EyePosW = m_dirLightCamera->GetPosition3f();
	passConstant.NearZ = m_dirLightCamera->GetNearZ();
	passConstant.FarZ = m_dirLightCamera->GetFarZ();

	// Camera Independent.
	passConstant.RenderTargetSize = XMFLOAT2((float)m_width, (float)m_height);
	passConstant.InvRenderTargetSize = XMFLOAT2(1.0f / m_width, 1.0f / m_height);
	passConstant.TotalTime = (float)m_timer->GetTotalSeconds();
	passConstant.DeltaTime = (float)m_timer->GetDeltaSeconds();

	m_currFrameResource->CopyData<PassConstant>(1, passConstant);
}

void AppEntry::UpdateMaterialSB()
{
	for (auto& mat : m_allMaterialRefs)
	{
		if (mat->NumFrameDirty > 0)
		{
			MaterialData materialData;
			materialData.DiffuseAlbedo = mat->DiffuseAlbedo;
			materialData.MatTransform = mat->MatTransform;
			materialData.Roughness = mat->Roughness;
			materialData.Metallicity = mat->Metallicity;
			materialData.DiffuseMapIndex = mat->DiffuseMapIndex;
			materialData.NormalMapIndex = mat->NormalMapIndex;
			materialData.ORMMapIndex = mat->ORMMapIndex;

			m_currFrameResource->CopyData<MaterialData>(mat->Index, materialData);

			mat->NumFrameDirty--;
		}
	}
}

void AppEntry::UpdateLightSB()
{
	for (auto& lit : m_allLightRefs)
	{
		if (lit->NumFrameDirty > 0)
		{
			LightData lightData;
			lightData.Position = lit->Position;
			lightData.Strength = lit->Strength;
			lightData.Direction = lit->Direction;
			lightData.FalloffStart = lit->FalloffStart;
			lightData.FalloffEnd = lit->FalloffEnd;

			if (lit->bIsVisible == false)
			{
				lightData.Strength = XMFLOAT3(0.0f, 0.0f, 0.0f);
			}

			m_currFrameResource->CopyData<LightData>(lit->Index, lightData);

			lit->NumFrameDirty--;
		}
	}
}

void AppEntry::UpdateCamera()
{
	// Set Camera View Type.
#define CV_SetView(x) case x:m_camera->SetViewType(x);break;
	switch (m_appGui->GetAppData()->_ECameraViewType)
	{
		CV_SetView(CV_FirstPersonView);
		CV_SetView(CV_FocusPointView);
		CV_SetView(CV_TopView);
		CV_SetView(CV_BottomView);
		CV_SetView(CV_LeftView);
		CV_SetView(CV_RightView);
		CV_SetView(CV_FrontView);
		CV_SetView(CV_BackView);
	default:
		break;
	}
#define CP_SetProj(x) case x:m_camera->SetProjType(x);break;
	switch (m_appGui->GetAppData()->_ECameraProjType)
	{
		CP_SetProj(CP_PerspectiveProj);
		CP_SetProj(CP_OrthographicProj);
	default:
		break;
	}

	m_camera->UpdateViewMatrix();

	// Update DirLight Cast Shadow Camera.
	if (!m_allLightRefs.empty() && m_allLightRefs[0]->LightType == LT_Directional)
	{
		// Only the first "main" light casts a shadow.
		XMVECTOR lightDir = XMLoadFloat3(&m_allLightRefs[0]->Direction);
		lightDir = XMVector3Normalize(lightDir);
		XMVECTOR lightPos = -2.0f*lightDir*(m_appGui->GetAppData()->SceneRadius > 10.0f ? m_appGui->GetAppData()->SceneRadius : 10.0f);
		XMFLOAT3 Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
		XMVECTOR targetPos = XMLoadFloat3(&Center);
		XMVECTOR lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

		// Define View Space.
		m_dirLightCamera->DirLightLookAt(lightPos, targetPos, lightUp);
		// Define Proj.
		m_dirLightCamera->SetDirLightFrustum(m_appGui->GetAppData()->SceneRadius > 10.0f ? m_appGui->GetAppData()->SceneRadius : 10.0f);
	}
}

void AppEntry::UpdatePerObjectCB()
{
	// PerObject Constant Buffer (Opaque).
	for (auto& ri : m_renderItemLayer[RenderLayer::All])
	{
		if (ri->NumFrameDirty > 0)
		{
			ObjectConstant objectConstant;
			objectConstant.World = ri->TransFormMatrix;
			objectConstant.InvTWorld = Math::Transpose(Math::Invert(objectConstant.World));
			objectConstant.MaterialIndex = ri->MaterialIndex;
			m_currFrameResource->CopyData<ObjectConstant>(ri->Index, objectConstant);

			ri->NumFrameDirty--;
		}
	}
}

#pragma endregion

#pragma region Frame Render
// Draws the scene.
void AppEntry::Render()
{	
    // Don't try to render anything before the first Update.
    if (m_timer->GetFrameCount() == 0)
    {
        return;
    }

    // Prepare the command list to render a new frame.
    m_deviceResources->Prepare();
    
    auto commandList = m_deviceResources->GetCommandList();

	// Clear the back buffers.
	PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Clear");
	m_deviceResources->Clear((float*)&m_appGui->GetAppData()->ClearColor);
	PIXEndEvent(commandList);

    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

    // TODO: Add your rendering code here.
	{
		ID3D12DescriptorHeap* descriptorHeaps[] = { m_srvCbvDescHeap.Get() };
		commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);		

		commandList->SetGraphicsRootSignature(m_rootSIGs["Main"].Get());
		
		if (!m_allLights.empty())
		{
			commandList->SetGraphicsRootShaderResourceView(2, m_currFrameResource->GetBufferGPUVirtualAddress<LightData>());
		}
		if (!m_allMaterials.empty())
		{
			commandList->SetGraphicsRootShaderResourceView(3, m_currFrameResource->GetBufferGPUVirtualAddress<MaterialData>());
		}

		// Shadow Pass.
		DrawSceneToShadowMap();

		// Set PerPass Data.
		commandList->SetGraphicsRootConstantBufferView(1, m_currFrameResource->GetBufferGPUVirtualAddress<PassConstant>());

		// Bind GBuffer & Debug Texture.
		commandList->SetGraphicsRootDescriptorTable(4, GetGPUDescriptorHeapStartOffset());
		commandList->SetGraphicsRootDescriptorTable(5, GetGPUDescriptorHeapStartOffset(m_maxPreGBuffers + m_maxGBuffers));

		// GBuffer. Currently Only Support For Opaque.
		{
			commandList->SetPipelineState(m_PSOs["GBuffer"].Get());
			commandList->OMSetRenderTargets(m_maxGBuffers, 
				&m_deviceResources->GetOffscreenRenderTargetView(0), true, 
				&m_deviceResources->GetActiveDepthStencilView());
			for (uint32 i = 0; i < m_maxGBuffers; ++i)
				commandList->ClearRenderTargetView(m_deviceResources->GetOffscreenRenderTargetView(i),
					Colors::Black, 0, nullptr);
			commandList->ClearDepthStencilView(m_deviceResources->GetActiveDepthStencilView(),
				D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 
				DefaultClearValue::Depth, DefaultClearValue::Stencil, 0, nullptr);
			
			DrawRenderItem(m_renderItemLayer[RenderLayer::Opaque]);

			for (uint32 i = 0; i < m_maxGBuffers; ++i)
				commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
					m_deviceResources->GetOffscreenRenderTarget(i),
					D3D12_RESOURCE_STATE_RENDER_TARGET,
					D3D12_RESOURCE_STATE_GENERIC_READ));

			commandList->OMSetRenderTargets(1, &m_deviceResources->GetActiveRenderTargetView(), FALSE, nullptr);
			
			commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
				m_deviceResources->GetDepthStencil(),
				D3D12_RESOURCE_STATE_DEPTH_WRITE,
				D3D12_RESOURCE_STATE_DEPTH_READ));
		}

		// Deferred Rendering.
		commandList->SetPipelineState(m_PSOs["PBR"].Get());
		DrawFullscreenQuad(commandList);

		// Forward Rendering Here.
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			m_deviceResources->GetDepthStencil(),
			D3D12_RESOURCE_STATE_DEPTH_READ,
			D3D12_RESOURCE_STATE_DEPTH_WRITE));

		commandList->OMSetRenderTargets(1, &m_deviceResources->GetActiveRenderTargetView(), FALSE,
			&m_deviceResources->GetActiveDepthStencilView());

		if (m_appGui->GetAppData()->bShowGrid)
		{
			commandList->SetPipelineState(m_PSOs["Line"].Get());
			DrawRenderItem(m_renderItemLayer[RenderLayer::Line]);
		}

		if (m_appGui->GetAppData()->bShowWireframe)
		{
			commandList->SetPipelineState(m_PSOs["Wireframe"].Get());
			for (auto& ri : m_renderItemLayer[RenderLayer::Opaque])
				ri->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_LINELIST;

			DrawRenderItem(m_renderItemLayer[RenderLayer::Opaque]);

			for (auto& ri : m_renderItemLayer[RenderLayer::Opaque])
				ri->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		}

		if (m_appGui->GetAppData()->bShowBackGround)
		{
			commandList->SetPipelineState(m_PSOs["FullscreenQuad"].Get());
			DrawFullscreenQuad(commandList);
		}

		if (m_appGui->GetAppData()->bShowSkySphere)
		{
			commandList->SetPipelineState(m_PSOs["SkySphere"].Get());
			DrawRenderItem(m_renderItemLayer[RenderLayer::SkySphere]);
		}

		for (uint32 i = 0; i < m_maxGBuffers; ++i)
			commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
				m_deviceResources->GetOffscreenRenderTarget(i),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				D3D12_RESOURCE_STATE_RENDER_TARGET));		
	}

    PIXEndEvent(commandList);

    // Show the new frame.
    PIXBeginEvent(m_deviceResources->GetCommandQueue(), PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();
    PIXEndEvent(m_deviceResources->GetCommandQueue());
}

void AppEntry::DrawRenderItem(std::vector<RenderItem*>& renderItemLayer)
{
	auto commandList = m_deviceResources->GetCommandList();

	for (auto& ri : renderItemLayer)
	{
		if (ri->bIsVisible == false)
			continue;

		m_deviceResources->DrawRenderItem(ri, [&]()
		{
			UINT objectCBufferByteSize = CalcConstantBufferByteSize(sizeof(ObjectConstant));
			D3D12_GPU_VIRTUAL_ADDRESS objectCBufferAddress = m_currFrameResource->GetBufferGPUVirtualAddress<ObjectConstant>()
				+ ri->Index * objectCBufferByteSize;
			commandList->SetGraphicsRootConstantBufferView(0, objectCBufferAddress);
		});
	}
}

void AppEntry::DrawFullscreenQuad(ID3D12GraphicsCommandList* commandList)
{
	// Null-out IA stage since we build the vertex off the SV_VertexID in the shader.
	commandList->IASetVertexBuffers(0, 1, nullptr);
	commandList->IASetIndexBuffer(nullptr);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->DrawInstanced(6, 1, 0, 0);
}

void AppEntry::DrawSceneToShadowMap()
{
	auto commandList = m_deviceResources->GetCommandList();

	commandList->RSSetViewports(1, &m_shadowMap->Viewport());
	commandList->RSSetScissorRects(1, &m_shadowMap->ScissorRect());

	// Change to DEPTH_WRITE.
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_shadowMap->Resource(),
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	UINT passCBByteSize = CalcConstantBufferByteSize(sizeof(PassConstant));

	// Clear the back buffer and depth buffer.
	commandList->ClearDepthStencilView(m_shadowMap->Dsv(),
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Set null render target because we are only going to draw to
	// depth buffer.  Setting a null render target will disable color writes.
	// Note the active PSO also must specify a render target count of 0.
	commandList->OMSetRenderTargets(0, nullptr, false, &m_shadowMap->Dsv());

	UINT passCBufferByteSize = CalcConstantBufferByteSize(sizeof(PassConstant));

	commandList->SetGraphicsRootConstantBufferView(1, m_currFrameResource->GetBufferGPUVirtualAddress<PassConstant>() + passCBufferByteSize);

	commandList->SetPipelineState(m_PSOs["Shadow"].Get());

	DrawRenderItem(m_renderItemLayer[RenderLayer::Opaque]);

	// Change back to GENERIC_READ so we can read the texture in a shader.
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_shadowMap->Resource(),
		D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ));

	commandList->RSSetViewports(1, &m_deviceResources->GetScreenViewport());
	commandList->RSSetScissorRects(1, &m_deviceResources->GetScissorRect());
}

#if 0
void AppEntry::DrawSceneToCubeMap()
{
	auto commandList = m_deviceResources->GetCommandList();

	commandList->RSSetViewports(1, &m_cubeMap->Viewport());
	commandList->RSSetScissorRects(1, &m_cubeMap->ScissorRect());

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_cubeMap->Resource(),
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));

	commandList->OMSetRenderTargets(1, &m_cubeMap->Rtv(), true, nullptr);

	commandList->SetPipelineState(m_PSOs["SkySphere"].Get());
	m_deviceResources->DrawRenderItem(m_allRItems["SkySphere"].get(), [&]()
	{
		UINT objectCBufferByteSize = CalcConstantBufferByteSize(sizeof(ObjectConstant));
		D3D12_GPU_VIRTUAL_ADDRESS objectCBufferAddress = m_currFrameResource->GetBufferGPUVirtualAddress<ObjectConstant>()
			+ m_allRItems["SkySphere"]->Index * objectCBufferByteSize;
		commandList->SetGraphicsRootConstantBufferView(0, objectCBufferAddress);
	});

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_cubeMap->Resource(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));

	commandList->RSSetViewports(1, &m_deviceResources->GetScreenViewport());
	commandList->RSSetScissorRects(1, &m_deviceResources->GetScissorRect());
}
#endif

#pragma endregion

#pragma region Message Handlers
// Message handlers
void AppEntry::OnActivated()
{
    // TODO: Game is becoming active window.
}

void AppEntry::OnDeactivated()
{
    // TODO: Game is becoming background window.
}

void AppEntry::OnSuspending()
{
    // TODO: Game is being power-suspended (or minimized).
}

void AppEntry::OnResuming()
{
	m_timer->Reset();

    // TODO: Game is being power-resumed (or returning from minimize).
}

void AppEntry::OnWindowMoved()
{
    auto r = m_deviceResources->GetOutputSize();
    m_deviceResources->WindowSizeChanged(r.right, r.bottom);
}

void AppEntry::OnWindowSizeChanged(int width, int height)
{
	m_deviceResources->WaitForGpu();
	m_appGui->GUIInvalidateDeviceObjects();

	m_width  = width;
	m_height = height;

	m_lastMousePos = XMINT2(0, 0); // Think it about if you have free time!!!

    if (!m_deviceResources->WindowSizeChanged(width, height))
        return;

    CreateWindowSizeDependentResources();

	m_appGui->GUICreateDeviceObjects();

    // TODO: Game window is being resized.

	// OnWindowSizeChanged.
	m_camera->SetFrustum(0.25f*XM_PI, m_width, m_height, 1.0f, 2000.0f);
}

void AppEntry::OnMouseDown(WPARAM btnState, int x, int y)
{
	if (m_bMousePosInBlockArea)
	{
		m_bMouseDownInBlockArea = true;
		return;
	}
	m_bMouseDownInBlockArea = false;

	if (btnState == MK_LBUTTON || btnState == MK_RBUTTON || btnState == MK_MBUTTON)
	{
		m_lastMousePos.x = x;
		m_lastMousePos.y = y;
		
		SetCapture(m_window);		
	}

	if (btnState == MK_LBUTTON || IsKeyDown(VK_CONTROL))
	{
		Pick(x, y);
	}

	if (btnState == MK_MBUTTON)
	{
		for (auto& ri : m_renderItemLayer[RenderLayer::Selectable])
		{
			ri->bSelected = false;
		}
	}
}

void AppEntry::OnMouseUp(WPARAM btnState, int x, int y)
{
	if (m_bMousePosInBlockArea)
		return;
	ReleaseCapture();
}

void AppEntry::OnMouseMove(WPARAM btnState, int x, int y)
{
	if (CheckInBlockAreas(x, y))
		m_bMousePosInBlockArea = true;
	else m_bMousePosInBlockArea = false;

	if (m_bMouseDownInBlockArea)
		return;

	if (btnState == MK_MBUTTON || btnState == MK_RBUTTON)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f*static_cast<float>(x - m_lastMousePos.x));
		float dy = XMConvertToRadians(0.25f*static_cast<float>(y - m_lastMousePos.y));

		m_camera->ProcessMoving(dx, dy);
	}
	else if (false/*btnState == MK_RBUTTON*/)
	{
		// Make each pixel correspond to 0.005 unit in the scene.
		float dx = 0.005f*static_cast<float>(x - m_lastMousePos.x);
		float dy = 0.005f*static_cast<float>(y - m_lastMousePos.y);
		m_camera->FocusRadius(dx - dy);
	}

	m_lastMousePos.x = x;
	m_lastMousePos.y = y;
}

// x, y is not the coordinate of the cursor but pointer.
void AppEntry::OnMouseWheel(int d, WPARAM btnState, int x, int y)
{
	if (m_bMousePosInBlockArea)
		return;
	m_camera->FocusRadius(0.02f*(float)d);
}

void AppEntry::OnKeyDown(WPARAM keyState)
{
	
}

void AppEntry::OnKeyUp(WPARAM keyState)
{

}

void AppEntry::OnKeyboardInput(const BaseTimer& timer)
{
	const float dt = (float)timer.GetDeltaSeconds();

	if (GetAsyncKeyState('W') & 0x8000)
		m_camera->Walk(m_appGui->GetAppData()->WalkSpeed*dt);

	if (GetAsyncKeyState('S') & 0x8000)
		m_camera->Walk(-m_appGui->GetAppData()->WalkSpeed*dt);

	if (GetAsyncKeyState('A') & 0x8000)
		m_camera->Strafe(-m_appGui->GetAppData()->WalkSpeed*dt);

	if (GetAsyncKeyState('D') & 0x8000)
		m_camera->Strafe(m_appGui->GetAppData()->WalkSpeed*dt);
}

// GUI Messages
bool AppEntry::CheckInBlockAreas(int x, int y)
{
	for (auto& blockArea : m_appGui->GetAppData()->BlockAreas)
	{
		if (blockArea == nullptr)
			continue;

		if (x >= blockArea->StartPos.x &&
			y >= blockArea->StartPos.y &&
			x <= (blockArea->StartPos.x + blockArea->BlockSize.x) &&
			y <= (blockArea->StartPos.y + blockArea->BlockSize.y))
			return true;
	}
	return false;
}

void AppEntry::Pick(int InPosX, int InPosY)
{
	XMFLOAT4X4 P = m_camera->GetProj4x4f();

	// Compute picking ray in view space.
	float vx = (+2.0f*InPosX / m_width - 1.0f) / P(0, 0);
	float vy = (-2.0f*InPosY / m_height + 1.0f) / P(1, 1);

	XMMATRIX V = m_camera->GetView();
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(V), V);

	// Check if we picked an opaque render item.  A real app might keep a separate "picking list"
	// of objects that can be selected.  
	std::string tname = "";
	float tmin0 = std::numeric_limits<float>::max();
	for (auto& ri : m_renderItemLayer[RenderLayer::Selectable])
	{
		// Skip invisible render-items.
		if (ri->bIsVisible == false || ri->bCanBeSelected == false)
			continue;

		XMMATRIX W = ri->TransFormMatrix;
		XMMATRIX invWorld = XMMatrixInverse(&XMMatrixDeterminant(W), W);

		// Tranform ray to vi space of Mesh.
		XMMATRIX toLocal = XMMatrixMultiply(invView, invWorld);

		// Ray definition in view space.
		XMVECTOR rayOrigin = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		XMVECTOR rayDir = XMVectorSet(vx, vy, 1.0f, 0.0f);
		rayOrigin = XMVector3TransformCoord(rayOrigin, toLocal);
		rayDir = XMVector3TransformNormal(rayDir, toLocal);

		// Make the ray direction unit length for the intersection tests.
		rayDir = XMVector3Normalize(rayDir);

		// If we hit the bounding box of the Mesh, then we might have picked a Mesh triangle,
		// so do the ray/triangle tests.
		//
		// If we did not hit the bounding box, then it is impossible that we hit 
		// the Mesh, so do not waste effort doing ray/triangle tests.

		float tdis = 0.0f;
		if (ri->Bounds.BoxBounds.Intersects(rayOrigin, rayDir, tdis))
		{
			assert(DirectX::Internal::XMVector3IsUnit(rayDir));
			if (ri->bIntersectBoundingOnly)
			{
				if (tdis < tmin0)
				{
					tmin0 = tdis;
					tname = ri->Name;
				}
			}
			else
			{
				// NOTE: For the demo, we know what to cast the vertex/index data to.  If we were mixing
				// m_offscreenRTFormats, some metadata would be needed to figure out what to cast it to.
				

				std::uint32_t* indices32 = (std::uint32_t*)ri->RenderData->IndexBufferCPU->GetBufferPointer();
				std::uint16_t* indices16 = (std::uint16_t*)ri->RenderData->IndexBufferCPU->GetBufferPointer();

				UINT triCount = ri->NumIndices / 3;

				// Find the nearest ray/triangle intersection.
				float tmin1 = std::numeric_limits<float>::max();
				for (UINT i = 0; i < triCount; ++i)
				{
					UINT i0;
					UINT i1;
					UINT i2;
					// Indices for this triangle.
					if (ri->RenderData->IndexFormat == DXGI_FORMAT_R32_UINT)
					{
						i0 = indices32[i * 3 + 0];
						i1 = indices32[i * 3 + 1];
						i2 = indices32[i * 3 + 2];
					}
					else
					{
						i0 = indices16[i * 3 + 0];
						i1 = indices16[i * 3 + 1];
						i2 = indices16[i * 3 + 2];
					}

					// Vertices for this triangle.
					XMVECTOR v0;
					XMVECTOR v1;
					XMVECTOR v2;					
					if (ri->RenderData->VertexFormat == VF_Vertex)
					{
						auto vertices = (Vertex*)ri->RenderData->VertexBufferCPU->GetBufferPointer();
						v0 = XMLoadFloat3(&vertices[i0].Position);
						v1 = XMLoadFloat3(&vertices[i1].Position);
						v2 = XMLoadFloat3(&vertices[i2].Position);
					}
					else if (ri->RenderData->VertexFormat == VF_ColorVertex)
					{
						auto vertices = (ColorVertex*)ri->RenderData->VertexBufferCPU->GetBufferPointer();
						v0 = XMLoadFloat3(&vertices[i0].Position);
						v1 = XMLoadFloat3(&vertices[i1].Position);
						v2 = XMLoadFloat3(&vertices[i2].Position);
					}
								
					// We have to iterate over all the triangles in order to find the nearest intersection.
					float t = 0.0f;
					if (TriangleTests::Intersects(rayOrigin, rayDir, v0, v1, v2, t))
					{
						if (t < tmin0)
						{
							tmin0 = t;
							tname = ri->Name;
						}
						break;
						if (t < tmin1)
						{
							// This is the new nearest picked triangle.
							tmin1 = t;
							UINT pickedTriangle = i;
						}
					}
				}
			}

		}
	}
	
	for (auto& ri : m_renderItemLayer[RenderLayer::Selectable])
	{
		if (ri->Name == tname)
		{
			if (!IsKeyDown(VK_CONTROL))
			{
				for (auto& _ri : m_renderItemLayer[RenderLayer::Selectable])
				{
					_ri->bSelected = false;
				}
			}
			ri->bSelected = true;		
			return;
		}
	}
}

LRESULT AppEntry::AppMessageProcessor(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return m_appGui->GUIMessageProcessor(hWnd, msg, wParam, lParam);
}

// Properties
void AppEntry::GetDefaultSize(int& width, int& height) const
{
    // TODO: Change to desired default window size (note minimum size is 320x200).
    width  = 1280;
    height = 720;
}

void AppEntry::GetCurrentSize(int& width, int& height) const
{
	width  = m_width;
	height = m_height;
}

float AppEntry::AspectRatio() const
{
	return static_cast<float>(m_width) / m_height;
}

#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void AppEntry::CreateDeviceDependentResources()
{
	auto device = m_deviceResources->GetD3DDevice();
	auto commandList = m_deviceResources->GetCommandList();

	Window window = { m_window, XMINT2(m_width, m_height) };
	m_appGui = std::make_unique<AppGUI>(this, window, device, commandList, m_deviceResources->GetBackBufferCount());	

	m_assimpImporter = std::make_unique<AssimpImporter>();
	m_textureImporter = std::make_unique<TextureImporter>();

	BuildDescriptorHeaps();
}

// Allocate all memory resources that change on a window SizeChanged event.
void AppEntry::CreateWindowSizeDependentResources()
{
    // TODO: Initialize windows-size dependent objects here.	
	m_offscreenRTFormats.fill(DXGI_FORMAT_R32G32B32A32_FLOAT);

	/////////////////////////////////////////////////////////
	// GBuffer.
	// Depth.
	m_deviceResources->CreateTex2DShaderResourceView(
		m_deviceResources->GetDepthStencil(), 
		GetCPUDescriptorHeapStartOffset(), 
		DXGI_FORMAT_R24_UNORM_X8_TYPELESS);	
	// Diffuse.
	m_offscreenRTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	// Normal.
	m_offscreenRTFormats[1] = DXGI_FORMAT_R11G11B10_FLOAT;	
	// ORM.
	m_offscreenRTFormats[2] = DXGI_FORMAT_R11G11B10_FLOAT;
	// Pos.
	m_offscreenRTFormats[3] = DXGI_FORMAT_R32G32B32A32_FLOAT;
	/////////////////////////////////////////////////////////

	std::array<D3D12_RESOURCE_STATES, m_maxGBuffers> defaultStates;
	defaultStates.fill(D3D12_RESOURCE_STATE_RENDER_TARGET);

	static bool bHasBeInit = false;
	if (!bHasBeInit)
	{	
		// Totally Five Textures BuiltIn GBuffer.
		std::array<const std::string, 5> GBufferNames = 
		{		
			"GBuffer_Diffuse",
			"GBuffer_Normal",
			"GBuffer_ORM",
			"GBuffer_Position",
			"GBuffer_Depth"
		};

		int index = 0;
		for (auto& format : m_offscreenRTFormats)
		{
			auto texture = std::make_unique<Texture>();
			texture->Index = -1;
			texture->Name = GBufferNames[index];
			texture->bIsBuiltIn = true;
			texture->bCanBeDeleted = false;
			texture->WPathName = L"JayouEngine__BuiltIn_Texture";
			texture->PathName = StringUtil::to_string(texture->WPathName);
			texture->HCPUDescriptor = GetCPUDescriptorHeapStartOffset(m_maxPreGBuffers + index);
			texture->HGPUDescriptor = GetGPUDescriptorHeapStartOffset(m_maxPreGBuffers + index);
			texture->Format = format;
			texture->Data = nullptr;
			texture->Width = m_width;
			texture->Height = m_height;

			GWorldCached(texture); index++;
		}

		auto texture = std::make_unique<Texture>();
		texture->Index = -1;
		texture->Name = GBufferNames[index++];
		texture->bIsBuiltIn = true;
		texture->bCanBeDeleted = false;
		texture->WPathName = L"JayouEngine__BuiltIn_Texture";
		texture->PathName = StringUtil::to_string(texture->WPathName);
		texture->HCPUDescriptor = GetCPUDescriptorHeapStartOffset();
		texture->HGPUDescriptor = GetGPUDescriptorHeapStartOffset();
		texture->Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		texture->Data = nullptr;
		texture->Width = m_width;
		texture->Height = m_height;

		GWorldCached(texture);

		Texture::Count = 0;

		bHasBeInit = true;
	}
	else
	{
		// Update GBuffer Texture Size Info.
		for (int i = 0; i < m_maxPreGBuffers + m_maxGBuffers; ++i)
		{
			m_allTextureRefs[i]->Width = m_width;
			m_allTextureRefs[i]->Height = m_height;
		}
	}

	m_deviceResources->CreateOffscreenRenderTargets(m_maxGBuffers, m_offscreenRTFormats.data(), defaultStates.data(), GetCPUDescriptorHeapStartOffset(m_maxPreGBuffers));
}

void AppEntry::CreateApplicationDependentResources()
{
	// ShowMap.
	m_deviceResources->CreateDsvDescriptorHeaps_AutoUpdate(1, []() {});
	m_shadowMap->BuildDescriptors(
		GetCPUDescriptorHeapStartOffset(1), // Front is Default Depth.
		GetGPUDescriptorHeapStartOffset(1),
		m_deviceResources->GetDepthStencilViewOffset(0));

	auto showMap = std::make_unique<Texture>();
	showMap->Index = -1;
	showMap->Name = "GBuffer_ShowMap";
	showMap->bIsBuiltIn = true;
	showMap->bCanBeDeleted = false;
	showMap->WPathName = L"JayouEngine__BuiltIn_Texture";
	showMap->PathName = StringUtil::to_string(showMap->WPathName);
	showMap->HCPUDescriptor = GetCPUDescriptorHeapStartOffset(1);
	showMap->HGPUDescriptor = GetGPUDescriptorHeapStartOffset(1);
	showMap->Format = m_shadowMap->GetFormat();
	showMap->Data = nullptr;
	showMap->Width = m_shadowMap->GetWidth();
	showMap->Height = m_shadowMap->GetHeight();

	GWorldCached(showMap);

	Texture::Count = 0;

#if 0
	// CubeMap.
	m_cubeMap->BuildDescriptors(
		GetCPUDescriptorHeapStartOffset(m_maxPreGBuffers + m_maxGBuffers + m_maxSupportTex2Ds),
		GetGPUDescriptorHeapStartOffset(m_maxPreGBuffers + m_maxGBuffers + m_maxSupportTex2Ds),
		m_deviceResources->GetCubeMapRenderTargetView());
#endif

	// Default Texture.
	auto texture = std::make_unique<Texture>();
	texture->Name = "Default";
	texture->bCanBeDeleted = false;
	texture->PathName = "JayouEngine_BuiltIn";
	texture->HCPUDescriptor = GetCPUDescriptorHeapStartOffset(m_maxPreGBuffers + m_maxGBuffers);
	texture->HGPUDescriptor = GetGPUDescriptorHeapStartOffset(m_maxPreGBuffers + m_maxGBuffers);

	m_textureImporter->CreateDefaultTexture(texture.get(), 128, 128);

	m_deviceResources->ExecuteCommandLists([&]()
	{
		m_deviceResources->CreateTexture2D(texture.get());
	});

	GWorldCached(texture);

	for (uint32 i = 0; i < m_deviceResources->GetBackBufferCount(); ++i)
	{
		m_frameResources.push_back(std::make_unique<FrameResource>(m_deviceResources->GetD3DDevice()));
		m_frameResources[i]->ResizeBuffer<PassConstant>(3); // Main & Shadow Pass & CubeMap.
	}

	// Default Material & Light.
	{
		MaterialDesc matDesc;
		matDesc.bCanbeDeleted = false;
		AddMaterial(matDesc, false);

		LightDesc litDesc;
		litDesc.bCanbeDeleted = false;
		AddLight(litDesc);
	}

	// Default RenderItem.
	m_deviceResources->ExecuteCommandLists([&]()
	{	
		BuildRenderItems();		
	});

	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildPSO();
}

void AppEntry::BuildDescriptorHeaps()
{
	m_deviceResources->CreateCommonDescriptorHeap(m_appGui->GetDescriptorCount() + 
		m_maxPreGBuffers + m_maxGBuffers + m_maxSupportTex2Ds + m_maxCubeMapSRVs, &m_srvCbvDescHeap);
	m_appGui->SetSrvDescHeap(m_srvCbvDescHeap.Get());
}

void AppEntry::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable[3];
	// GBuffer & Offscreen RT.
	texTable[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, m_maxPreGBuffers + m_maxGBuffers, 0, 2);
	// Common 2D Textures.
	texTable[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, m_maxSupportTex2Ds, 0, 3);
	// Cube Maps.
	texTable[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, m_maxCubeMapSRVs, 0, 4);

	const unsigned int NUM_ROOTPARAMETER = 7;
	CD3DX12_ROOT_PARAMETER slotRootParameter[NUM_ROOTPARAMETER];

	// Per Object.
	slotRootParameter[0].InitAsConstantBufferView(0);
	// Per Pass.
	slotRootParameter[1].InitAsConstantBufferView(1);	
	// Light.
	slotRootParameter[2].InitAsShaderResourceView(0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
	// Materail.
	slotRootParameter[3].InitAsShaderResourceView(0, 1, D3D12_SHADER_VISIBILITY_ALL);
	// GBuffer & Offscreen RT.
	slotRootParameter[4].InitAsDescriptorTable(1, &texTable[0], D3D12_SHADER_VISIBILITY_PIXEL);
	// 2D Textures
	slotRootParameter[5].InitAsDescriptorTable(1, &texTable[1], D3D12_SHADER_VISIBILITY_PIXEL);
	// Cube Maps.
	slotRootParameter[6].InitAsDescriptorTable(1, &texTable[2], D3D12_SHADER_VISIBILITY_PIXEL);

	auto staticSamplers = m_deviceResources->GetAllStaticSamplers();

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(NUM_ROOTPARAMETER, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	m_deviceResources->CreateRootSignature(&rootSigDesc, &m_rootSIGs["Main"]);
}

void AppEntry::BuildShadersAndInputLayout()
{
	std::string NumTextures = std::to_string(m_maxSupportTex2Ds);
	std::string NumLights = std::to_string(MaxLights);
	const D3D_SHADER_MACRO defines[] =
	{
		NameOf(NumTextures), NumTextures.c_str(),
		NameOf(NumLights), NumLights.c_str(),
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	m_shaderByteCode["ColorVS"] = WinUtility::CompileShader(m_appPath + L"../JayouEngine/Core/Shaders/VertexColor.hlsl", defines, "VS", "vs_5_1");
	m_shaderByteCode["WireframeVS"] = WinUtility::CompileShader(m_appPath + L"../JayouEngine/Core/Shaders/VertexColor.hlsl", defines, "WireframeVS", "vs_5_1");
	m_shaderByteCode["ColorPS"] = WinUtility::CompileShader(m_appPath + L"../JayouEngine/Core/Shaders/VertexColor.hlsl", defines, "PS", "ps_5_1");

	m_shaderByteCode["GBufferVS"] = WinUtility::CompileShader(m_appPath + L"../JayouEngine/Core/Shaders/GBuffer.hlsl", defines, "VS", "vs_5_1");
	m_shaderByteCode["GBufferPS"] = WinUtility::CompileShader(m_appPath + L"../JayouEngine/Core/Shaders/GBuffer.hlsl", defines, "PS", "ps_5_1");

	m_shaderByteCode["PBRVS"] = WinUtility::CompileShader(m_appPath + L"../JayouEngine/Core/Shaders/PBR.hlsl", defines, "VS", "vs_5_1");
	m_shaderByteCode["PBRPS"] = WinUtility::CompileShader(m_appPath + L"../JayouEngine/Core/Shaders/PBR.hlsl", defines, "PS", "ps_5_1");

	m_shaderByteCode["FullSQuadVS"] = WinUtility::CompileShader(m_appPath + L"../JayouEngine/Core/Shaders/FullscreenQuad.hlsl", defines, "VS", "vs_5_1");
	m_shaderByteCode["FullSQuadPS"] = WinUtility::CompileShader(m_appPath + L"../JayouEngine/Core/Shaders/FullscreenQuad.hlsl", defines, "PS", "ps_5_1");

	m_shaderByteCode["ShadowVS"] = WinUtility::CompileShader(m_appPath + L"../JayouEngine/Core/Shaders/Shadow.hlsl", defines, "VS", "vs_5_1");
	m_shaderByteCode["ShadowPS"] = WinUtility::CompileShader(m_appPath + L"../JayouEngine/Core/Shaders/Shadow.hlsl", defines, "PS", "ps_5_1");

	m_shaderByteCode["SkySphereVS"] = WinUtility::CompileShader(m_appPath + L"../JayouEngine/Core/Shaders/SkySphere.hlsl", defines, "VS", "vs_5_1");
	m_shaderByteCode["SkySpherePS"] = WinUtility::CompileShader(m_appPath + L"../JayouEngine/Core/Shaders/SkySphere.hlsl", defines, "PS", "ps_5_1");

	std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout =
	{
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 52, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	m_inputLayout["BaseShaderInputLayout"] = inputLayout;
}

void AppEntry::BuildRenderItems()
{
	GeometryData<ColorVertex> gridMesh = GeometryCreator::CreateLineGrid(
		m_appGui->GetAppData()->GridWidth,
		m_appGui->GetAppData()->GridWidth,
		m_appGui->GetAppData()->GridUnit,
		m_appGui->GetAppData()->GridUnit);

	auto gridRItem = std::make_unique<RenderItem>();
	gridRItem->Name = "grid";
	gridRItem->PathName = "JayouEngine_BuiltIn_Geometry";
	gridRItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
	gridRItem->NumVertices = (uint32)gridMesh.Vertices.size();
	gridRItem->NumIndices = (uint32)gridMesh.Indices32.size();
	gridRItem->bIsBuiltIn = true;
	gridRItem->Bounds = gridMesh.CalcBounds();

	m_deviceResources->CreateCommonGeometry<ColorVertex, uint16>(gridRItem.get(), gridMesh.Vertices, gridMesh.GetIndices16());

	GWorldCached(gridRItem, RenderLayer::Line, false);

	// SkySphere.
	GeometryData<Vertex> skyMesh = GeometryCreator::CreateGeosphere(1.0f, 5);

	auto skyRIem = std::make_unique<RenderItem>();
	skyRIem->Name = "SkySphere";
	skyRIem->PathName = "JayouEngine_BuiltIn_Geometry";
	skyRIem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skyRIem->NumVertices = (uint32)skyMesh.Vertices.size();
	skyRIem->NumIndices = (uint32)skyMesh.Indices32.size();
	skyRIem->bIsBuiltIn = true;
	skyRIem->TransFormMatrix = Matrix4::MakeScale(5000.0f);

	m_deviceResources->CreateCommonGeometry<Vertex, uint32>(skyRIem.get(), skyMesh.Vertices, skyMesh.Indices32);

	GWorldCached(skyRIem, RenderLayer::SkySphere, false);

	for (uint32 i = 0; i < m_deviceResources->GetBackBufferCount(); ++i)
	{
		m_frameResources[i]->ResizeBuffer<ObjectConstant>((uint32)m_allRItems.size());
	}
}

void AppEntry::BuildPSO()
{
	//////////////////////////////////////////////////////////////////////////
	// PSO Default.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC defaultPSO = m_deviceResources->CreateCommonPSO(
		m_inputLayout["BaseShaderInputLayout"],
		m_rootSIGs["Main"].Get(),
		m_shaderByteCode["ColorVS"].Get(),
		m_shaderByteCode["ColorPS"].Get(),
		&m_PSOs["Default"]);
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// PSO Line.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC linePSO = defaultPSO;
	linePSO.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	m_deviceResources->CreateGraphicsPipelineState(&linePSO, &m_PSOs["Line"]);
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// PSO Wireframe.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC wireframePSO = linePSO;
	wireframePSO.RasterizerState.AntialiasedLineEnable = true;
	wireframePSO.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	wireframePSO.VS = CD3DX12_SHADER_BYTECODE(m_shaderByteCode["WireframeVS"].Get());
	m_deviceResources->CreateGraphicsPipelineState(&wireframePSO, &m_PSOs["Wireframe"]);
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// PSO FullscreenQuad / BackGround.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC fullQuadPSO = defaultPSO;
	fullQuadPSO.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	fullQuadPSO.VS = CD3DX12_SHADER_BYTECODE(m_shaderByteCode["FullSQuadVS"].Get());
	fullQuadPSO.PS = CD3DX12_SHADER_BYTECODE(m_shaderByteCode["FullSQuadPS"].Get());
	m_deviceResources->CreateGraphicsPipelineState(&fullQuadPSO, &m_PSOs["FullscreenQuad"]);
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// PSO GBuffer.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC GBufferPSO = defaultPSO;
	GBufferPSO.NumRenderTargets = m_maxGBuffers;
	for (uint32 i = 0; i < m_maxGBuffers; ++i)
		GBufferPSO.RTVFormats[i] = m_offscreenRTFormats[i];
	GBufferPSO.VS = CD3DX12_SHADER_BYTECODE(m_shaderByteCode["GBufferVS"].Get());
	GBufferPSO.PS = CD3DX12_SHADER_BYTECODE(m_shaderByteCode["GBufferPS"].Get());
	GBufferPSO.SampleDesc.Count = 1;
	GBufferPSO.SampleDesc.Quality = 0;
	m_deviceResources->CreateGraphicsPipelineState(&GBufferPSO, &m_PSOs["GBuffer"]);
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// PSO PBR
	D3D12_GRAPHICS_PIPELINE_STATE_DESC PBRPSO = defaultPSO;
	PBRPSO.DSVFormat = DXGI_FORMAT_UNKNOWN;
	PBRPSO.VS = CD3DX12_SHADER_BYTECODE(m_shaderByteCode["PBRVS"].Get());
	PBRPSO.PS = CD3DX12_SHADER_BYTECODE(m_shaderByteCode["PBRPS"].Get());
	m_deviceResources->CreateGraphicsPipelineState(&PBRPSO, &m_PSOs["PBR"]);
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// PSO Shadow Pass.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC ShadowPSO = defaultPSO;
	ShadowPSO.RasterizerState.DepthBias = 100000;
	ShadowPSO.RasterizerState.DepthBiasClamp = 0.0f;
	ShadowPSO.RasterizerState.SlopeScaledDepthBias = 1.0f;
	ShadowPSO.VS = CD3DX12_SHADER_BYTECODE(m_shaderByteCode["ShadowVS"].Get());
	ShadowPSO.PS = CD3DX12_SHADER_BYTECODE(m_shaderByteCode["ShadowPS"].Get());
	// Shadow map pass does not have a render target.
	ShadowPSO.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	ShadowPSO.NumRenderTargets = 0;
	m_deviceResources->CreateGraphicsPipelineState(&ShadowPSO, &m_PSOs["Shadow"]);
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// PSO CubeMap.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC SkySpherePSO = defaultPSO;
	SkySpherePSO.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
	SkySpherePSO.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	SkySpherePSO.VS = CD3DX12_SHADER_BYTECODE(m_shaderByteCode["SkySphereVS"].Get());
	SkySpherePSO.PS = CD3DX12_SHADER_BYTECODE(m_shaderByteCode["SkySpherePS"].Get());
	m_deviceResources->CreateGraphicsPipelineState(&SkySpherePSO, &m_PSOs["SkySphere"]);
	//////////////////////////////////////////////////////////////////////////
}

#pragma endregion

#pragma region Interface IDeviceNotify
// IDeviceNotify
void AppEntry::OnDeviceLost()
{
    // TODO: Add Direct3D resource cleanup here.
	m_srvCbvDescHeap.Reset();
	for (auto& e : m_rootSIGs)
		e.second.Reset();
	for (auto& e : m_PSOs)
		e.second.Reset();
	for (auto& e : m_shaderByteCode)
		e.second.Reset();
}

void AppEntry::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}

void AppEntry::UpdateGUI()
{
	m_appGui->UpdateGUI();
}

void AppEntry::RenderGUI()
{
	m_appGui->RenderGUI();
}

void AppEntry::PostProcessing()
{

}

void AppEntry::OnOptionsChanged()
{
	BuildPSO();
}

void AppEntry::ParseCommandLine(std::wstring cmdLine)
{

}

#pragma endregion

void GWorld::GWorldCached(std::unique_ptr<Texture>& InTexture)
{
	m_allTextureRefs.push_back(InTexture.get());
	m_allTextures[InTexture->Name] = std::move(InTexture);
}

void GWorld::GWorldCached(std::unique_ptr<Material>& InMaterial)
{
	m_allMaterialRefs.push_back(InMaterial.get());
	m_allMaterials[InMaterial->Name] = std::move(InMaterial);
}

void GWorld::GWorldCached(std::unique_ptr<RenderItem>& InRenderItem, const RenderLayer& InRenderLayer, bool bIsSelectable)
{
	if (bIsSelectable)
	{
		m_renderItemLayer[RenderLayer::Selectable].push_back(InRenderItem.get());
	}
	m_renderItemLayer[InRenderLayer].push_back(InRenderItem.get());
	m_renderItemLayer[RenderLayer::All].push_back(InRenderItem.get());
	m_allRItems[InRenderItem->Name] = std::move(InRenderItem);	
}

void GWorld::GWorldCached(std::unique_ptr<Light>& InLight)
{
	m_allLightRefs.push_back(InLight.get());
	m_allLights[InLight->Name] = std::move(InLight);
}

void GWorld::AddRenderItem(const BuiltInGeoDesc& InGeoDesc)
{
	m_deviceResources->ExecuteCommandLists([&]()
	{
		auto builtInRItem = std::make_unique<RenderItem>();
		builtInRItem->Name = InGeoDesc.Name;

		for (auto ri : m_renderItemLayer[RenderLayer::All])
			if (builtInRItem->Name == ri->Name)
				builtInRItem->Name = ri->Name + "_" + std::to_string(builtInRItem->Index);

		GeometryData<Vertex> builtInMesh;
		switch (InGeoDesc.GeoType)
		{
		case BG_Box:
			builtInMesh = GeometryCreator::CreateBox(InGeoDesc.Width, InGeoDesc.Height, InGeoDesc.Depth, InGeoDesc.NumSubdivisions);
			break;
		case BG_Sphere_1:
			builtInMesh = GeometryCreator::CreateSphere(InGeoDesc.Radius, InGeoDesc.SliceCount, InGeoDesc.StackCount);
			break;
		case BG_Sphere_2:
			builtInMesh = GeometryCreator::CreateGeosphere(InGeoDesc.Radius, InGeoDesc.NumSubdivisions);
			break;
		case BG_Cylinder:
			builtInMesh = GeometryCreator::CreateCylinder(InGeoDesc.BottomRadius, InGeoDesc.TopRadius, InGeoDesc.Height, InGeoDesc.SliceCount, InGeoDesc.StackCount);
			break;
		case BG_Plane:
			builtInMesh = GeometryCreator::CreatePlane(InGeoDesc.Width, InGeoDesc.Depth, InGeoDesc.SliceCount, InGeoDesc.StackCount);
			break;
		default:
			break;
		}
		builtInMesh.SetColor(InGeoDesc.Color);

		builtInRItem->PathName = InGeoDesc.PathName;
		builtInRItem->bIsBuiltIn = true;
		builtInRItem->CachedBuiltInGeoDesc = InGeoDesc;
		builtInRItem->SetTransFormMatrix(InGeoDesc.Translation, InGeoDesc.Rotation, InGeoDesc.Scale);
		builtInRItem->NumVertices = (uint32)builtInMesh.Vertices.size();
		builtInRItem->NumIndices = (uint32)builtInMesh.Indices32.size();
		builtInRItem->Bounds = builtInMesh.CalcBounds();
		builtInRItem->VertexColor = InGeoDesc.Color;
		builtInRItem->CachedGeometryData = builtInMesh;

		m_deviceResources->CreateCommonGeometry<Vertex, uint32>(builtInRItem.get(), builtInMesh.Vertices, builtInMesh.Indices32);

		GWorldCached(builtInRItem, RenderLayer::Opaque);
	});
	
	for (auto& ri : m_allRItems)
	{
		ri.second->MarkAsDirty();
	}
	for (uint32 i = 0; i < m_deviceResources->GetBackBufferCount(); ++i)
	{
		m_frameResources[i]->ResizeBuffer<ObjectConstant>((uint32)m_allRItems.size());
	}
}

void GWorld::AddRenderItem(const ImportGeoDesc& InGeoDesc)
{
	m_deviceResources->ExecuteCommandLists([&]()
	{
		if (m_assimpImporter->Import(InGeoDesc))
		{
			const std::unordered_map<std::string, Geometry>& geos = m_assimpImporter->GetAllGeometries();
			for (auto pair : geos)
			{
				auto importRItem = std::make_unique<RenderItem>();
				importRItem->Name = InGeoDesc.Name;

				for (auto ri : m_renderItemLayer[RenderLayer::All])
					if (importRItem->Name == ri->Name)
						importRItem->Name = ri->Name + "_" + std::to_string(importRItem->Index);

				Geometry geo = pair.second;
				geo.SetColor(InGeoDesc.Color);
				GeometryData<Vertex> importMesh = geo.Data;

				importRItem->PathName = InGeoDesc.PathName;
				importRItem->SetTransFormMatrix(InGeoDesc.Translation, InGeoDesc.Rotation, InGeoDesc.Scale);
				importRItem->NumVertices = (uint32)importMesh.Vertices.size();
				importRItem->NumIndices = (uint32)importMesh.Indices32.size();
				importRItem->Bounds = geo.Bounds;
				importRItem->VertexColor = InGeoDesc.Color;
				importRItem->CachedGeometryData = importMesh;

				m_deviceResources->CreateCommonGeometry<Vertex, uint32>(importRItem.get(), importMesh.Vertices, importMesh.Indices32);

				GWorldCached(importRItem, RenderLayer::Opaque);
			}
		}
	});
	
	for (auto& ri : m_allRItems)
	{
		ri.second->MarkAsDirty();
	}
	for (uint32 i = 0; i < m_deviceResources->GetBackBufferCount(); ++i)
	{
		m_frameResources[i]->ResizeBuffer<ObjectConstant>((uint32)m_allRItems.size());
	}
}

void GWorld::AddTexture2D(const ImportTexDesc& InTexDesc)
{
	m_deviceResources->ExecuteCommandLists([&]()
	{
		auto texture = std::make_unique<Texture>();
		texture->Name = InTexDesc.Name;

		for (auto& tex : m_allTextureRefs)
			if (texture->Name == tex->Name)
				texture->Name = tex->Name + "_" + std::to_string(texture->Index);

		texture->PathName = InTexDesc.PathName;
		texture->HCPUDescriptor = GetCPUDescriptorHeapStartOffset((uint32)m_allTextureRefs.size());
		texture->HGPUDescriptor = GetGPUDescriptorHeapStartOffset((uint32)m_allTextureRefs.size());
		texture->bIsHDR = InTexDesc.bIsHDR;

		m_textureImporter->LoadTexture(texture.get());
		m_deviceResources->CreateTexture2D(texture.get());
		GWorldCached(texture);
	});
}

void GWorld::AddMaterial(const MaterialDesc& InMaterialDesc, bool bUseTexture /*= true*/)
{
	auto material = std::make_unique<Material>();
	material->Name = InMaterialDesc.Name;

	for (auto& mat : m_allMaterialRefs)
		if (material->Name == mat->Name)
			material->Name = mat->Name + "_" + std::to_string(material->Index);

	material->PathName = InMaterialDesc.PathName;
	material->bCanBeDeleted = InMaterialDesc.bCanbeDeleted;
	material->DiffuseAlbedo = InMaterialDesc.DiffuseAlbedo;
	material->SetTransFormMatrix(InMaterialDesc.Translation, InMaterialDesc.Rotation, InMaterialDesc.Scale);
	material->Roughness = InMaterialDesc.Roughness;
	material->Metallicity = InMaterialDesc.Metallicity;
	if (bUseTexture)
	{
		material->DiffuseMapIndex = InMaterialDesc.DiffuseMapName != "" ? m_allTextures[InMaterialDesc.DiffuseMapName]->Index : -1;
		material->NormalMapIndex = InMaterialDesc.NormalMapName != "" ? m_allTextures[InMaterialDesc.NormalMapName]->Index : -1;
		material->ORMMapIndex = InMaterialDesc.ORMMapName != "" ? m_allTextures[InMaterialDesc.ORMMapName]->Index : -1;

		material->DiffuseMapComboIndex = InMaterialDesc.DiffuseMapComboIndex;
		material->NormalMapComboIndex = InMaterialDesc.NormalMapComboIndex;
		material->ORMMapComboIndex = InMaterialDesc.ORMMapComboIndex;
	}
	material->bUseTexture = bUseTexture;

	GWorldCached(material);

	m_deviceResources->WaitForGpu();
	
	for (auto& mat : m_allMaterialRefs)
	{
		mat->MarkAsDirty();
	}
	for (uint32 i = 0; i < m_deviceResources->GetBackBufferCount(); ++i)
	{
		m_frameResources[i]->ResizeBuffer<MaterialData>((uint32)m_allMaterialRefs.size());
	}
}

void GWorld::AddLight(const LightDesc& InLightDesc)
{
	auto light = std::make_unique<Light>();
	light->Name = InLightDesc.Name;

	for (auto& lit : m_allLightRefs)
		if (light->Name == lit->Name)
			light->Name = lit->Name + "_" + std::to_string(light->Index);

	light->PathName = InLightDesc.PathName;
	light->bCanBeDeleted = InLightDesc.bCanbeDeleted;
	light->SetTransFormMatrix(InLightDesc.Translation, InLightDesc.Rotation, InLightDesc.Scale);
	light->LightType = InLightDesc.LightType;
	light->Strength = InLightDesc.Strength;
	light->FalloffStart = InLightDesc.FalloffStart;
	light->FalloffEnd = InLightDesc.FalloffEnd;
	light->SpotPower = InLightDesc.SpotPower;

	switch (InLightDesc.LightType)
	{
	case LT_Directional:
		m_numDirLights++;
		break;
	case LT_Point:
		m_numPointLights++;
		break;
	case LT_Spot:
		m_numSpotLights++;
		break;
	default:
		break;
	}

	GWorldCached(light);

	m_deviceResources->WaitForGpu();

	for (auto& lit : m_allLightRefs)
	{
		lit->MarkAsDirty();
	}
	for (uint32 i = 0; i < m_deviceResources->GetBackBufferCount(); ++i)
	{
		m_frameResources[i]->ResizeBuffer<LightData>((uint32)m_allLightRefs.size());
	}
}

void GWorld::GarbageCollection()
{
	// This Function Should Not Be Called Every Frame!!!

	bool bHasAnyItemsToDelete = false;
	bool bResizeObjCBNeeded = false;
	bool bResizeMatSBNeeded = false;
	bool bResizeLitSBNeeded = false;

	// First to Find Any Items that MarkAsDeleted, And Reset Index.
	// One BuiltIn Mesh: Grid.
	int32 index = 1;
	std::map<int32, std::string> deletedItemIDNames[4];	
	for (uint32 i = 1; i < m_renderItemLayer[RenderLayer::All].size(); ++i)
	{
		if (m_renderItemLayer[RenderLayer::All][i]->CanbeDeletedNow())
		{
			deletedItemIDNames[0][m_renderItemLayer[RenderLayer::All][i]->Index] = m_renderItemLayer[RenderLayer::All][i]->Name;
			m_renderItemLayer[RenderLayer::All].erase(m_renderItemLayer[RenderLayer::All].begin() + index);		
			bHasAnyItemsToDelete = true;
			bResizeObjCBNeeded = true;
			--i;
			continue;
		}
		m_renderItemLayer[RenderLayer::All][i]->Index = index++;
	}

	for (uint32 i = 0; i < (uint32)RenderLayer::All; ++i)
	{
		for (uint32 j = 0; j < (uint32)m_renderItemLayer[i].size(); ++j)
		{
			if (m_renderItemLayer[i][j]->CanbeDeletedNow())
			{
				m_renderItemLayer[i].erase(m_renderItemLayer[i].begin() + j);
				--j;
			}
		}
	}

	// One BuiltIn DirLight.
	index = 1;	
	for (uint32 i = 1; i < m_allLightRefs.size(); ++i)
	{
		if (m_allLightRefs[i]->CanbeDeletedNow())
		{
			deletedItemIDNames[1][m_allLightRefs[i]->Index] = m_allLightRefs[i]->Name;
			m_allLightRefs.erase(m_allLightRefs.begin() + index);
			bHasAnyItemsToDelete = true;
			bResizeLitSBNeeded = true;
			--i;
			continue;
		}
		m_allLightRefs[i]->Index = index++;
	}

	// One BuiltIn Default Material.
	index = 1;	
	for (uint32 i = 1; i < m_allMaterialRefs.size(); ++i)
	{
		if (m_allMaterialRefs[i]->CanbeDeletedNow())
		{
			deletedItemIDNames[2][m_allMaterialRefs[i]->Index] = m_allMaterialRefs[i]->Name;
			m_allMaterialRefs.erase(m_allMaterialRefs.begin() + index);
			bHasAnyItemsToDelete = true;
			bResizeMatSBNeeded = true;
			--i;
			continue;
		}
		m_allMaterialRefs[i]->Index = index++;
	}

	// BuiltIn GBuffers And One Texture.
	index = 1;	
	for (uint32 i = m_maxPreGBuffers + m_maxGBuffers + 1; i < m_allTextureRefs.size(); ++i)
	{
		if (m_allTextureRefs[i]->CanbeDeletedNow())
		{
			deletedItemIDNames[3][m_allTextureRefs[i]->Index] = m_allTextureRefs[i]->Name;
			m_allTextureRefs.erase(m_allTextureRefs.begin() + index + m_maxPreGBuffers + m_maxGBuffers);
			bHasAnyItemsToDelete = true;
			--i;
			continue;
		}
		index++;
	}

	// Now Perform Truely Delete.
	if (bHasAnyItemsToDelete)
	{
		m_deviceResources->WaitForGpu();
	}

	// Delete RenderItems.
	for (auto& id_name : deletedItemIDNames[0])
	{
		m_allRItems.erase(id_name.second);
	}
	RenderItem::Count = (uint32)m_allRItems.size();

	// Delete Lights.
	for (auto& id_name : deletedItemIDNames[1])
	{
		m_allLights.erase(id_name.second);
	}
	Light::Count = (uint32)m_allLights.size();

	// Delete Materials.	
	for (auto& id_name : deletedItemIDNames[2])
	{
		// First to Find All References.
		for (auto& ri : m_allRItems)
		{
			if (ri.second->MaterialIndex == id_name.first)
			{
				ri.second->MaterialIndex = 0;
				ri.second->MarkAsDirty();
			}
		}

		m_allMaterials.erase(id_name.second);
	}
	Material::Count = (uint32)m_allMaterials.size();

	// Delete Textures.
	for (auto& id_name : deletedItemIDNames[3])
	{
		int32 texID = id_name.first;
		// First to Find All References.
		for (auto& mat : m_allMaterials)
		{
			if (mat.second->DiffuseMapIndex == texID)
			{
				mat.second->DiffuseMapIndex = -1;
				mat.second->DiffuseMapComboIndex = -1;
				mat.second->MarkAsDirty();
			}
			if (mat.second->NormalMapIndex == texID)
			{
				mat.second->NormalMapIndex = -1;
				mat.second->NormalMapComboIndex = -1;
				mat.second->MarkAsDirty();
			}
			if (mat.second->ORMMapIndex == texID)
			{
				mat.second->ORMMapIndex = -1;
				mat.second->ORMMapComboIndex = -1;
				mat.second->MarkAsDirty();
			}
			mat.second->DiffuseMapComboIndex--;
			mat.second->NormalMapComboIndex--;
			mat.second->ORMMapComboIndex--;
		}

		if (m_appGui->GetAppData()->CubeMapIndex == texID)
		{
			m_appGui->GetAppData()->CubeMapIndex = -1;
			m_appGui->GetAppData()->CubeMapComboIndex = -1;
		}
		m_appGui->GetAppData()->CubeMapComboIndex--;	

		m_allTextures.erase(id_name.second);
	}
	Texture::Count = (uint32)m_allTextures.size() - m_maxPreGBuffers - m_maxGBuffers;

	// Finally Resize Buffers if Needed.
	if (bHasAnyItemsToDelete && bResizeObjCBNeeded)
	{		
		for (auto& ri : m_allRItems)
		{
			ri.second->MarkAsDirty();
		}
		for (uint32 i = 0; i < m_deviceResources->GetBackBufferCount(); ++i)
		{
			m_frameResources[i]->ResizeBuffer<ObjectConstant>((uint32)m_allRItems.size());
		}
	}
	if (bHasAnyItemsToDelete && bResizeMatSBNeeded)
	{
		for (auto& mat : m_allMaterials)
		{
			mat.second->MarkAsDirty();
		}
		for (uint32 i = 0; i < m_deviceResources->GetBackBufferCount(); ++i)
		{
			m_frameResources[i]->ResizeBuffer<MaterialData>((uint32)m_allMaterials.size());
		}
	}
	if (bHasAnyItemsToDelete && bResizeLitSBNeeded)
	{
		for (auto& lit : m_allLights)
		{
			lit.second->MarkAsDirty();
		}
		for (uint32 i = 0; i < m_deviceResources->GetBackBufferCount(); ++i)
		{
			m_frameResources[i]->ResizeBuffer<LightData>((uint32)m_allLights.size());
		}
	}

}

void GWorld::HandleRebuildRenderItem()
{
	for (auto& ri : m_renderItemLayer[RenderLayer::All])
	{
		if (ri->bNeedRebuild == true)
		{
			ri->bNeedRebuild = false;
			
			// Rebuild RenderItem.
			m_deviceResources->ExecuteCommandLists([&]()
			{
				if (ri->bIsBuiltIn)
				{
					BuiltInGeoDesc InGeoDesc = ri->CachedBuiltInGeoDesc;

					GeometryData<Vertex> builtInMesh;
					switch (InGeoDesc.GeoType)
					{
					case BG_Box:
						builtInMesh = GeometryCreator::CreateBox(InGeoDesc.Width, InGeoDesc.Height, InGeoDesc.Depth, InGeoDesc.NumSubdivisions);
						break;
					case BG_Sphere_1:
						builtInMesh = GeometryCreator::CreateSphere(InGeoDesc.Radius, InGeoDesc.SliceCount, InGeoDesc.StackCount);
						break;
					case BG_Sphere_2:
						builtInMesh = GeometryCreator::CreateGeosphere(InGeoDesc.Radius, InGeoDesc.NumSubdivisions);
						break;
					case BG_Cylinder:
						builtInMesh = GeometryCreator::CreateCylinder(InGeoDesc.BottomRadius, InGeoDesc.TopRadius, InGeoDesc.Height, InGeoDesc.SliceCount, InGeoDesc.StackCount);
						break;
					case BG_Plane:
						builtInMesh = GeometryCreator::CreatePlane(InGeoDesc.Width, InGeoDesc.Depth, InGeoDesc.SliceCount, InGeoDesc.StackCount);
						break;
					default:
						break;
					}
					builtInMesh.SetColor(InGeoDesc.Color);

					// Dynamic Create Resource Need WaitForGpu.
					m_deviceResources->WaitForGpu();

					ri->CachedGeometryData = builtInMesh;
					ri->NumVertices = (uint32)builtInMesh.Vertices.size();
					ri->NumIndices = (uint32)builtInMesh.Indices32.size();
					m_deviceResources->CreateCommonGeometry<Vertex, uint32>(ri, builtInMesh.Vertices, builtInMesh.Indices32);
				}
				else
				{
					m_deviceResources->WaitForGpu();
					ri->CachedGeometryData.SetColor(ri->VertexColor);
					m_deviceResources->CreateCommonGeometry<Vertex, uint32>(ri, ri->CachedGeometryData.Vertices, ri->CachedGeometryData.Indices32);
				}
			});
		}
	}
}

void GWorld::HandleRenderItemStateChanged()
{
	m_renderItemLayer[RenderLayer::Selected].clear();
	for (auto& ri : m_renderItemLayer[RenderLayer::Selectable])
	{
		if (ri->bSelected == true)
		{
			m_renderItemLayer[RenderLayer::Selected].push_back(ri);
		}
	}
}

CD3DX12_CPU_DESCRIPTOR_HANDLE GWorld::GetCPUDescriptorHeapStartOffset(uint32 InOffset /*= 0*/)
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		m_srvCbvDescHeap->GetCPUDescriptorHandleForHeapStart(),
		m_appGui->GetDescriptorCount() + InOffset,
		m_deviceResources->GetCbvSrvUavDescriptorSize());
}

CD3DX12_GPU_DESCRIPTOR_HANDLE GWorld::GetGPUDescriptorHeapStartOffset(uint32 InOffset /*= 0*/)
{
	return CD3DX12_GPU_DESCRIPTOR_HANDLE(
		m_srvCbvDescHeap->GetGPUDescriptorHandleForHeapStart(),
		m_appGui->GetDescriptorCount() + InOffset,
		m_deviceResources->GetCbvSrvUavDescriptorSize());
}
