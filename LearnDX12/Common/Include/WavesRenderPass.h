#pragma once
#include "Waves.h"
#include "Base/RenderPass.h"


class WavesRenderPass : public RenderPass
{
public:

	WavesRenderPass();

	~WavesRenderPass() override;

public:

	void		Initialize() override;


protected:

	void		BuildRootSignature() override;

private:

	std::unique_ptr<Waves> mWaves;

};