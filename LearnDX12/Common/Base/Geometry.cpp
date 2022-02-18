#include "Base/Geometry.h"
#include "DX12Util.h"


void Geometry::UploadVertexData(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const void* initData, UINT64 StrideSize, UINT64 byteSize)
{
	if (!CreateAndUploadBuffer(device, cmdList, initData, byteSize, VertexBufferUploader, VertexBufferGPU))
		return;

	// 顶点缓冲区描述符D3D12_VERTEX_BUFFER_VIEW
	VertexBufferView.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();	// 记录顶点缓冲区显存地址
	VertexBufferView.SizeInBytes = byteSize;		// 顶点数据所占字节数
	VertexBufferView.StrideInBytes = StrideSize;	// 每个顶点元素所占的字节数
}

void Geometry::UploadVertexIndexData(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const void* initData, UINT64 byteSize)
{
	if (!CreateAndUploadBuffer(device, cmdList, initData, byteSize, IndexBufferUploader, IndexBufferGPU))
		return;

	// 索引缓冲区描述符D3D12_INDEX_BUFFER_VIEW
	IndexBufferView.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();	// 索引缓冲区地址
	IndexBufferView.Format = DXGI_FORMAT_R16_UINT;		// 索引格式
	IndexBufferView.SizeInBytes = byteSize;				// 索引缓冲区大小

}

bool Geometry::CreateAndUploadBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const void* initData, UINT64 byteSize, ComPtr<ID3D12Resource>& uploadBuffer, ComPtr<ID3D12Resource>& buffer)
{
	if (device == nullptr || cmdList == nullptr || initData == nullptr)
		return false;

	CD3DX12_HEAP_PROPERTIES DX12HeapDefault = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);		// 默认显存堆缓冲资源类型
	CD3DX12_HEAP_PROPERTIES DX12HeapUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);		// 上传堆缓冲资源类型
	CD3DX12_RESOURCE_DESC DX12ResDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
	/**
	*		static inline CD3DX12_RESOURCE_DESC Buffer(UINT64 width, UINT64 alignment = 0)
			{
				return CD3DX12_RESOURCE_DESC(D3D12_RESOURCE_DIMENSION_BUFFER,	// 创建资源的类型为维度缓冲区
					alignment,													// 字节对齐
					width,														// 缓冲区大小 （资源宽度eg:tex2D）
					1,															// 资源高度 (eg: tex2D)
					1,															// 资源深度/数组个数(eg:tex3D)
					1,															// mipleves级别
					DXGI_FORMAT_UNKNOWN,										// 存储的单个数据格式
					1,															// 样本计数
					0,															// 样本质量
					D3D12_TEXTURE_LAYOUT_ROW_MAJOR,								// D3D12_TEXTURE_LAYOUT布局
					flags);														// D3D12_RESOURCE_FLAGS标志
			}
	*/

	// 在显存中创建默认堆的缓冲区用于渲染流水线读取资源
	ThrowIfFailed(device->CreateCommittedResource(
		&DX12HeapDefault,							// 创建资源堆属性信息，指明资源存放的堆
		D3D12_HEAP_FLAG_NONE,						// 堆选项，作为D3D12_HEAP_FLAGS枚举常量的按位或组合。
		&DX12ResDesc,								// 描述资源信息的结构体
		D3D12_RESOURCE_STATE_COMMON,				// 资源初始化状态为Common
		nullptr,									// 指定描述清理Clear颜色默认值的D3D12_CLEAR_VALUE结构(对于Tex类的资源使用)。
		IID_PPV_ARGS(buffer.GetAddressOf())));		// 创建成功的资源返回

	// 在显存中创建上传堆的缓冲区用于快速接受内存上传的数据，然后将
	// 此缓冲区数据拷贝到默认堆缓冲，用于渲染流水线快速读取
	ThrowIfFailed(device->CreateCommittedResource(
		&DX12HeapUpload,
		D3D12_HEAP_FLAG_NONE,
		&DX12ResDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(uploadBuffer.GetAddressOf())));


	// 描述我们希望拷贝到顶点/索引缓冲区中的资源
	D3D12_SUBRESOURCE_DATA subResourceData = {};
	subResourceData.pData = initData;		// 数据所在的内存块/数据指针
	subResourceData.RowPitch = byteSize;	// 希望拷贝上传到显存缓冲区中的数据大小(字节数)   子资源数据的行间距、宽度或物理大小（以字节为单位）。
	subResourceData.SlicePitch = subResourceData.RowPitch;	// 希望拷贝上传到显存缓冲区中的数据大小(字节数)  子资源数据的深度间距、宽度或物理大小（以字节为单位）。

	// 将数据上传到upload堆缓冲区然后复制到资源缓冲区的流程
	// UpdateSubresources函数将辅助我们实现将内存数据上传到到upload堆缓冲区
	// 然后再讲upload堆缓冲区中的数据复制到资源缓冲区的流程，下面详细分析

	CD3DX12_RESOURCE_BARRIER defaultResBarrier1 = CD3DX12_RESOURCE_BARRIER::Transition(buffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
	CD3DX12_RESOURCE_BARRIER defaultResBarrier2 = CD3DX12_RESOURCE_BARRIER::Transition(buffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
	// 先将最终顶点/索引缓冲区的资源状态改为拷贝到的目标
	cmdList->ResourceBarrier(1, &defaultResBarrier1);
	// 利用UpdateSubresources函数进行上传及拷贝
	UpdateSubresources<1>(cmdList,				// cmdList
		buffer.Get(),							// 渲染流水线使用的最终顶点/索引缓冲区
		uploadBuffer.Get(),						// 用于接收CPU上传数据的upload缓冲区
		0,										// 上传资源偏移量
		0,										// 第一个子资源索引
		1,										// 包含的子资源个数
		&subResourceData);						// 子资源数据及信息
	// 将最终顶点/索引缓冲区的资源状态由拷贝到的目标改为等待渲染流水线读取
	cmdList->ResourceBarrier(1, &defaultResBarrier2);

	return true;
}
