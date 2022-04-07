
#include "Filter/BlurFilter.h"
#include "DXRenderDeviceManager.h"

BlurFilter::BlurFilter(ID3D12Device* device,
	UINT width, UINT height,
	DXGI_FORMAT format)
{
	md3dDevice = device;

	mWidth = width;
	mHeight = height;
	mFormat = format;

	BuildResources();
}

void BlurFilter::Initialize()
{
	BuildPostProcessRootSignature();
	BuildShadersAndInputLayout();
	BuildBlurDescriptors();
	BuildPSOs();
}

ID3D12Resource* BlurFilter::Output()
{
	return mBlurMap0.Get();
}

void BlurFilter::BuildPostProcessRootSignature()
{
	ID3D12Device* pD3DDevice = DXRenderDeviceManager::GetInstance().GetD3DDevice();
	if (pD3DDevice == nullptr)
		return;

	CD3DX12_DESCRIPTOR_RANGE srvTable;
	srvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_DESCRIPTOR_RANGE uavTable;
	uavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[3];

	// 权重值参数
	slotRootParameter[0].InitAsConstants(12, 0);
	//输入纹理描述符
	slotRootParameter[1].InitAsDescriptorTable(1, &srvTable);
	//输出纹理描述符
	slotRootParameter[2].InitAsDescriptorTable(1, &uavTable);

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter,
		0, nullptr,
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
		IID_PPV_ARGS(mPostProcessRootSignature.GetAddressOf())));
}

void BlurFilter::BuildShadersAndInputLayout()
{
	mShaders["horzBlurCS"] = d3dUtil::CompileShader(L"Shaders\\Blur.hlsl", nullptr, "HorzBlurCS", "cs_5_0");
	mShaders["vertBlurCS"] = d3dUtil::CompileShader(L"Shaders\\Blur.hlsl", nullptr, "VertBlurCS", "cs_5_0");
}

void BlurFilter::BuildPSOs()
{
	//
	// 创建横向高斯模糊计算的计算管线状态对象
	//
	D3D12_COMPUTE_PIPELINE_STATE_DESC horzBlurPSO = {};
	horzBlurPSO.pRootSignature = mPostProcessRootSignature.Get();
	horzBlurPSO.CS =
	{
		reinterpret_cast<BYTE*>(mShaders["horzBlurCS"]->GetBufferPointer()),
		mShaders["horzBlurCS"]->GetBufferSize()
	};
	horzBlurPSO.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateComputePipelineState(&horzBlurPSO, IID_PPV_ARGS(&mPSOs["horzBlur"])));

	//
	// 创建纵向高斯模糊计算的计算管线状态对象
	//
	D3D12_COMPUTE_PIPELINE_STATE_DESC vertBlurPSO = {};
	vertBlurPSO.pRootSignature = mPostProcessRootSignature.Get();
	vertBlurPSO.CS =
	{
		reinterpret_cast<BYTE*>(mShaders["vertBlurCS"]->GetBufferPointer()),
		mShaders["vertBlurCS"]->GetBufferSize()
	};
	vertBlurPSO.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateComputePipelineState(&vertBlurPSO, IID_PPV_ARGS(&mPSOs["vertBlur"])));
}

void BlurFilter::BuildBlurDescriptors()
{
	ID3D12Device* pD3DDevice = DXRenderDeviceManager::GetInstance().GetD3DDevice();
	if (pD3DDevice == nullptr)
		return;

	// 先创建包含四个描述符的描述符堆
	UINT descriptorSize = DXRenderDeviceManager::GetInstance().GetConstantDescriptorSize();
	const int blurDescriptorCount = 4;
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = blurDescriptorCount;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;	// 描述符类型为CBV.SRV.UAV
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(pD3DDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mCbvSrvUavDescriptorHeap))); // 创建包含四个类型为CBV.SRV.UAV描述符的描述符堆

	CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvSrvUavDescriptorHeap->GetCPUDescriptorHandleForHeapStart()); // CPU可以看到的堆描述符
	//  将四个描述符分别映射到成员变量
	mBlur0CpuSrv = hCpuDescriptor;
	mBlur0CpuUav = hCpuDescriptor.Offset(1, descriptorSize);
	mBlur1CpuSrv = hCpuDescriptor.Offset(1, descriptorSize);
	mBlur1CpuUav = hCpuDescriptor.Offset(1, descriptorSize);

	CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvSrvUavDescriptorHeap->GetGPUDescriptorHandleForHeapStart()); // GPU可以看到的堆描述符
	mBlur0GpuSrv = hGpuDescriptor;
	mBlur0GpuUav = hGpuDescriptor.Offset(1, descriptorSize);
	mBlur1GpuSrv = hGpuDescriptor.Offset(1, descriptorSize);
	mBlur1GpuUav = hGpuDescriptor.Offset(1, descriptorSize);

	BuildDescriptors();
}

void BlurFilter::OnResize(UINT newWidth, UINT newHeight)
{
	if ((mWidth != newWidth) || (mHeight != newHeight))
	{
		mWidth = newWidth;
		mHeight = newHeight;

		BuildResources();

		// New resource, so we need new descriptors to that resource.
		BuildDescriptors();
	}
}

void BlurFilter::Execute(ID3D12GraphicsCommandList* cmdList,
	ID3D12Resource* input,  // 当前后台缓冲区内使用的缓冲区资源(本质上也是一张纹理)
	int blurCount)
{
	// 根据高斯函数计算出模糊核范围内每个像素点的权重值 高斯函数G(x) = exp(-x² / 2∝²)
	auto weights = CalcGaussWeights(2.5f);
	int blurRadius = (int)weights.size() / 2;

	// 将先前创建的两个纹理的描述符堆设置到命令列表中
	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavDescriptorHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	// 设置模糊处理计算着色器的根签名
	cmdList->SetComputeRootSignature(mPostProcessRootSignature.Get());
	// 设置模糊核根参数
	cmdList->SetComputeRoot32BitConstants(0, 1, &blurRadius, 0);
	// 设置权重值根参数
	cmdList->SetComputeRoot32BitConstants(0, (UINT)weights.size(), weights.data(), 1);

	// 将后台缓冲区资源状态从rendertarget该为CopySource 准备从该后台缓冲区中将数据拷贝到纹理A
	CD3DX12_RESOURCE_BARRIER RTToCopy = CD3DX12_RESOURCE_BARRIER::Transition(input,
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
	cmdList->ResourceBarrier(1, &RTToCopy);
	// 将纹理A从普通状态改为CopyDest 准备接受来自后台缓冲区的数据拷贝
	CD3DX12_RESOURCE_BARRIER CommonToCopy = CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap0.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
	cmdList->ResourceBarrier(1, &CommonToCopy);

	// 将后台缓冲区数据拷贝到纹理A
	cmdList->CopyResource(mBlurMap0.Get(), input);

	// 将纹理A的状态改为D3D12_RESOURCE_STATE_GENERIC_READ 准备将纹理A作为计算着色器的输入提交计算着色器读取
	CD3DX12_RESOURCE_BARRIER CopyToRead = CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap0.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
	cmdList->ResourceBarrier(1, &CopyToRead);
	// 将纹理B的状态改为D3D12_RESOURCE_STATE_UNORDERED_ACCESS 准备将纹理B作为计算着色器的输出
	CD3DX12_RESOURCE_BARRIER CommonToUA = CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap1.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	cmdList->ResourceBarrier(1, &CommonToUA);

	// 对图像进行blurCount次模糊处理，次数越大模糊程度越大
	for (int i = 0; i < blurCount; ++i)
	{
		// 先进行横向模糊计算，设置横向计算管线状态对象
		cmdList->SetPipelineState(mPSOs["horzBlur"].Get());
		// 设置计算着色器输入纹理描述符为A 输出纹理描述符为B
		cmdList->SetComputeRootDescriptorTable(1, mBlur0GpuSrv);
		cmdList->SetComputeRootDescriptorTable(2, mBlur1GpuUav);

		// 由于计算着色器开了(256,1,1)个线程，因此需要开启UINT numGroupsX = (UINT)ceilf(mWidth / 256.0f)个线程组
		UINT numGroupsX = (UINT)ceilf(mWidth / 256.0f);
		cmdList->Dispatch(numGroupsX, mHeight, 1);

		// 将纹理A的读取状态改为UA用作纵向计算着色器的输出
		CD3DX12_RESOURCE_BARRIER ReadToUA = CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap0.Get(),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		cmdList->ResourceBarrier(1, &ReadToUA);
		// 将纹理B的UA状态改为Read作为纵向计算着色器的输入(将横向计算着色器的输出作为纵向计算着色器的输入)
		CD3DX12_RESOURCE_BARRIER UAToRead = CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap1.Get(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
		cmdList->ResourceBarrier(1, &UAToRead);

		// 设置纵向计算管线状态对象
		cmdList->SetPipelineState(mPSOs["vertBlur"].Get());
		// 分别将B作为输入纹理 A作为输出纹理设置到根参数
		cmdList->SetComputeRootDescriptorTable(1, mBlur1GpuSrv);
		cmdList->SetComputeRootDescriptorTable(2, mBlur0GpuUav);

		// 由于计算着色器开了(1,256,1)个线程，因此需要开启UINT numGroupsY = (UINT)ceilf(mWidth / 256.0f)个线程组
		UINT numGroupsY = (UINT)ceilf(mHeight / 256.0f);
		cmdList->Dispatch(mWidth, numGroupsY, 1);

		// 将纹理A纹理设置为读取方便将其数据拷贝到后台缓冲区显示
		CD3DX12_RESOURCE_BARRIER UAToRead1 = CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap0.Get(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
		CD3DX12_RESOURCE_BARRIER ReadToUA1 = CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap1.Get(),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		cmdList->ResourceBarrier(1, &UAToRead1);

		cmdList->ResourceBarrier(1, &ReadToUA1);
	}

	CopyBlurToBackBuffer();
}

std::vector<float> BlurFilter::CalcGaussWeights(float sigma)
{
	float twoSigma2 = 2.0f * sigma * sigma;

	// Estimate the blur radius based on sigma since sigma controls the "width" of the bell curve.
	// For example, for sigma = 3, the width of the bell curve is 
	int blurRadius = (int)ceil(2.0f * sigma);

	assert(blurRadius <= MaxBlurRadius);

	std::vector<float> weights;
	weights.resize(2 * blurRadius + 1);

	float weightSum = 0.0f;

	for (int i = -blurRadius; i <= blurRadius; ++i)
	{
		float x = (float)i;

		weights[i + blurRadius] = expf(-x * x / twoSigma2);

		weightSum += weights[i + blurRadius];
	}

	// Divide by the sum so all the weights add up to 1.0.
	for (int i = 0; i < weights.size(); ++i)
	{
		weights[i] /= weightSum;
	}

	return weights;
}

void BlurFilter::BuildDescriptors()
{
	//
	// 在CPU中创建并填充四个CPU描述符
	//

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = mFormat;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

	uavDesc.Format = mFormat;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;
	// 为纹理A创建纹理资源和UAV描述符 在CPU创建因此放到CPU可见的描述符中
	md3dDevice->CreateShaderResourceView(mBlurMap0.Get(), &srvDesc, mBlur0CpuSrv);
	md3dDevice->CreateUnorderedAccessView(mBlurMap0.Get(), nullptr, &uavDesc, mBlur0CpuUav);
	// 为纹理B创建纹理资源和UAV描述符 在CPU创建因此放到CPU可见的描述符中
	md3dDevice->CreateShaderResourceView(mBlurMap1.Get(), &srvDesc, mBlur1CpuSrv);
	md3dDevice->CreateUnorderedAccessView(mBlurMap1.Get(), nullptr, &uavDesc, mBlur1CpuUav);
}

void BlurFilter::BuildResources()
{
	// Note, compressed formats cannot be used for UAV.  We get error like:
	// ERROR: ID3D11Device::CreateTexture2D: The format (0x4d, BC3_UNORM) 
	// cannot be bound as an UnorderedAccessView, or cast to a format that
	// could be bound as an UnorderedAccessView.  Therefore this format 
	// does not support D3D11_BIND_UNORDERED_ACCESS.

	// 为横向/纵向模糊处理分别创建一个纹理资源
	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = mWidth;
	texDesc.Height = mHeight;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = mFormat;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	// 因为创建的纹理既要用做计算着色其的输入又要用作计算着色器的输出因此一定要定义为D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;  

	CD3DX12_HEAP_PROPERTIES Blur0Res = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&Blur0Res,
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&mBlurMap0)));

	CD3DX12_HEAP_PROPERTIES Blur1Res = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&Blur1Res,
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&mBlurMap1)));
}

void BlurFilter::CopyBlurToBackBuffer()
{
	ID3D12Device* pD3DDevice = DXRenderDeviceManager::GetInstance().GetD3DDevice();
	ID3D12GraphicsCommandList* pCommandList = DXRenderDeviceManager::GetInstance().GetCommandList();

	if (pD3DDevice == nullptr || pCommandList == nullptr)
		return;

	CD3DX12_RESOURCE_BARRIER CopyRes = CD3DX12_RESOURCE_BARRIER::Transition(DXRenderDeviceManager::GetInstance().GetCurrentBackgroundBuffer(),
		D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
	// Prepare to copy blurred output to the back buffer.
	pCommandList->ResourceBarrier(1, &CopyRes);

	pCommandList->CopyResource(DXRenderDeviceManager::GetInstance().GetCurrentBackgroundBuffer(), Output());

	CD3DX12_RESOURCE_BARRIER PresentRes = CD3DX12_RESOURCE_BARRIER::Transition(DXRenderDeviceManager::GetInstance().GetCurrentBackgroundBuffer(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
	// Transition to PRESENT state.
	pCommandList->ResourceBarrier(1, &PresentRes);

}
