#pragma once
#include "Geometry.h"
#include "FrameResource.h"
#include "SystemTimer.h"
#include "Waves.h"



enum class RenderLayer : int
{
	Opaque = 0,
	Transparent,
	AlphaTested,
	Count
};


class RenderPass
{
public:

	RenderPass();

	virtual ~RenderPass();

public:

	virtual void		Initialize();

	virtual void		Tick(const SystemTimer& Timer, const XMFLOAT4X4& View, const XMFLOAT4X4& Proj, const XMFLOAT3& EyePos);

	virtual void		Draw(const SystemTimer& Timer);

protected:

	virtual void		BuildTextures();
	virtual void		BuildRootSignature();
	virtual void		BuildShadersAndInputLayout();
	virtual void		BuildShapeGeometry();
	virtual void		BuildWavesGeometry();
	virtual void		BuildBoxGeometry();
	virtual void		BuildMaterials();
	virtual void		BuildRenderItems();
	virtual void		BuildDescriptorHeaps();
	virtual void		BuildPSOs();

	virtual void		TickRenderPass(const SystemTimer& Timer, const XMFLOAT4X4& View, const XMFLOAT4X4& Proj, const XMFLOAT3& EyePos);
	virtual void		TickRenderItems(const SystemTimer& Timer);
	virtual void		TickMaterials(const SystemTimer& Timer);

	virtual void		DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

	float GetHillsHeight(float x, float z)const;
	XMFLOAT3 GetHillsNormal(float x, float z)const;

protected:

	ComPtr<ID3D12RootSignature> RootSignature = nullptr;
	ComPtr<ID3D12DescriptorHeap> SrvDescriptorHeap = nullptr;
	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> Geometries;
	std::unordered_map<std::string, std::unique_ptr<Texture>> Textures;
	std::unordered_map<std::string, std::unique_ptr<Material>> Materials;
	ComPtr<ID3D12DescriptorHeap> CBVHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> SRVDescriptorHeap = nullptr;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> Shaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> PSOs;
	std::vector<D3D12_INPUT_ELEMENT_DESC> InputLayout;
	std::unique_ptr<Waves> mWaves;

	PassConstants MainPassCB;
	bool IsWireframe = false;

	std::vector<std::unique_ptr<RenderItem>> AllRItems;
	std::vector<RenderItem*> OpaqueRitems;
	RenderItem* WavesRitem = nullptr;
	// Render items divided by PSO.
	std::vector<RenderItem*> RItemLayer[(int)RenderLayer::Count];

	UINT PassCbvOffset = 0;
};
