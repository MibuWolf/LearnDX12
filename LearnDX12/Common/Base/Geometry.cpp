#include "Base/Geometry.h"
#include "DX12Util.h"
#include "DXRenderDeviceManager.h"




void Geometry::Initialize()
{
	CreateConstantBuffers();
	CreateRootSignature();
	CreateShader();
	CreateVertexAndIndexBuffer();
	CreatePSO();
}

void Geometry::Draw(SystemTimer& Timer)
{
	ID3D12GraphicsCommandList* pCommandList = DXRenderDeviceManager::GetInstance().GetCommandList();

	if (pCommandList == nullptr)
		return;

	ID3D12DescriptorHeap* descriptorHeaps[] = { CBVHeap.Get() };
	pCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	pCommandList->SetGraphicsRootSignature(RootSignature.Get());


	// 向命令列表中设置顶点缓冲区描述符
	pCommandList->IASetVertexBuffers(0,	// 该接口支持设置多个缓冲区，此参数表示起始输入缓冲区的索引 
		1,								// 缓冲区的数量
		&VertexBufferView);	// 指向一个缓冲区描述符数组
	// 向命令列表中设置索引缓冲区描述符的数组指针
	pCommandList->IASetIndexBuffer(&IndexBufferView);
	// 指定将要绘制的图元类型
	pCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	// 以索引方式开始绘制(支持多实例渲染)
	pCommandList->DrawIndexedInstanced(
		36,		// 每个绘制实例需要绘制的索引数量	
		1,		// 每次绘制1个实例
		0,		// 从索引下标为0的位置开始读取索引
		0,		// BaseVertexLocation 根据索引查找顶点时的基础顶点偏移(eg: 多个模型顶点索引数据合并后，可用此偏移表示绘制第几个模型)
		0);		// 用于在多实例渲染时使用
}


void Geometry::SetMatrixParameter(XMMATRIX& worldViewProj)
{
	ObjectConstants objConstants;
	XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(worldViewProj));
	ObjectConstantBuffer->CopyData(0, objConstants);
}



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

void Geometry::CreateConstantBuffers()
{
	ID3D12Device* pD3DDevice = DXRenderDeviceManager::GetInstance().GetD3DDevice();
	ID3D12GraphicsCommandList* pCommandList = DXRenderDeviceManager::GetInstance().GetCommandList();

	if (pD3DDevice == nullptr || pCommandList == nullptr)
		return;

	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NumDescriptors = 1;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;
	ThrowIfFailed(pD3DDevice->CreateDescriptorHeap(&cbvHeapDesc,
		IID_PPV_ARGS(&CBVHeap)));


	ObjectConstantBuffer = std::make_unique<UploadBuffer<ObjectConstants>>(pD3DDevice, 1, true);

	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	D3D12_GPU_VIRTUAL_ADDRESS cbAddress = ObjectConstantBuffer->Resource()->GetGPUVirtualAddress();
	// Offset to the ith object constant buffer in the buffer.
	int boxCBufIndex = 0;
	cbAddress += boxCBufIndex * objCBByteSize;

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
	cbvDesc.BufferLocation = cbAddress;
	cbvDesc.SizeInBytes = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	pD3DDevice->CreateConstantBufferView(
		&cbvDesc,
		CBVHeap->GetCPUDescriptorHandleForHeapStart());
}

void Geometry::CreateRootSignature()
{
	ID3D12Device* pD3DDevice = DXRenderDeviceManager::GetInstance().GetD3DDevice();
	ID3D12GraphicsCommandList* pCommandList = DXRenderDeviceManager::GetInstance().GetCommandList();

	if (pD3DDevice == nullptr || pCommandList == nullptr)
		return;

	// Shader programs typically require resources as input (constant buffers,
	// textures, samplers).  The root signature defines the resources the shader
	// programs expect.  If we think of the shader programs as a function, and
	// the input resources as function parameters, then the root signature can be
	// thought of as defining the function signature.  

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[1];

	// Create a single descriptor table of CBVs.
	CD3DX12_DESCRIPTOR_RANGE cbvTable;
	cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, 0, nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(pD3DDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(&RootSignature)));
}


void Geometry::CreateVertexAndIndexBuffer()
{
	std::array<Vertex, 8> vertices =
	{
		Vertex({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Black) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Red) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green) }),
		Vertex({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Blue) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Yellow) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Cyan) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Magenta) })
	};

	std::array<std::uint16_t, 36> indices =
	{
		// front face
		0, 1, 2,
		0, 2, 3,

		// back face
		4, 6, 5,
		4, 7, 6,

		// left face
		4, 5, 1,
		4, 1, 0,

		// right face
		3, 2, 6,
		3, 6, 7,

		// top face
		1, 5, 6,
		1, 6, 2,

		// bottom face
		4, 0, 3,
		4, 3, 7
	};

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	Name = "boxGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &VertexBufferCPU));
	CopyMemory(VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &IndexBufferCPU));
	CopyMemory(IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	ID3D12Device* pD3DDevice = DXRenderDeviceManager::GetInstance().GetD3DDevice();
	ID3D12GraphicsCommandList* pCommandList = DXRenderDeviceManager::GetInstance().GetCommandList();
	UploadVertexData(pD3DDevice, pCommandList, vertices.data(), sizeof(Vertex), vbByteSize);
	UploadVertexIndexData(pD3DDevice, pCommandList, indices.data(), ibByteSize);
}

void Geometry::CreateShader()
{
	HRESULT hr = S_OK;

	VSByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "VS", "vs_5_0");
	PSByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "PS", "ps_5_0");

	InputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void Geometry::CreatePSO()
{
	ID3D12Device* pD3DDevice = DXRenderDeviceManager::GetInstance().GetD3DDevice();
	if (pD3DDevice == nullptr)
		return;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.InputLayout = { InputLayout.data(), (UINT)InputLayout.size() };
	psoDesc.pRootSignature = RootSignature.Get();
	psoDesc.VS =
	{
		reinterpret_cast<BYTE*>(VSByteCode->GetBufferPointer()),
		VSByteCode->GetBufferSize()
	};
	psoDesc.PS =
	{
		reinterpret_cast<BYTE*>(PSByteCode->GetBufferPointer()),
		PSByteCode->GetBufferSize()
	};

	bool enableMSAA = DXRenderDeviceManager::GetInstance().CheckMSAAState();
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = BackBufferFormat;
	psoDesc.SampleDesc.Count = enableMSAA ? 4 : 1;
	psoDesc.SampleDesc.Quality = enableMSAA ? (DXRenderDeviceManager::GetInstance().GetMSAAQuality() - 1) : 0;
	psoDesc.DSVFormat = DepthStencilFormat;
	ThrowIfFailed(pD3DDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&PSO)));
}
