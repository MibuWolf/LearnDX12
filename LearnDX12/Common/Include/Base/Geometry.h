#pragma once
#include <string>
#include <DirectXMath.h>
#include "DX12Util.h"
#include "UploadBuffer.h"
#include "MathHelper.h"
#include "SystemTimer.h"
#include "Base/GeometryUtil.h"
using namespace DirectX;



struct Geometry
{
public:

	// 模型名
	std::string Name;

	// 在内存级别的ID3DBlob，用于存储一般格式的模型顶点/索引的数据缓冲区
	ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
	ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

	// 在显存级别为顶点/索引创建的缓冲区资源(Default堆内存储的缓冲区，渲染管线各个阶段从该缓冲区中读取顶点/索引数据)
	ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
	ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

	// 顶点缓冲区描述符
	D3D12_VERTEX_BUFFER_VIEW	VertexBufferView;
	// 顶点索引缓冲区描述符
	D3D12_INDEX_BUFFER_VIEW		IndexBufferView;

	// 在显存级别为顶点/索引创建的缓冲区资源(Upload堆内存储的缓冲区，用于快速高效接受从内存传输而来的数据)
	// 因此一般用此缓冲区接受内存上传的数据，然后将此缓冲区的数据拷贝的Default堆内存的缓冲区VertexBufferGPU/IndexBufferGPU
	// 中用于后续渲染管线的快速读取。
	ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
	ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

	// 常量缓冲区描述符堆
	ComPtr<ID3D12DescriptorHeap> CBVHeap = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectConstantBuffer = nullptr;

	ComPtr<ID3D12RootSignature> RootSignature = nullptr;

	// ShaderCode
	ComPtr<ID3DBlob> VSByteCode = nullptr;
	ComPtr<ID3DBlob> PSByteCode = nullptr;
	// Input描述信息
	std::vector<D3D12_INPUT_ELEMENT_DESC> InputLayout;

	ComPtr<ID3D12PipelineState> PSO = nullptr;

	DXGI_FORMAT BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT DepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;


public:


	// 初始化Gemetry数据
	void	Initialize();

	// 设置渲染参数
	void	SetMatrixParameter(XMMATRIX& matrixParam);

	// 渲染
	void	Draw(SystemTimer& Timer);


protected:

	// 创建并上传数据到缓冲区
	bool	CreateAndUploadBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const void* initData, UINT64 byteSize, ComPtr<ID3D12Resource>& uploadBuffer, ComPtr<ID3D12Resource>& buffer);

	// 从内存上传顶点数据到顶点缓冲区，先将内存顶点数据上传到显存的Upload堆缓冲区
	// 然后将数据从Upload堆缓冲区将数据拷贝到用于读取的显存默认堆缓冲区供后续渲染流水线使用
	void	UploadVertexData(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const void* initData, UINT64 StrideSize, UINT64 byteSize);

	// 从内存上传顶点索引数据到顶点缓冲区，先将内存顶点数据上传到显存的Upload堆缓冲区
	// 然后将数据从Upload堆缓冲区将数据拷贝到用于读取的显存默认堆缓冲区供后续渲染流水线使用
	void	UploadVertexIndexData(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const void* initData, UINT64 byteSize);

	// 创建常量缓冲区
	void	CreateConstantBuffers();

	// 创建RootSignature
	void	CreateRootSignature();

	// 创建Shader
	void	CreateShader();

	// 创建顶点/索引缓冲区
	void	CreateVertexAndIndexBuffer();

	// 创建PSO
	void	CreatePSO();

};
