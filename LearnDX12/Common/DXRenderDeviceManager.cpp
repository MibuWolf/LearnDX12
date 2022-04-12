#include <WindowsX.h>
#include <DirectXColors.h>
#include "DXRenderDeviceManager.h"



DXRenderDeviceManager& DXRenderDeviceManager::GetInstance()
{
	static DXRenderDeviceManager sInstance;
	return sInstance;
}


bool DXRenderDeviceManager::InitD3DDevice(HWND	hwnd)
{
	MainWinHWND = hwnd;

	if (!InitD3DDevice())
		return FALSE;

	if (!CheckDeviceBaseInfo())
		return FALSE;

	CreateCommondQueue();

	CreateSwapChain();

	CreateDescriptorHeap();

	OnResize();

	return TRUE;
}

void DXRenderDeviceManager::OnResize()
{
	assert(D3DDevice);
	assert(SwapChain);
	assert(CmdListAlloc);

	// Flush before changing any resources.
	FlushCommandQueue();

	ThrowIfFailed(CommandList->Reset(CmdListAlloc.Get(), nullptr));

	// Release the previous resources we will be recreating.
	for (int i = 0; i < SWAPCHAINBUFFERCOUNT; ++i)
		BackgroundBuffer[i].Reset();
	DepthStencilBuffer.Reset();

	// Resize the swap chain.
	ThrowIfFailed(SwapChain->ResizeBuffers(
		SWAPCHAINBUFFERCOUNT,
		BackBufferWidth, BackBufferHeight,
		BACKBUFFER_FORMAT,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

	//mCurrBackBuffer = 0;

	// 重新创建后台缓冲区 深度缓冲区描述符
	CreateBufferDescriptor();

	// 重置命令列表和命令队列
	ThrowIfFailed(CommandList->Close());
	ID3D12CommandList* cmdsLists[] = { CommandList.Get() };
	CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until resize is complete.
	FlushCommandQueue();

	// 更新视口覆盖区域
	ScreenViewport.TopLeftX = 0;
	ScreenViewport.TopLeftY = 0;
	ScreenViewport.Width = static_cast<float>(BackBufferWidth);
	ScreenViewport.Height = static_cast<float>(BackBufferWidth);
	ScreenViewport.MinDepth = 0.0f;
	ScreenViewport.MaxDepth = 1.0f;

	// 设置视口裁剪矩形，在此矩形外的像素都会被剔除不会写入后台缓冲区
	ScissorRect = { 0, 0, BackBufferWidth, BackBufferHeight };

}

void DXRenderDeviceManager::Tick(SystemTimer& Timer)
{
	// 每帧开始绘制前先将帧资源数组中循环找出下一个帧资
	// 源索引并获取其相应的帧资源作为当前使用的帧资源
	CurrentFrameResourceIndex = (CurrentFrameResourceIndex + 1) % gNumFrameResources;
	CurrentFrameResource = FrameResources[CurrentFrameResourceIndex].get();

	// 根据围栏点Fence判断当前GPU是否还在执行此帧资源的命令队列
	// 每个帧资源(FrameResource)中的Fence默认都为0，每次在CPU向名列列表写完命令后FrameResource中的Fence都会自增1
	// 因此如果CurrentFrameResource->Fence != 0则CurrentFrameResource一定被写入过命令和资源描述符信息，而当
	// 围栏Fence的完成值GetCompletedValue()小于CurrentFrameResource->Fence时则表示此帧资源仍被GPU执行使用
	if (CurrentFrameResource->Fence != 0 && Fence->GetCompletedValue() < CurrentFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, LPCWSTR("GPUFence"), false, EVENT_ALL_ACCESS);
		ThrowIfFailed(Fence->SetEventOnCompletion(CurrentFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	// 找到可用帧资源后，继续后续CPU绘制逻辑
}

void DXRenderDeviceManager::Clear(SystemTimer& Timer, ID3D12PipelineState* pPipelineState)
{
	if (CurrentFrameResource == nullptr)
		return;

	// 将当前帧的绘制渲染指令通过命令列表写入当前帧资源的命令分配器中
	CmdListAlloc = CurrentFrameResource->CmdListAlloc;

	// 由于在本函数的最后调用了FlushCommandQueue()，这就可以确保每次调用Render()时,GPU已经将上一帧的所有指令全部执行完成
	// 因此在绘制新的一帧前，要清理命令/指令分配器 重置命令/指令列表
	ThrowIfFailed(CmdListAlloc->Reset());

	// 重置命令列表
	ThrowIfFailed(CommandList->Reset(CmdListAlloc.Get(), pPipelineState));

	// 由于上一帧绘制完成时会执行交换链的两个缓冲区互换，这就使得之前的用于显示的缓冲区变成了当前帧需要绘制的缓冲
	// 因此需要将该缓冲区的资源状态改为渲染目标
	CD3DX12_RESOURCE_BARRIER currentBackBufferResBarrier = CD3DX12_RESOURCE_BARRIER::Transition(BackgroundBuffer[CurrBackBuffer].Get(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	CommandList->ResourceBarrier(1, &currentBackBufferResBarrier);

	// 设置视口及裁剪矩形
	CommandList->RSSetViewports(1, &ScreenViewport);
	CommandList->RSSetScissorRects(1, &ScissorRect);

	// 清理后台缓冲区及深度缓冲区
	CommandList->ClearRenderTargetView(GetCurrentBackBufferDescriptor(), DirectX::Colors::LightSteelBlue, 0, nullptr);
	CommandList->ClearDepthStencilView(GetDepthStencilDescriptor(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// 指定我们要渲染到的后台缓冲区和深度缓冲区
	D3D12_CPU_DESCRIPTOR_HANDLE curBackBufferDescriptor = GetCurrentBackBufferDescriptor();
	D3D12_CPU_DESCRIPTOR_HANDLE depthStencilDesc = GetDepthStencilDescriptor();
	CommandList->OMSetRenderTargets(1, &curBackBufferDescriptor, true, &depthStencilDesc);
}

void DXRenderDeviceManager::Present(SystemTimer& Timer)
{
	// 在设置完所有渲染指令后，将后台缓冲区的资源状态改为呈现(准备提交后台缓冲区到前台显示)
	CD3DX12_RESOURCE_BARRIER currentBackBufferResBarrier = CD3DX12_RESOURCE_BARRIER::Transition(BackgroundBuffer[CurrBackBuffer].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

	CommandList->ResourceBarrier(1, &currentBackBufferResBarrier);

	// 关闭命令列表(完成本帧内的命令写入)
	ThrowIfFailed(CommandList->Close());

	// 将命令列表中的命令提交到GPU的命令队列中执行
	ID3D12CommandList* cmdsLists[] = { CommandList.Get() };
	CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// 执行交换链的前后缓冲区互换
	ThrowIfFailed(SwapChain->Present(0, 0));
	CurrBackBuffer = (CurrBackBuffer + 1) % SWAPCHAINBUFFERCOUNT;

	//// 等待GPU完成所有命令队列中的指令后CPU继续执行
	//FlushCommandQueue();

	// 不在需要等待GPU完成所有命令而只需将当前帧资源的围栏点自增1
	// 并通知GPU此段命令执行完成后的新围栏点即可
	CurrentFrameResource->Fence = ++CurrentFence;

	CommandQueue->Signal(Fence.Get(), CurrentFence);
}

void DXRenderDeviceManager::ResetCommandList(ID3D12PipelineState* pPipelineState)
{
	ThrowIfFailed(CommandList->Reset(CmdListAlloc.Get(), pPipelineState));
}

void DXRenderDeviceManager::ExecuteCommandQueue()
{
	// Execute the initialization commands.
	ThrowIfFailed(CommandList->Close());
	ID3D12CommandList* cmdsLists[] = { CommandList.Get() };
	CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();
}

//void DXRenderDeviceManager::Render(SystemTimer& Timer)
//{
//	// 由于在本函数的最后调用了FlushCommandQueue()，这就可以确保每次调用Render()时,GPU已经将上一帧的所有指令全部执行完成
//	// 因此在绘制新的一帧前，要清理命令/指令分配器 重置命令/指令列表
//	ThrowIfFailed(CmdListAlloc->Reset());
//
//	// 重置命令列表
//	ThrowIfFailed(CommandList->Reset(CmdListAlloc.Get(), nullptr));
//
//	// 由于上一帧绘制完成时会执行交换链的两个缓冲区互换，这就使得之前的用于显示的缓冲区变成了当前帧需要绘制的缓冲
//	// 因此需要将该缓冲区的资源状态改为渲染目标
//	CD3DX12_RESOURCE_BARRIER currentBackBufferResBarrier = CD3DX12_RESOURCE_BARRIER::Transition(BackgroundBuffer[CurrBackBuffer].Get(),
//		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
//	CommandList->ResourceBarrier(1, &currentBackBufferResBarrier);
//
//	// 设置视口及裁剪矩形
//	CommandList->RSSetViewports(1, &ScreenViewport);
//	CommandList->RSSetScissorRects(1, &ScissorRect);
//
//	// 清理后台缓冲区及深度缓冲区
//	CommandList->ClearRenderTargetView(GetCurrentBackBufferDescriptor(), DirectX::Colors::LightSteelBlue, 0, nullptr);
//	CommandList->ClearDepthStencilView(GetDepthStencilDescriptor(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
//
//	// 指定我们要渲染到的后台缓冲区和深度缓冲区
//	D3D12_CPU_DESCRIPTOR_HANDLE curBackBufferDescriptor = GetCurrentBackBufferDescriptor();
//	D3D12_CPU_DESCRIPTOR_HANDLE depthStencilDesc = GetDepthStencilDescriptor();
//	CommandList->OMSetRenderTargets(1, &curBackBufferDescriptor, true, &depthStencilDesc);
//
//	// 此处在后期可以加上各种渲染绘制指令
//	// ...
//
//	CommandList->ResourceBarrier(1, &currentBackBufferResBarrier);
//
//	// 在设置完所有渲染指令后，将后台缓冲区的资源状态改为呈现(准备提交后台缓冲区到前台显示)
//	currentBackBufferResBarrier = CD3DX12_RESOURCE_BARRIER::Transition(BackgroundBuffer[CurrBackBuffer].Get(),
//		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
//	CommandList->ResourceBarrier(1, &currentBackBufferResBarrier);
//
//	// 关闭命令列表(完成本帧内的命令写入)
//	ThrowIfFailed(CommandList->Close());
//
//	// 将命令列表中的命令提交到GPU的命令队列中执行
//	ID3D12CommandList* cmdsLists[] = { CommandList.Get() };
//	CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
//
//	// 执行交换链的前后缓冲区互换
//	ThrowIfFailed(SwapChain->Present(0, 0));
//	CurrBackBuffer = (CurrBackBuffer + 1) % SWAPCHAINBUFFERCOUNT;
//
//	// 等待GPU完成所有命令队列中的指令后CPU继续执行
//	FlushCommandQueue();
//}

void DXRenderDeviceManager::FlushCommandQueue()
{
	// 更新CPU/GPU同步围栏值，带GPU完成此前所有命令列表中命令后CPU继续
	CurrentFence++;

	// 向命令队列设置一个新的围栏值，当GPU完成当前命令队列中所有命令指令时才会设置此围栏值
	ThrowIfFailed(CommandQueue->Signal(Fence.Get(), CurrentFence));

	// 如果当前GPU完成指令的表示仍然在当前新围栏值之前(说明当前GPU尚未完成所有指令4命令)
	if (Fence->GetCompletedValue() < CurrentFence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, LPCWSTR("GPUFence"), false, EVENT_ALL_ACCESS);

		// 设置当GPU完成当前所有指令后(即:完成所有指令并将围栏值更新为最新currentFence时)发送eventHandle事件 
		ThrowIfFailed(Fence->SetEventOnCompletion(CurrentFence, eventHandle));

		// CPU等待GPU执行完成
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

void DXRenderDeviceManager::UploadRenderPassConstantBuffer(const PassConstants& PassConstantsData)
{
	if (CurrentFrameResource == nullptr)
		return;

	auto RenderPassCB = CurrentFrameResource->PassCB.get();

	if (RenderPassCB == nullptr)
		return;

	RenderPassCB->CopyData(0, PassConstantsData);
}

void DXRenderDeviceManager::UploadObjectConstantBuffer(const ObjectConstants& ObjectConstantsData, int ObjectCBIndex)
{
	if (CurrentFrameResource == nullptr)
		return;

	auto CurrObjectCB = CurrentFrameResource->ObjectCB.get();

	if (CurrObjectCB == nullptr)
		return;

	//CurrObjectCB->CopyData(ObjectCBIndex, ObjectConstantsData);
}

FrameResource* DXRenderDeviceManager::GetCurrentFrameResource()
{
	return CurrentFrameResource;
}

UINT DXRenderDeviceManager::GetCurrentFrameResourceIndex()
{
	return CurrentFrameResourceIndex;
}

// 初始化D3DDevice
bool DXRenderDeviceManager::InitD3DDevice()
{

#if defined(DEBUG) || defined(_DEBUG) 
	// Debug模式下开启debugController
	{
		ComPtr<ID3D12Debug> debugController;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
		debugController->EnableDebugLayer();
	}
#endif

	// 创建DXGIFactory
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&DXGIFactory)));

	// 尝试创建硬件的D3DDevice设备
	HRESULT hardwareResult = D3D12CreateDevice(
		nullptr,						// 创建D3DDevice时所用的显示适配器(显卡)，nullptr表示使用默认主显卡 
		D3D_FEATURE_LEVEL_11_0,			// 创建D3DDevice设备时要求硬件所能支持的最低功能级别在dx11特性以上(如果硬件不支持则创建失败)
		IID_PPV_ARGS(&D3DDevice));		// IID_PPV_ARGS宏定义了两个变量一个是局部变量D3DDevice接口的COMID(一般不会直接使用)，另一个
										// 则是我们经常使用的D3DDevice设备的指针也就是d3dDevice

	// 如果创建失败则退回到WARP Device(Windows自带的软件光栅化设备).
	if (FAILED(hardwareResult))
	{
		// 由DXGI的EnumWarpAdapter接口获取软光栅化设备
		ComPtr<IDXGIAdapter> pWarpAdapter;
		ThrowIfFailed(DXGIFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter)));

		// 在此调用D3D12CreateDevice接口，将创建D3DDevice时所用的显示适配器改为软光栅化设备
		hardwareResult = D3D12CreateDevice(
			pWarpAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&D3DDevice));

		// 此时仍然创建失败则初始化结束，返回false
		if (FAILED(hardwareResult))
			return false;
	}

	return true;
}

// 检测D3DDevice的基本信息
bool DXRenderDeviceManager::CheckDeviceBaseInfo()
{
	// 创建护栏
	HRESULT hardwareResult = D3DDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Fence));

	if (FAILED(hardwareResult))
		return false;

	// 获取三种描述符在当前d3dDevice设备下的大小
	RTVDescriptorSize = D3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	DSVDescriptorSize = D3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	CBVDescriptorSize = D3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// 由于InitD3DDevice()中创建的D3DDevice最低功能级别为DX11，而DX11功能级别以上的设备对所有渲染对象格式均
	// 都支持4XMSAA，因此我们只需检测其所支持的品质级别即可。
	// 通过CheckFeatureSupport接口可以查询各种功能特性的支持情况，此处使用D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS
	// 结构体来查询MASS品质级别的支持情况
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
	msQualityLevels.Format = BACKBUFFER_FORMAT;
	msQualityLevels.SampleCount = 4;
	msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	msQualityLevels.NumQualityLevels = 0;
	ThrowIfFailed(D3DDevice->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&msQualityLevels,
		sizeof(msQualityLevels)));

	MSAAQuality = msQualityLevels.NumQualityLevels;

	return MSAAQuality > 0;
}

void DXRenderDeviceManager::CreateCommondQueue()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; // 命令队列中存储指令的方式为存储GPU直接可执行的指令(D3D12_COMMAND_LIST_TYPE_DIRECT)，
													 // 一般均使用D3D12_COMMAND_LIST_TYPE_DIRECT， 其他类型可自行翻阅文档，eg:D3D12_COMMAND_LIST_TYPE_BUNDLE
													 // 为将命令打包
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	// 创建CommandQueue
	ThrowIfFailed(D3DDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&CommandQueue)));

	// 创建指令/命令分配器cmdListAlloc
	ThrowIfFailed(D3DDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));

	ThrowIfFailed(D3DDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		CmdListAlloc.Get(), // 要创建的命令列表绑定的指令分配器
		nullptr,                   // 渲染流水线状态对象: 我们目前不会发起任何绘制指令因此渲染流水线状态对象置空
		IID_PPV_ARGS(CommandList.GetAddressOf())));	// 创建完成的命令列表

	// 之所以需要将命令列表关闭是因为在第一次引用命令队列时，我们要对其进行重置(Reset),而调用
	// Reset()重置前需要先将CommandList关闭
	CommandList->Close();
}

void DXRenderDeviceManager::CreateSwapChain()
{
	// 释放之前的交换链，随后进行重建(有可能会在运行时重新创建交换链，eg: 运行时开启/关闭MASS)
	SwapChain.Reset();

	// DXGI_SWAP_CHAIN_DESC为交换链信息
	DXGI_SWAP_CHAIN_DESC sd;
	sd.BufferDesc.Width = BackBufferWidth;				// 后台缓冲区分辨率宽度
	sd.BufferDesc.Height = BackBufferHeight;			// 后台缓冲区分辨率高度
	sd.BufferDesc.RefreshRate.Numerator = 60;			// 后台缓冲区刷新频率 60次
	sd.BufferDesc.RefreshRate.Denominator = 1;			// 后台缓冲区刷新频率 1s
	sd.BufferDesc.Format = BACKBUFFER_FORMAT;			// 后台缓冲区格式
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sd.SampleDesc.Count = EnableMSAA ? 4 : 1;			// 对每个像素的采样次数(子像素/片元个数)，如果开启4xMASS,则为4
	sd.SampleDesc.Quality = EnableMSAA ? (MSAAQuality - 1) : 0;	// 采样质量级别
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;	// 由于我们要将数据最终渲染到后台缓冲区，因此其类型为渲染目标
	sd.BufferCount = SWAPCHAINBUFFERCOUNT;				// 后台缓冲区个数(默认2个也就是双缓冲)
	sd.OutputWindow = MainWinHWND;						// 绑定的主窗口Handle
	sd.Windowed = true;									// 是否是窗口模式
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	// 使用DXGI创建交换链，绑定命令队列并使用DXGI_SWAP_CHAIN_DESC描述交换链信息
	ThrowIfFailed(DXGIFactory->CreateSwapChain(CommandQueue.Get(), &sd, SwapChain.GetAddressOf()));
}

void DXRenderDeviceManager::CreateDescriptorHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;				// rtv描述符堆的信息类型
	rtvHeapDesc.NumDescriptors = SWAPCHAINBUFFERCOUNT;	// 在交换链中有SWAPCHAINBUFFERCOUNT个后台缓冲区因此创建SWAPCHAINBUFFERCOUNT个
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;	// 创建类型为RTV描述符
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(D3DDevice->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(RTVHeap.GetAddressOf())));


	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;				// dsv描述符堆的信息类型	
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;	// 创建类型为DSV描述符
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(D3DDevice->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(DSVHeap.GetAddressOf())));
}

void DXRenderDeviceManager::CreateBufferDescriptor()
{
	// 对于后台缓冲，由于SwapChain已经创建了SWAPCHAINBUFFERCOUNT个缓冲，此时仅需获取出
	// 每个缓冲区的资源并为其创建描述符即可
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(RTVHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < SWAPCHAINBUFFERCOUNT; i++)
	{
		// 从交换链中获取SWAPCHAINBUFFERCOUNT个后台缓冲区资源
		ThrowIfFailed(SwapChain->GetBuffer(i, IID_PPV_ARGS(&BackgroundBuffer[i])));
		// 为每个后台缓冲区资源创建RenderTarget类型的描述符()
		D3DDevice->CreateRenderTargetView(BackgroundBuffer[i].Get(),		// RenderTarget的资源
			nullptr,		// D3D12_RENDER_TARGET_VIEW_DESC结构数据用于描述资源中数据类型及格式，由于在创建SwapChain时已制定此处未nullptr
			rtvHeapHandle);	// 该资源锁对应的描述符句柄(之前创建的资源描述符)
		rtvHeapHandle.Offset(1, RTVDescriptorSize);	// rtvHeapHandle为描述符堆/描述符数组，将该数组数据偏移取下一个数组中的数据
	}

	// 对于深度/模板缓冲区而言，则需要手动创建深度缓冲区资源，然后再为其创建描述符
	D3D12_RESOURCE_DESC depthStencilDesc;			// 创建资源时对资源的描述信息
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;	// 资源类型/资源维度，是Buffer/1DTexture/2DTexture/3DTextire 此处深度模板缓存使用2DTexture杰克
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = BackBufferWidth;	// 资源宽高
	depthStencilDesc.Height = BackBufferHeight;
	depthStencilDesc.DepthOrArraySize = 1;		// 资源深度(3DTexture)或者资源数组中的资源个数
	depthStencilDesc.MipLevels = 1;				// Mipmap级别  深度缓冲区无需Mipmap
	depthStencilDesc.Format = DEPTHSTENCIL_FORMAT; // 该资源的数据元素格式(也就是深度/模板缓冲区存储的纹理数据格式)

	depthStencilDesc.SampleDesc.Count = EnableMSAA ? 4 : 1;	// 为每个像素的采样次数(子像素/片元采样个数)
	depthStencilDesc.SampleDesc.Quality = EnableMSAA ? (MSAAQuality - 1) : 0;	// 采样品质级别
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;	// 纹理布局
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;	// 资源相关的标识

	// 清理该资源时的清理值信息
	D3D12_CLEAR_VALUE optClear;
	optClear.Format = DEPTHSTENCIL_FORMAT;	// 清理格式类型
	optClear.DepthStencil.Depth = 1.0f;		// 清理默认深度值
	optClear.DepthStencil.Stencil = 0;		// 清理默认模板之

	CD3DX12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	// 使用CreateCommittedResource创建深度/模板缓冲区资源
	ThrowIfFailed(D3DDevice->CreateCommittedResource(
		&heapProperties,		// GPU中的资源都存放在特定的显存堆中，为了性能优化此处指明该资源放置的堆类型(默认堆/上传堆/回读堆等)
		D3D12_HEAP_FLAG_NONE,	// 与该资源上传到的堆有关的标志信息
		&depthStencilDesc,		// 要创建的资源信息
		D3D12_RESOURCE_STATE_COMMON,	// 由于DX中每个资源在任何时刻都会标明其当前所处在渲染流水线中的状态，此处为该资源的默认初始状态
		&optClear,	// 清理该资源时的清理值信息
		IID_PPV_ARGS(DepthStencilBuffer.GetAddressOf())));		// 创建返回的深度模板缓冲区

	// 与后台缓冲区的描述符创建方式(CreateRenderTarget)类似，使用CreateDepthStencilView创建深度/模板缓冲区描述符
	D3DDevice->CreateDepthStencilView(DepthStencilBuffer.Get(), nullptr, DSVHeap->GetCPUDescriptorHandleForHeapStart());

	CD3DX12_RESOURCE_BARRIER depthStencilResBarrier = CD3DX12_RESOURCE_BARRIER::Transition(DepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	// 将后台缓冲区资源从初始状态设置为写入状态，等待渲染命令对列表写入深度信息
	CommandList->ResourceBarrier(1, &depthStencilResBarrier);
}

void DXRenderDeviceManager::CreateFrameResources(UINT ObjectCount, UINT MatCount)
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		FrameResources.push_back(std::make_unique<FrameResource>(D3DDevice.Get(),
			1, ObjectCount, MatCount));
	}
}

void DXRenderDeviceManager::CreateFrameResources(UINT passCount, UINT ObjectCount, UINT MatCount, UINT waveVertexCount)
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		FrameResources.push_back(std::make_unique<FrameResource>(D3DDevice.Get(),
			passCount, ObjectCount, MatCount, waveVertexCount));
	}
}

FrameResource* DXRenderDeviceManager::GetFrameResource(UINT Index)
{
	if (Index >= FrameResources.size())
		return nullptr;

	return FrameResources[Index].get();
}

UINT DXRenderDeviceManager::GetConstantDescriptorSize()
{
	return CBVDescriptorSize;
}

D3D12_CPU_DESCRIPTOR_HANDLE DXRenderDeviceManager::GetCurrentBackBufferDescriptor()
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		RTVHeap->GetCPUDescriptorHandleForHeapStart(),
		CurrBackBuffer,
		RTVDescriptorSize);;
}

D3D12_CPU_DESCRIPTOR_HANDLE DXRenderDeviceManager::GetDepthStencilDescriptor()
{
	return DSVHeap->GetCPUDescriptorHandleForHeapStart();
}


DXRenderDeviceManager::DXRenderDeviceManager()
{

}


DXRenderDeviceManager::~DXRenderDeviceManager()
{

}
