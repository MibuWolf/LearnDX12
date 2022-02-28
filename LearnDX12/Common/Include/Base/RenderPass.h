#pragma once
#include "Geometry.h"
#include "FrameResource.h"
#include "SystemTimer.h"

class RenderPass
{
public:

	RenderPass();

	~RenderPass();

public:

	void		Initialize();

	ID3D12PipelineState* GetRenderPassPSO();

	void		Tick(const SystemTimer& Timer, const XMFLOAT4X4& View, const XMFLOAT4X4& Proj, const XMFLOAT3& EyePos);

	void		Draw(const SystemTimer& Timer);

protected:

	void		BuildRootSignature();
	void		BuildShadersAndInputLayout();
	void		BuildShapeGeometry();
	void		BuildRenderItems();
	void		BuildDescriptorHeaps();
	void		BuildConstantBufferViews();
	void		BuildPSOs();

	void		TickRenderPass(const SystemTimer& Timer, const XMFLOAT4X4& View, const XMFLOAT4X4& Proj, const XMFLOAT3& EyePos);
	void		TickRenderItems(const SystemTimer& Timer);

	void		DrawRenderItems(const SystemTimer& Timer);

private:

	ComPtr<ID3D12RootSignature> RootSignature = nullptr;
	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> Geometries;
	ComPtr<ID3D12DescriptorHeap> CBVHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> SRVDescriptorHeap = nullptr;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> Shaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> PSOs;
	std::vector<D3D12_INPUT_ELEMENT_DESC> InputLayout;

	PassConstants MainPassCB;
	UINT PassCBVOffset = 0;
	bool IsWireframe = false;

	std::vector<std::unique_ptr<RenderItem>> AllRItems;
	std::vector<RenderItem*> OpaqueRitems;

	UINT PassCbvOffset = 0;
};
