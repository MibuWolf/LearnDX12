#include "Base/RenderPass.h"
#include "DXRenderDeviceManager.h"
#include "Base/GeometryGenerator.h"
#include "Base/DDSTextureLoader.h"

RenderPass::RenderPass()
{

}


RenderPass::~RenderPass()
{

}


void RenderPass::Initialize()
{
	LoadTextures();
	// 创建根签名
	BuildRootSignature();
	// 编译着色器并设置着色器输入顶点参数布局
	BuildShadersAndInputLayout();
	// 构建基本静态模型网格信息(将所有静态模型的顶点放到一个顶点缓冲区，索引也整合到一个索引缓冲区)
	BuildShapeGeometry();
	BuildSkullGeometry();
	// 初始化材质信息
	BuildMaterials();
	// 为每个网格模型的渲染实例记录渲染项信息
	BuildRenderItems();
	// 创建一个renderpass常量缓冲区和OpaqueRitems.size()个对象常量缓冲区的帧资源
	DXRenderDeviceManager::GetInstance().CreateFrameResources(AllRItems.size(), Materials.size());
	// 为三个FrameRender(每个FrameRender分别有对象常量缓冲区和RenderPass常量缓冲区两个缓冲区)的常量缓冲区创建描述符堆
	BuildDescriptorHeaps();
	// 为三个FrameRender中的每个缓冲区创建描述符并存储在描述符堆中
	//BuildConstantBufferViews();
	// 创建针对RenderPass的渲染管线状态对象
	BuildPSOs();
}


void RenderPass::Tick(const SystemTimer& Timer, const XMFLOAT4X4& View, const XMFLOAT4X4& Proj, const XMFLOAT3& EyePos)
{
	TickRenderItems(Timer);
	TickMaterials(Timer);
	TickRenderPass(Timer, View, Proj, EyePos);
}

void RenderPass::Draw(const SystemTimer& Timer)
{
	ID3D12Device* pD3DDevice = DXRenderDeviceManager::GetInstance().GetD3DDevice();
	ID3D12GraphicsCommandList* pCommandList = DXRenderDeviceManager::GetInstance().GetCommandList();
	FrameResource* CurrentFrameResource = DXRenderDeviceManager::GetInstance().GetCurrentFrameResource();
	if (pD3DDevice == nullptr || pCommandList == nullptr || CurrentFrameResource == nullptr)
		return;

	UINT FrameResourceIndex = DXRenderDeviceManager::GetInstance().GetCurrentFrameResourceIndex();
	UINT ConstantDDescriptorSize = DXRenderDeviceManager::GetInstance().GetConstantDescriptorSize();

	// 设置渲染流水线状态
	pCommandList->SetPipelineState(PSOs["opaque"].Get());
	// 设置纹理描述符堆
	ID3D12DescriptorHeap* descriptorHeaps[] = { SrvDescriptorHeap.Get() };
	pCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	// 设置根签名
	pCommandList->SetGraphicsRootSignature(RootSignature.Get());

	// 绑定渲染Pass参数
	auto passCB = CurrentFrameResource->PassCB->Resource();
	pCommandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

	// 绑定材质数组
	auto matBuffer = CurrentFrameResource->MaterialCB->Resource();
	pCommandList->SetGraphicsRootShaderResourceView(2, matBuffer->GetGPUVirtualAddress());

	// 设置天空球材质数据
	CD3DX12_GPU_DESCRIPTOR_HANDLE skyTexDescriptor(SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	skyTexDescriptor.Offset(mSkyTexHeapIndex, ConstantDDescriptorSize);
	pCommandList->SetGraphicsRootDescriptorTable(3, skyTexDescriptor);

	// 绑定纹理数组
	pCommandList->SetGraphicsRootDescriptorTable(4, SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	DrawRenderItems(Timer, RenderLayer::Opaque);

	// 设置天空球渲染流水线状态
	pCommandList->SetPipelineState(PSOs["sky"].Get());
	DrawRenderItems(Timer, RenderLayer::Sky);
}



void RenderPass::LoadTextures()
{
	ID3D12Device* pD3DDevice = DXRenderDeviceManager::GetInstance().GetD3DDevice();
	ID3D12GraphicsCommandList* pCommandList = DXRenderDeviceManager::GetInstance().GetCommandList();
	if (pD3DDevice == nullptr || pCommandList == nullptr)
		return;
	std::vector<std::string> texNames =
	{
		"bricksDiffuseMap",
		"tileDiffuseMap",
		"defaultDiffuseMap",
		"skyCubeMap"			// 天空盒的环境贴图
	};

	std::vector<std::wstring> texFilenames =
	{
		L"Textures/bricks2.dds",
		L"Textures/tile.dds",
		L"Textures/white1x1.dds",
		L"Textures/grasscube1024.dds"			// 像加载普通纹理一样加载环境立方体贴图贴图
	};

	for (int i = 0; i < (int)texNames.size(); ++i)
	{
		auto texMap = std::make_unique<Texture>();
		texMap->Name = texNames[i];
		texMap->Filename = texFilenames[i];
		ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(pD3DDevice,
			pCommandList, texMap->Filename.c_str(),
			texMap->Resource, texMap->UploadHeap));

		Textures[texMap->Name] = std::move(texMap);
	}
}

void RenderPass::BuildRootSignature()
{
	ID3D12Device* pD3DDevice = DXRenderDeviceManager::GetInstance().GetD3DDevice();
	ID3D12GraphicsCommandList* pCommandList = DXRenderDeviceManager::GetInstance().GetCommandList();

	if (pD3DDevice == nullptr || pCommandList == nullptr)
		return;

	CD3DX12_DESCRIPTOR_RANGE texTable0;
	texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);	// 纹理描述符表 1个纹理描述符 对应0号寄存器 0号空间

	CD3DX12_DESCRIPTOR_RANGE texTable1;
	texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 1, 0);	// 纹理描述符表 5个纹理描述符 对应1号寄存器 0号空间

	// 五个根参数
	CD3DX12_ROOT_PARAMETER slotRootParameter[5];

	// 从更新频率高到低
	slotRootParameter[0].InitAsConstantBufferView(0);	// 对应每个渲染对象的常量缓冲区数据映射到着色器中的b0寄存器
	slotRootParameter[1].InitAsConstantBufferView(1);	// 对应每个渲染Pass的常量缓冲区数据映射到着色器中的b1寄存器
	slotRootParameter[2].InitAsShaderResourceView(0, 1); // 对应材质数组的结构化缓冲区数据映射到着色器中t0寄存器的space1空间
	slotRootParameter[3].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);		// 根描述符表对应的是放置在t0的环境贴图(立方体贴图)
	slotRootParameter[4].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);		// 描述符表对对应的是放置在t1的普通纹理贴图数组


	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(5, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
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
		IID_PPV_ARGS(RootSignature.GetAddressOf())));
}


void RenderPass::BuildShadersAndInputLayout()
{
	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	Shaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_1");
	Shaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "PS", "ps_5_1");

	Shaders["skyVS"] = d3dUtil::CompileShader(L"Shaders\\Sky.hlsl", nullptr, "VS", "vs_5_1");  // 对天空球的绘制与普通模型不同，因此需要对其做区分
	Shaders["skyPS"] = d3dUtil::CompileShader(L"Shaders\\Sky.hlsl", nullptr, "PS", "ps_5_1");

	InputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}


void RenderPass::BuildShapeGeometry()
{
	ID3D12Device* pD3DDevice = DXRenderDeviceManager::GetInstance().GetD3DDevice();
	ID3D12GraphicsCommandList* pCommandList = DXRenderDeviceManager::GetInstance().GetCommandList();

	if (pD3DDevice == nullptr || pCommandList == nullptr)
		return;

	// 生成场景中的几何体模型数据，其中的球体可用作天空球
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);

	// 创建顶点和索引缓冲取并将创建出来的这些模型数据整理后存入顶点/索引缓冲区
	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size();
	UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
	UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();

	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size();
	UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
	UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.BaseVertexLocation = gridVertexOffset;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

	//
	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.
	//

	auto totalVertexCount =
		box.Vertices.size() +
		grid.Vertices.size() +
		sphere.Vertices.size() +
		cylinder.Vertices.size();

	std::vector<Vertex> vertices(totalVertexCount);

	UINT k = 0;
	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Normal = box.Vertices[i].Normal;
		vertices[k].TexC = box.Vertices[i].TexC;
	}

	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Normal = grid.Vertices[i].Normal;
		vertices[k].TexC = grid.Vertices[i].TexC;
	}

	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Normal = sphere.Vertices[i].Normal;
		vertices[k].TexC = sphere.Vertices[i].TexC;
	}

	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Normal = cylinder.Vertices[i].Normal;
		vertices[k].TexC = cylinder.Vertices[i].TexC;
	}

	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
	indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "shapeGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(pD3DDevice,
		pCommandList, vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(pD3DDevice,
		pCommandList, indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;

	Geometries[geo->Name] = std::move(geo);
}

void RenderPass::BuildSkullGeometry()
{
	ID3D12Device* pD3DDevice = DXRenderDeviceManager::GetInstance().GetD3DDevice();
	ID3D12GraphicsCommandList* pCommandList = DXRenderDeviceManager::GetInstance().GetCommandList();

	if (pD3DDevice == nullptr || pCommandList == nullptr)
		return;


	std::ifstream fin("Models/skull.txt");

	if (!fin)
	{
		MessageBox(0, L"Models/skull.txt not found.", 0, 0);
		return;
	}

	UINT vcount = 0;
	UINT tcount = 0;
	std::string ignore;

	fin >> ignore >> vcount;
	fin >> ignore >> tcount;
	fin >> ignore >> ignore >> ignore >> ignore;

	XMFLOAT3 vMinf3(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
	XMFLOAT3 vMaxf3(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);

	XMVECTOR vMin = XMLoadFloat3(&vMinf3);
	XMVECTOR vMax = XMLoadFloat3(&vMaxf3);

	std::vector<Vertex> vertices(vcount);
	for (UINT i = 0; i < vcount; ++i)
	{
		fin >> vertices[i].Pos.x >> vertices[i].Pos.y >> vertices[i].Pos.z;
		fin >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;

		vertices[i].TexC = { 0.0f, 0.0f };

		XMVECTOR P = XMLoadFloat3(&vertices[i].Pos);

		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}

	BoundingBox bounds;
	XMStoreFloat3(&bounds.Center, 0.5f * (vMin + vMax));
	XMStoreFloat3(&bounds.Extents, 0.5f * (vMax - vMin));

	fin >> ignore;
	fin >> ignore;
	fin >> ignore;

	std::vector<std::int32_t> indices(3 * tcount);
	for (UINT i = 0; i < tcount; ++i)
	{
		fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
	}

	fin.close();

	//
	// Pack the indices of all the meshes into one index buffer.
	//

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::int32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "skullGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(pD3DDevice,
		pCommandList, vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(pD3DDevice,
		pCommandList, indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;
	submesh.Bounds = bounds;

	geo->DrawArgs["skull"] = submesh;

	Geometries[geo->Name] = std::move(geo);
}

void RenderPass::BuildMaterials()
{
	auto bricks0 = std::make_unique<Material>();
	bricks0->Name = "bricks0";
	bricks0->MatCBIndex = 0;
	bricks0->DiffuseSrvHeapIndex = 0;
	bricks0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	bricks0->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	bricks0->Roughness = 0.3f;

	auto tile0 = std::make_unique<Material>();
	tile0->Name = "tile0";
	tile0->MatCBIndex = 1;
	tile0->DiffuseSrvHeapIndex = 1;
	tile0->DiffuseAlbedo = XMFLOAT4(0.9f, 0.9f, 0.9f, 1.0f);
	tile0->FresnelR0 = XMFLOAT3(0.2f, 0.2f, 0.2f);
	tile0->Roughness = 0.1f;

	auto mirror0 = std::make_unique<Material>();
	mirror0->Name = "mirror0";
	mirror0->MatCBIndex = 2;
	mirror0->DiffuseSrvHeapIndex = 2;
	mirror0->DiffuseAlbedo = XMFLOAT4(0.0f, 0.0f, 0.1f, 1.0f);
	mirror0->FresnelR0 = XMFLOAT3(0.98f, 0.97f, 0.95f);
	mirror0->Roughness = 0.1f;

	auto skullMat = std::make_unique<Material>();
	skullMat->Name = "skullMat";
	skullMat->MatCBIndex = 3;
	skullMat->DiffuseSrvHeapIndex = 2;
	skullMat->DiffuseAlbedo = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
	skullMat->FresnelR0 = XMFLOAT3(0.2f, 0.2f, 0.2f);
	skullMat->Roughness = 0.2f;

	auto sky = std::make_unique<Material>();
	sky->Name = "sky";									
	sky->MatCBIndex = 4;						// 天空球的材质数据在材质缓冲区中的索引
	sky->DiffuseSrvHeapIndex = 3;				// 天空球使用的纹理描述符在纹理描述符堆中的索引
	sky->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);	// 漫反射反照率
	sky->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);	// 0°时的反照率
	sky->Roughness = 1.0f;			// 粗糙度

	Materials["bricks0"] = std::move(bricks0);
	Materials["tile0"] = std::move(tile0);
	Materials["mirror0"] = std::move(mirror0);
	Materials["skullMat"] = std::move(skullMat);
	Materials["sky"] = std::move(sky);
}


void RenderPass::BuildRenderItems()
{
	auto skyRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&skyRitem->World, XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));	// 将球体缩放5000倍作为天空球
	skyRitem->TexTransform = MathHelper::Identity4x4();
	skyRitem->ObjCBIndex = 0;
	skyRitem->Mat = Materials["sky"].get();	// 记录天空球使用的材质信息
	skyRitem->Geo = Geometries["shapeGeo"].get();	// 模型信息
	skyRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST; // 绘制的图元类型
	skyRitem->IndexCount = skyRitem->Geo->DrawArgs["sphere"].IndexCount;	// 在所有模型中的索引
	skyRitem->StartIndexLocation = skyRitem->Geo->DrawArgs["sphere"].StartIndexLocation;	// 索引缓冲区开始位置
	skyRitem->BaseVertexLocation = skyRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;	// 顶点缓冲区开始位置

	mRitemLayer[(int)RenderLayer::Sky].push_back(skyRitem.get());
	AllRItems.push_back(std::move(skyRitem));

	auto boxRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem->World, XMMatrixScaling(2.0f, 1.0f, 2.0f) * XMMatrixTranslation(0.0f, 0.5f, 0.0f));
	XMStoreFloat4x4(&boxRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	boxRitem->ObjCBIndex = 1;
	boxRitem->Mat = Materials["bricks0"].get();
	boxRitem->Geo = Geometries["shapeGeo"].get();
	boxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::Opaque].push_back(boxRitem.get());
	AllRItems.push_back(std::move(boxRitem));

	auto skullRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&skullRitem->World, XMMatrixScaling(0.4f, 0.4f, 0.4f) * XMMatrixTranslation(0.0f, 1.0f, 0.0f));
	skullRitem->TexTransform = MathHelper::Identity4x4();
	skullRitem->ObjCBIndex = 2;
	skullRitem->Mat = Materials["skullMat"].get();
	skullRitem->Geo = Geometries["skullGeo"].get();
	skullRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skullRitem->IndexCount = skullRitem->Geo->DrawArgs["skull"].IndexCount;
	skullRitem->StartIndexLocation = skullRitem->Geo->DrawArgs["skull"].StartIndexLocation;
	skullRitem->BaseVertexLocation = skullRitem->Geo->DrawArgs["skull"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::Opaque].push_back(skullRitem.get());
	AllRItems.push_back(std::move(skullRitem));

	auto gridRitem = std::make_unique<RenderItem>();
	gridRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&gridRitem->TexTransform, XMMatrixScaling(8.0f, 8.0f, 1.0f));
	gridRitem->ObjCBIndex = 3;
	gridRitem->Mat = Materials["tile0"].get();
	gridRitem->Geo = Geometries["shapeGeo"].get();
	gridRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::Opaque].push_back(gridRitem.get());
	AllRItems.push_back(std::move(gridRitem));

	XMMATRIX brickTexTransform = XMMatrixScaling(1.5f, 2.0f, 1.0f);
	UINT objCBIndex = 4;
	for (int i = 0; i < 5; ++i)
	{
		auto leftCylRitem = std::make_unique<RenderItem>();
		auto rightCylRitem = std::make_unique<RenderItem>();
		auto leftSphereRitem = std::make_unique<RenderItem>();
		auto rightSphereRitem = std::make_unique<RenderItem>();

		XMMATRIX leftCylWorld = XMMatrixTranslation(-5.0f, 1.5f, -10.0f + i * 5.0f);
		XMMATRIX rightCylWorld = XMMatrixTranslation(+5.0f, 1.5f, -10.0f + i * 5.0f);

		XMMATRIX leftSphereWorld = XMMatrixTranslation(-5.0f, 3.5f, -10.0f + i * 5.0f);
		XMMATRIX rightSphereWorld = XMMatrixTranslation(+5.0f, 3.5f, -10.0f + i * 5.0f);

		XMStoreFloat4x4(&leftCylRitem->World, rightCylWorld);
		XMStoreFloat4x4(&leftCylRitem->TexTransform, brickTexTransform);
		leftCylRitem->ObjCBIndex = objCBIndex++;
		leftCylRitem->Mat = Materials["bricks0"].get();
		leftCylRitem->Geo = Geometries["shapeGeo"].get();
		leftCylRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftCylRitem->IndexCount = leftCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		leftCylRitem->StartIndexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		leftCylRitem->BaseVertexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

		XMStoreFloat4x4(&rightCylRitem->World, leftCylWorld);
		XMStoreFloat4x4(&rightCylRitem->TexTransform, brickTexTransform);
		rightCylRitem->ObjCBIndex = objCBIndex++;
		rightCylRitem->Mat = Materials["bricks0"].get();
		rightCylRitem->Geo = Geometries["shapeGeo"].get();
		rightCylRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightCylRitem->IndexCount = rightCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		rightCylRitem->StartIndexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		rightCylRitem->BaseVertexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

		XMStoreFloat4x4(&leftSphereRitem->World, leftSphereWorld);
		leftSphereRitem->TexTransform = MathHelper::Identity4x4();
		leftSphereRitem->ObjCBIndex = objCBIndex++;
		leftSphereRitem->Mat = Materials["mirror0"].get();
		leftSphereRitem->Geo = Geometries["shapeGeo"].get();
		leftSphereRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftSphereRitem->IndexCount = leftSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		leftSphereRitem->StartIndexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		leftSphereRitem->BaseVertexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

		XMStoreFloat4x4(&rightSphereRitem->World, rightSphereWorld);
		rightSphereRitem->TexTransform = MathHelper::Identity4x4();
		rightSphereRitem->ObjCBIndex = objCBIndex++;
		rightSphereRitem->Mat = Materials["mirror0"].get();
		rightSphereRitem->Geo = Geometries["shapeGeo"].get();
		rightSphereRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightSphereRitem->IndexCount = rightSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		rightSphereRitem->StartIndexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		rightSphereRitem->BaseVertexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

		mRitemLayer[(int)RenderLayer::Opaque].push_back(leftCylRitem.get());
		mRitemLayer[(int)RenderLayer::Opaque].push_back(rightCylRitem.get());
		mRitemLayer[(int)RenderLayer::Opaque].push_back(leftSphereRitem.get());
		mRitemLayer[(int)RenderLayer::Opaque].push_back(rightSphereRitem.get());

		AllRItems.push_back(std::move(leftCylRitem));
		AllRItems.push_back(std::move(rightCylRitem));
		AllRItems.push_back(std::move(leftSphereRitem));
		AllRItems.push_back(std::move(rightSphereRitem));
	}
}



void RenderPass::BuildDescriptorHeaps()
{
	ID3D12Device* pD3DDevice = DXRenderDeviceManager::GetInstance().GetD3DDevice();
	if (pD3DDevice == nullptr)
		return;

	UINT CbvSrvDescriptorSize = DXRenderDeviceManager::GetInstance().GetConstantDescriptorSize();
	//
	// 创建存储纹理贴图SRV描述符的SVR描述符堆
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 5;	// 该描述符堆存储5个纹理描述符
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;	// 描述符类型为SRV/CBV/UAV
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(pD3DDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&SrvDescriptorHeap)));

	//
	// 为每个纹理资源创建描述并填充到该描述符堆
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	auto bricksTex = Textures["bricksDiffuseMap"]->Resource;
	auto tileTex = Textures["tileDiffuseMap"]->Resource;
	auto whiteTex = Textures["defaultDiffuseMap"]->Resource;
	auto skyTex = Textures["skyCubeMap"]->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = bricksTex->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = bricksTex->GetDesc().MipLevels;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	pD3DDevice->CreateShaderResourceView(bricksTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, CbvSrvDescriptorSize);

	srvDesc.Format = tileTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = tileTex->GetDesc().MipLevels;
	pD3DDevice->CreateShaderResourceView(tileTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, CbvSrvDescriptorSize);

	srvDesc.Format = whiteTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = whiteTex->GetDesc().MipLevels;
	pD3DDevice->CreateShaderResourceView(whiteTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, CbvSrvDescriptorSize);

	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;  // 指定环境贴图类型为立方体贴图
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = skyTex->GetDesc().MipLevels;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	srvDesc.Format = skyTex->GetDesc().Format;					// 指定纹理格式
	pD3DDevice->CreateShaderResourceView(skyTex.Get(), &srvDesc, hDescriptor);	// 为环境贴图创建纹理描述符

	mSkyTexHeapIndex = 3;			// 记录用于天空盒/天空球的环境贴图描述符在纹理描述符堆中的索引偏移值为3，为的是后续使用时可以快速找到环境贴图纹理描述符
}


void RenderPass::BuildConstantBufferViews()
{
	ID3D12Device* pD3DDevice = DXRenderDeviceManager::GetInstance().GetD3DDevice();
	if (pD3DDevice == nullptr)
		return;

	UINT ConstantDDescriptorSize = DXRenderDeviceManager::GetInstance().GetConstantDescriptorSize();
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	UINT objCount = (UINT)OpaqueRitems.size();
	UINT matCount = (UINT)Materials.size();

	// Need a CBV descriptor for each object for each frame resource.
	for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
	{
		FrameResource* FrameRes = DXRenderDeviceManager::GetInstance().GetFrameResource(frameIndex);
		if (FrameRes == nullptr)
			continue;

		auto objectCB = FrameRes->ObjectCB->Resource();
		for (UINT i = 0; i < objCount; ++i)
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = objectCB->GetGPUVirtualAddress();

			// 常量缓冲区资源描述符大小objCBByteSize
			cbAddress += i * objCBByteSize;

			// Offset to the object cbv in the descriptor heap.
			int heapIndex = frameIndex * objCount + frameIndex * matCount + i;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(CBVHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, ConstantDDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = objCBByteSize;

			pD3DDevice->CreateConstantBufferView(&cbvDesc, handle);
		}

		auto materialCB = FrameRes->MaterialCB->Resource();
		for (UINT i = 0; i < matCount; ++i)
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = materialCB->GetGPUVirtualAddress();

			// 常量缓冲区资源描述符大小objCBByteSize
			cbAddress += i * matCBByteSize;

			// Offset to the object cbv in the descriptor heap.
			int heapIndex = (frameIndex + 1) * objCount + frameIndex * matCount + i;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(CBVHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, ConstantDDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = matCBByteSize;

			pD3DDevice->CreateConstantBufferView(&cbvDesc, handle);
		}
	}

	UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

	// Last three descriptors are the pass CBVs for each frame resource.
	for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
	{
		FrameResource* FrameRes = DXRenderDeviceManager::GetInstance().GetFrameResource(frameIndex);
		if (FrameRes == nullptr)
			continue;

		auto passCB = FrameRes->PassCB->Resource();
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress = passCB->GetGPUVirtualAddress();

		// Offset to the pass cbv in the descriptor heap.
		int heapIndex = PassCbvOffset + frameIndex;
		auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(CBVHeap->GetCPUDescriptorHandleForHeapStart());
		handle.Offset(heapIndex, ConstantDDescriptorSize);

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = cbAddress;
		cbvDesc.SizeInBytes = passCBByteSize;

		pD3DDevice->CreateConstantBufferView(&cbvDesc, handle);
	}
}

void RenderPass::BuildPSOs()
{
	ID3D12Device* pD3DDevice = DXRenderDeviceManager::GetInstance().GetD3DDevice();
	if (pD3DDevice == nullptr)
		return;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;


	//
	// PSO for opaque objects.
	//
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { InputLayout.data(), (UINT)InputLayout.size() };
	opaquePsoDesc.pRootSignature = RootSignature.Get();
	opaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(Shaders["standardVS"]->GetBufferPointer()),
		Shaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(Shaders["opaquePS"]->GetBufferPointer()),
		Shaders["opaquePS"]->GetBufferSize()
	};
	bool enableMSAA = DXRenderDeviceManager::GetInstance().CheckMSAAState();
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	opaquePsoDesc.SampleDesc.Count = enableMSAA ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = enableMSAA ? (DXRenderDeviceManager::GetInstance().GetMSAAQuality() - 1) : 0;
	opaquePsoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	ThrowIfFailed(pD3DDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&PSOs["opaque"])));

	//
	// 天空球与普通物体对象的渲染状态对象基本相同因此先复用普通对象的渲染状态对象再做稍许调整
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPsoDesc = opaquePsoDesc;

	// 因为相机处于天空球内部因此对天空球的绘制不需要背面剔除
	skyPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	// 绘制是的深度比较函数是小于等于是因为默认深度为1当距离较远是深度也近似为1而只用D3D12_COMPARISON_FUNC_LESS
	// 则可能导致较远出的包围球像素不能通过深度比较测试
	skyPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	skyPsoDesc.pRootSignature = RootSignature.Get();
	skyPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(Shaders["skyVS"]->GetBufferPointer()),
		Shaders["skyVS"]->GetBufferSize()
	};
	skyPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(Shaders["skyPS"]->GetBufferPointer()),
		Shaders["skyPS"]->GetBufferSize()
	};
	ThrowIfFailed(pD3DDevice->CreateGraphicsPipelineState(&skyPsoDesc, IID_PPV_ARGS(&PSOs["sky"])));


}



void RenderPass::TickRenderPass(const SystemTimer& Timer, const XMFLOAT4X4& View, const XMFLOAT4X4& Proj, const XMFLOAT3& EyePos)
{
	ID3D12Device* pD3DDevice = DXRenderDeviceManager::GetInstance().GetD3DDevice();
	FrameResource* CurrentFrameRender = DXRenderDeviceManager::GetInstance().GetCurrentFrameResource();
	if (pD3DDevice == nullptr || CurrentFrameRender == nullptr)
		return;

	XMMATRIX view = XMLoadFloat4x4(&View);
	XMMATRIX proj = XMLoadFloat4x4(&Proj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMVECTOR viewVec = XMMatrixDeterminant(view);
	XMVECTOR projVec = XMMatrixDeterminant(proj);
	XMVECTOR viewProjVec = XMMatrixDeterminant(viewProj);
	XMMATRIX invView = XMMatrixInverse(&viewVec, view);
	XMMATRIX invProj = XMMatrixInverse(&projVec, proj);
	XMMATRIX invViewProj = XMMatrixInverse(&viewProjVec, viewProj);

	XMStoreFloat4x4(&MainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&MainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&MainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&MainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&MainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&MainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	MainPassCB.EyePosW = EyePos;
	MainPassCB.RenderTargetSize = XMFLOAT2((float)1280.0f, (float)768.0f);
	MainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / 1280.0f, 1.0f / 768.0f);
	MainPassCB.NearZ = 1.0f;
	MainPassCB.FarZ = 1000.0f;
	MainPassCB.TotalTime = Timer.TotalTime();
	MainPassCB.DeltaTime = Timer.DeltaTime();

	MainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	MainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	MainPassCB.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
	MainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	MainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	MainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	MainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

	auto currPassCB = CurrentFrameRender->PassCB.get();
	currPassCB->CopyData(0, MainPassCB);
}

void RenderPass::TickRenderItems(const SystemTimer& Timer)
{
	ID3D12Device* pD3DDevice = DXRenderDeviceManager::GetInstance().GetD3DDevice();
	FrameResource* CurrentFrameRender = DXRenderDeviceManager::GetInstance().GetCurrentFrameResource();
	if (pD3DDevice == nullptr || CurrentFrameRender == nullptr)
		return;

	auto currObjectCB = CurrentFrameRender->ObjectCB.get();
	for (auto& Item : AllRItems)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if (Item->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&Item->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&Item->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));
			objConstants.MaterialIndex = Item->Mat->MatCBIndex;

			currObjectCB->CopyData(Item->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			Item->NumFramesDirty--;
		}
	}
}

void RenderPass::TickMaterials(const SystemTimer& Timer)
{
	ID3D12Device* pD3DDevice = DXRenderDeviceManager::GetInstance().GetD3DDevice();
	FrameResource* CurrentFrameRender = DXRenderDeviceManager::GetInstance().GetCurrentFrameResource();
	if (pD3DDevice == nullptr || CurrentFrameRender == nullptr)
		return;

	auto currMatCB = CurrentFrameRender->MaterialCB.get();
	for (auto& e : Materials)
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matData;
			matData.DiffuseAlbedo = mat->DiffuseAlbedo;
			matData.FresnelR0 = mat->FresnelR0;
			matData.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matData.MatTransform, XMMatrixTranspose(matTransform));
			matData.DiffuseMapIndex = mat->DiffuseSrvHeapIndex;

			currMatCB->CopyData(mat->MatCBIndex, matData);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void RenderPass::DrawRenderItems(const SystemTimer& Timer, RenderLayer LayerType)
{
	ID3D12GraphicsCommandList* pCommandList = DXRenderDeviceManager::GetInstance().GetCommandList();
	FrameResource* CurrentFrameResource = DXRenderDeviceManager::GetInstance().GetCurrentFrameResource();
	UINT FrameResourceIndex = DXRenderDeviceManager::GetInstance().GetCurrentFrameResourceIndex();
	if (pCommandList == nullptr || CurrentFrameResource == nullptr)
		return;

	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));
	UINT ConstantDDescriptorSize = DXRenderDeviceManager::GetInstance().GetConstantDescriptorSize();
	auto objectCB = CurrentFrameResource->ObjectCB->Resource();

	// For each render item...
	for (size_t i = 0; i < mRitemLayer[int(LayerType)].size(); ++i)
	{
		auto ri = mRitemLayer[int(LayerType)][i];

		D3D12_VERTEX_BUFFER_VIEW vertexBuffView = ri->Geo->VertexBufferView();
		D3D12_INDEX_BUFFER_VIEW indexBuffView = ri->Geo->IndexBufferView();
		pCommandList->IASetVertexBuffers(0, 1, &vertexBuffView);
		pCommandList->IASetIndexBuffer(&indexBuffView);
		pCommandList->IASetPrimitiveTopology(ri->PrimitiveType);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;

		pCommandList->SetGraphicsRootConstantBufferView(0, objCBAddress);

		pCommandList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}


std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> RenderPass::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}