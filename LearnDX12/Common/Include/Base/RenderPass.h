#pragma once
#include "Geometry.h"
#include "FrameResource.h"


class RenderPass
{
public:

	RenderPass();

	~RenderPass();

public:

	void		InitRenderPass();

	void		DrawRenderItems();

protected:

	void		BuildRootSignature();
	void		BuildShadersAndInputLayout();
	void		BuildShapeGeometry();
	void		BuildRenderItems();
	void		BuildDescriptorHeaps();
	void		BuildConstantBufferViews();
	void		BuildPSOs();



private:

	ComPtr<ID3D12RootSignature> RootSignature = nullptr;
	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> Geometries;
	ComPtr<ID3D12DescriptorHeap> CBVHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> SRVDescriptorHeap = nullptr;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> Shaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> PSOs;
	std::vector<D3D12_INPUT_ELEMENT_DESC> InputLayout;

	std::vector<std::unique_ptr<RenderItem>> AllRitems;
	std::vector<RenderItem*> OpaqueRitems;

	UINT PassCbvOffset = 0;
};
