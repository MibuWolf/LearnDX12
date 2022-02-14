#pragma once

#include "DX12Util.h"
#include "SystemTimer.h"
#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif


// Link necessary d3d12 libraries.
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

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

	// 重置后台缓冲区大小
	virtual void	OnResize();

	// 总循环的逻辑更新
	virtual void Tick(SystemTimer& Timer);

	// 总循环的绘制
	virtual void Render(SystemTimer& Timer);

	// 刷新GPU命令队列，待GPU命令队列玩成前CPU处于等待避免在GPU完成绘制前，CPU修改资源属性
	void		FlushCommandQueue();

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
	ComPtr<IDXGIFactory4>	dxgiFactory;
	// D3D设备
	ComPtr<ID3D12Device>	d3dDevice;
	// CPU/GPU同步围栏
	ComPtr<ID3D12Fence>		fence;
	UINT64					currentFence = 0;

	// 命令队列
	ComPtr<ID3D12CommandQueue> commandQueue;
	// 命令/指令分配器
	ComPtr<ID3D12CommandAllocator> cmdListAlloc;
	// 命令列表
	ComPtr<ID3D12GraphicsCommandList> commandList;
	// 交换链
	ComPtr<IDXGISwapChain>	swapChain;
	// 后台缓冲区Buffer
	ComPtr<ID3D12Resource> backgroundBuffer[SWAPCHAINBUFFERCOUNT];
	int currBackBuffer = 0;
	// 深度/模板缓冲区Buffer
	ComPtr<ID3D12Resource> depthStencilBuffer;
	// 为后台缓冲区创建RenderTargetView描述符
	ComPtr<ID3D12DescriptorHeap> rtvHeap;
	// 为深度缓冲区创建Depth/StencilView描述符
	ComPtr<ID3D12DescriptorHeap> dsvHeap;

	// 视口
	D3D12_VIEWPORT screenViewport;
	// 裁剪矩形
	D3D12_RECT scissorRect;

	// 主窗口handle
	HWND      mainWinHWND = nullptr;

	// 4XMass品质级别
	UINT      msaaQuality = 0;
	// 是否开启MSAA
	bool	  enableMSAA = false;
	// rendertarget描述符大小
	UINT					rtvDescriptorSize = 0;
	// 常量缓冲区描述符大小
	UINT					cbvDescriptorSize = 0;
	// 深度/模板缓冲区描述符大小
	UINT					dsvDescriptorSize = 0;

	// 后台缓冲区宽高
	int backBufferWidth = 1280;
	int backBufferHeight = 768;

};
