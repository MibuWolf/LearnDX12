#pragma once
#include "Geometry.h"
#include "FrameResource.h"
#include "SystemTimer.h"

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

	virtual void		LoadTextures();
	virtual void		BuildRootSignature();
	virtual void		BuildShadersAndInputLayout();
	virtual void		BuildShapeGeometry();
	virtual void		BuildMaterials();
	virtual void		BuildRenderItems();
	virtual void		BuildDescriptorHeaps();
	virtual void		BuildConstantBufferViews();
	virtual void		BuildPSOs();

	virtual void		TickRenderPass(const SystemTimer& Timer, const XMFLOAT4X4& View, const XMFLOAT4X4& Proj, const XMFLOAT3& EyePos);
	virtual void		TickRenderItems(const SystemTimer& Timer);
	virtual void		TickMaterials(const SystemTimer& Timer);

	virtual void		DrawRenderItems(const SystemTimer& Timer);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

protected:

	ComPtr<ID3D12RootSignature> RootSignature = nullptr;
	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> Geometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> Materials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> Textures;
	ComPtr<ID3D12DescriptorHeap> CBVHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> SRVDescriptorHeap = nullptr;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> Shaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> PSOs;
	ComPtr<ID3D12DescriptorHeap> SrvDescriptorHeap = nullptr;
	std::vector<D3D12_INPUT_ELEMENT_DESC> InputLayout;

	PassConstants MainPassCB;
	bool IsWireframe = false;

	std::vector<std::unique_ptr<RenderItem>> AllRItems;
	std::vector<RenderItem*> OpaqueRitems;

	UINT PassCbvOffset = 0;
	UINT mInstanceCount = 0;
};
