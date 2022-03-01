#include "Include\WavesRenderPass.h"
#include "DXRenderDeviceManager.h"

WavesRenderPass::WavesRenderPass()
{

}


WavesRenderPass::~WavesRenderPass()
{

}


void WavesRenderPass::Initialize()
{
	mWaves = std::make_unique<Waves>(128, 128, 1.0f, 0.03f, 4.0f, 0.2f);

	RenderPass::Initialize();
}

void WavesRenderPass::BuildRootSignature()
{
    ID3D12Device* pD3DDevice = DXRenderDeviceManager::GetInstance().GetD3DDevice();
    ID3D12GraphicsCommandList* pCommandList = DXRenderDeviceManager::GetInstance().GetCommandList();

    if (pD3DDevice == nullptr || pCommandList == nullptr)
        return;

    // 此处使用根描述符来创建根参数
    CD3DX12_ROOT_PARAMETER slotRootParameter[2];

    // 创建两个根描述类型的根参数
    slotRootParameter[0].InitAsConstantBufferView(0);
    slotRootParameter[1].InitAsConstantBufferView(1);

    // A root signature is an array of root parameters.
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

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

    ThrowIfFailed(md3dDevice->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}
