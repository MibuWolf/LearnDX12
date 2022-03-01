#pragma once
#include "Waves.h"
#include "Base/RenderPass.h"


enum class RenderLayer : int
{
	Opaque = 0,
	Count
};

class WavesRenderPass : public RenderPass
{
public:

	WavesRenderPass();

	~WavesRenderPass() override;

public:

	void		Initialize() override;

	void		Tick(const SystemTimer& Timer, const XMFLOAT4X4& View, const XMFLOAT4X4& Proj, const XMFLOAT3& EyePos) override;

	void		Draw(const SystemTimer& Timer) override;

protected:

	void		BuildRootSignature() override;
	void		BuildShapeGeometry() override;
	void		BuildRenderItems() override;

	void		BuildLandGeometry();
	void		BuildWavesGeometryBuffers();
	void		BuildFrameResources();

	void		UpdateWaves(const SystemTimer& Timer);

	void		DrawRenderItems(const SystemTimer& Timer) override;

	float GetHillsHeight(float x, float z)const;
	XMFLOAT3 GetHillsNormal(float x, float z)const;

private:

	std::unique_ptr<Waves> mWaves;
	RenderItem* mWavesRitem = nullptr;
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];
};
