#pragma once

#include "DX12Util.h"
#include "UploadBuffer.h"
#include "MathHelper.h"
#include "SystemTimer.h"
#include "Base/FrameResource.h"
#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif


// Link necessary d3d12 libraries.
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")
using namespace DirectX;

#define BACKBUFFER_FORMAT  DXGI_FORMAT_R8G8B8A8_UNORM
#define DEPTHSTENCIL_FORMAT  DXGI_FORMAT_D24_UNORM_S8_UINT
#define SWAPCHAINBUFFERCOUNT 2



class DXRenderDeviceManager
{
public:

	static DXRenderDeviceManager& GetInstance();

protected:

	DXRenderDeviceManager();

	virtual ~DXRenderDeviceManager();

public:

	// 初始化渲染设备设备
	virtual	bool	InitD3DDevice(HWND	hwnd);

	// 创建帧资源数组
	void		CreateFrameResources(UINT passCount, UINT ObjectCount, UINT MatCount);

	// 创建帧资源数组
	void		CreateFrameResources(UINT passCount, UINT ObjectCount, UINT MatCount, UINT waveVertexCount);

	// 重置后台缓冲区大小
	virtual void	OnResize();

	// 总循环的逻辑更新
	virtual void Tick(SystemTimer& Timer);

	// 清理后台缓冲区
	virtual void Clear(SystemTimer& Timer, ID3D12PipelineState* pPipelineState = nullptr);

	// 推送后台缓冲区到前台显示
	virtual void Present(SystemTimer& Timer, bool DefaultPresent = true);

	// 重置命令列表
	virtual void ResetCommandList(ID3D12PipelineState* pPipelineState = nullptr);

	// 执行命令队列
	virtual void ExecuteCommandQueue();

	//// 总循环的绘制
	//virtual void Render(SystemTimer& Timer);

	// 刷新GPU命令队列，待GPU命令队列玩成前CPU处于等待避免在GPU完成绘制前，CPU修改资源属性
	void		FlushCommandQueue();

	// 上传更新RenderPass常量缓冲区到当前的FrameResrouce中
	void		UploadRenderPassConstantBuffer(const PassConstants& PassConstantsData);

	// 上传更新几何体对象常量缓冲区到当前的FrameResrouce中
	void		UploadObjectConstantBuffer(const ObjectConstants& ObjectConstantsData, int ObjectCBIndex);

	// 获取当前帧资源
	FrameResource* GetCurrentFrameResource();

	// 获取当前帧资源索引
	UINT		GetCurrentFrameResourceIndex();

	// 获取帧资源
	FrameResource* GetFrameResource(UINT Index);

	// 获取常量缓冲区描述符大小
	UINT		GetConstantDescriptorSize();

	// 获取当前MASS是否开启
	bool		CheckMSAAState()
	{
		return EnableMSAA;
	}

	// 获取MASS品质
	UINT		GetMSAAQuality()
	{
		return MSAAQuality;
	}

public:

	// 获取D3D设备
	ID3D12Device* GetD3DDevice()
	{
		return D3DDevice.Get();
	}

	// 获取命令列表
	ID3D12GraphicsCommandList* GetCommandList()
	{
		return CommandList.Get();
	}

	// 获取当前后台缓冲区资源
	ID3D12Resource* GetCurrentBackgroundBuffer();

protected:

	// 初始化D3DDevice
	bool		InitD3DDevice();

	// 检测D3DDevice的基本信息
	bool		CheckDeviceBaseInfo();

	// 创建及初始化命令队列及命令列表
	void		CreateCommondQueue();

	// 描述创建交换链
	void		CreateSwapChain();

	// 创建描述符堆
	void		CreateDescriptorHeap();

	// 创建缓冲区(后台缓冲区/深度模板缓冲区)描述符
	void		CreateBufferDescriptor();


	// 获取当前后台缓冲区的描述符
	D3D12_CPU_DESCRIPTOR_HANDLE		GetCurrentBackBufferDescriptor();

	// 获取当前深度缓冲区的描述符
	D3D12_CPU_DESCRIPTOR_HANDLE		GetDepthStencilDescriptor();

private:

	// dxgiFactory 用于创建和调用各种DXGI接口
	ComPtr<IDXGIFactory4>	DXGIFactory;
	// D3D设备
	ComPtr<ID3D12Device>	D3DDevice;
	// CPU/GPU同步围栏
	ComPtr<ID3D12Fence>		Fence;
	UINT64					CurrentFence = 0;

	// 命令队列
	ComPtr<ID3D12CommandQueue> CommandQueue;
	// 命令/指令分配器
	ComPtr<ID3D12CommandAllocator> CmdListAlloc;
	// 命令列表
	ComPtr<ID3D12GraphicsCommandList> CommandList;
	// 交换链
	ComPtr<IDXGISwapChain>	SwapChain;
	// 后台缓冲区Buffer
	ComPtr<ID3D12Resource> BackgroundBuffer[SWAPCHAINBUFFERCOUNT];
	int CurrBackBuffer = 0;
	// 深度/模板缓冲区Buffer
	ComPtr<ID3D12Resource> DepthStencilBuffer;
	// 为后台缓冲区创建RenderTargetView描述符
	ComPtr<ID3D12DescriptorHeap> RTVHeap;
	// 为深度缓冲区创建Depth/StencilView描述符
	ComPtr<ID3D12DescriptorHeap> DSVHeap;

	// 视口
	D3D12_VIEWPORT ScreenViewport;
	// 裁剪矩形
	D3D12_RECT ScissorRect;

	// 主窗口handle
	HWND      MainWinHWND = nullptr;

	// 4XMass品质级别
	UINT      MSAAQuality = 0;
	// 是否开启MSAA
	bool	  EnableMSAA = false;
	// rendertarget描述符大小
	UINT					RTVDescriptorSize = 0;
	// 常量缓冲区描述符大小
	UINT					CBVDescriptorSize = 0;
	// 深度/模板缓冲区描述符大小
	UINT					DSVDescriptorSize = 0;

	// 后台缓冲区宽高
	int BackBufferWidth = 1920;
	int BackBufferHeight = 1080;

	// 帧资源数组
	std::vector<std::unique_ptr<FrameResource>> FrameResources;
	//当前使用的帧资源
	FrameResource* CurrentFrameResource = nullptr;
	// 当前帧资源索引
	int	CurrentFrameResourceIndex = 0;
};
