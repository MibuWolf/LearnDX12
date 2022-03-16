#pragma once
#include "DX12Util.h"
#include "MathHelper.h"
#include "UploadBuffer.h"
#include "GeometryUtil.h"


// 帧资源用于记录和存储CPU为构建每帧命令列表所需的资源及命令信息
struct FrameResource
{
public:

	FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount);
	FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount, UINT waveVertCount);
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource();

	// 每帧都需要有个独立的命令分配器用于记录当前帧CPU要执行的Command信息
	// 如果GPU正在使用当前命令分配器进行绘制，则在绘制完成前CPU不可再对其重置或修改
	ComPtr<ID3D12CommandAllocator> CmdListAlloc;

	// 渲染每一帧时需要设置的常量缓冲区中的数据或资源
	// 同样如果GPU正在使用当前命令分配器进行绘制，则在绘制完成前CPU不可再对其重置或修改
	std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
	std::unique_ptr<UploadBuffer<MaterialConstants>> MaterialCB = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;

	// 用于存储动态的顶点数据的缓冲区
	std::unique_ptr<UploadBuffer<Vertex>> WavesVB = nullptr;

	// 记录当前帧所处的围栏点信息，用于判断当前GPU是否仍在使用该帧资源数据
	// 判断方式与DXRenderDeviceManager::FlushCommandQueue()相同
	UINT64 Fence = 0;
};
